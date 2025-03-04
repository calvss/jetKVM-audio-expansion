// Author: Calvin Ng 2025

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/dma.h"

#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1
#define BUFFER_SIZE 16384
#define BUFFER_SIZE_BYTES BUFFER_SIZE*2 // 16-bits per array element
#define CAPTURE_RING_BITS 15 // Number of bits to store the address of 16384*2 bytes

#define UART_TX_PIN 16
#define UART_RX_PIN 17

// need to 0-align the buffer to make it work with the pico's circular DMA
static uint16_t buffer1[BUFFER_SIZE] __attribute__((aligned(BUFFER_SIZE_BYTES)));
static uint16_t buffer2[BUFFER_SIZE] __attribute__((aligned(BUFFER_SIZE_BYTES)));

static bool buffer1_is_ready = false;
static bool buffer2_is_ready = false;

static uint32_t irq_counter1 = 0;
static uint32_t irq_counter2 = 0;
static uint32_t irq_counter3 = 0;
static uint8_t loop_overspeed_counter = 0;

unsigned int dma_channel_1;
unsigned int dma_channel_2;

void dma_handler();

int main()
{
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    sleep_ms(1000);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    sleep_ms(5000);
    // printf("Start up\n");
    uart_init(uart0, 3'000'000);

    // disable fifo buffering since we want to send bytes as fast as possible
    uart_set_fifo_enabled(uart0, false);

    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(uart0, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(uart0, UART_RX_PIN));

    adc_gpio_init(26 + ADC_CHANNEL_0);
    adc_gpio_init(26 + ADC_CHANNEL_1);

    adc_init();

    // switch channel to 0 explicitly so the round robin always runs 0-1-0-1... instead of 1-0-1-0...
    adc_select_input(ADC_CHANNEL_0);

    // bit mask for first 2 ADC channels
    adc_set_round_robin(0b00000011);

    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // Disable the ERR bit
        false    // Don't shift each sample to 8 bits when pushing to FIFO
    );

    // set ADC clock to 96 kHz (499 + 1 clock cycles at 48MHz = 10.416667 microseconds)
    // 96 kHz = 48kHz sample rate for each audio channel
    adc_set_clkdiv(499);
    // adc_set_clkdiv(2999);

    // obtain a DMA, function call will panic if there's no DMAs available
    dma_channel_1 = dma_claim_unused_channel(true);
    dma_channel_2 = dma_claim_unused_channel(true);

    {
        dma_channel_config cfg1 = dma_channel_get_default_config(dma_channel_1);
        // Reading from constant address, writing to incrementing byte addresses
        channel_config_set_transfer_data_size(&cfg1, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg1, false); // read from ADC FIFO
        channel_config_set_write_increment(&cfg1, true); // write to RAM array buffer

        // Wrap back to beginning of buffer (buffer must be 0-aligned)
        channel_config_set_ring(&cfg1, true, CAPTURE_RING_BITS);

        // Pace transfers based on availability of ADC samples
        channel_config_set_dreq(&cfg1, DREQ_ADC);

        // when this DMA finishes, start the other one
        channel_config_set_chain_to(&cfg1, dma_channel_2);

        // interrupt when done
        dma_channel_set_irq0_enabled(dma_channel_1, true);

        dma_channel_configure(dma_channel_1, &cfg1,
            buffer1,         // dst
            &adc_hw->fifo,  // src
            BUFFER_SIZE,  // transfer count
            true            // start immediately
        );

    }

    {
        dma_channel_config cfg2 = dma_channel_get_default_config(dma_channel_2);
        // Reading from constant address, writing to incrementing byte addresses
        channel_config_set_transfer_data_size(&cfg2, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg2, false); // read from ADC FIFO
        channel_config_set_write_increment(&cfg2, true); // write to RAM array buffer

        // Wrap back to beginning of buffer (buffer must be 0-aligned)
        channel_config_set_ring(&cfg2, true, CAPTURE_RING_BITS);

        // Pace transfers based on availability of ADC samples
        channel_config_set_dreq(&cfg2, DREQ_ADC);

        // when this DMA finishes, start the other one
        channel_config_set_chain_to(&cfg2, dma_channel_1);

        // interrupt when done
        dma_channel_set_irq0_enabled(dma_channel_2, true);

        dma_channel_configure(dma_channel_2, &cfg2,
            buffer2,         // dst
            &adc_hw->fifo,  // src
            BUFFER_SIZE,  // transfer count
            false            // do not start immediately, channel 1 will trigger this when done
        );
    }

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);

    // enable irq
    irq_set_mask_enabled(1 << DMA_IRQ_0, true);
    adc_run(true);

    while(true)
    {
        // each iteration of this loop processes one audio packet
        // BUFFER_SIZE samples per packet, with a 0xffff stop byte at the end

        uint16_t *buffer;

        // possible off-by-one error? not sure
        uint8_t char_buffer[BUFFER_SIZE_BYTES + 1];
        uint8_t stop_bytes[2];
        stop_bytes[0] = 0xff;
        stop_bytes[1] = 0xff;

        if(buffer1_is_ready)
        {
            buffer = buffer1;
            buffer1_is_ready = false;
        }
        else if(buffer2_is_ready)
        {
            buffer = buffer2;
            buffer2_is_ready = false;
        }
        else
        {
            loop_overspeed_counter++;

            // don't go on to process the packet if there's no new data
            continue;
        }

        // convert uint16_t buffer into a uint8 array (little endian)
        // need to send chars through UART, not int16
        for(int i = 0, j = 0; i < BUFFER_SIZE; i++, j+=2)
        {
            uint8_t lower_byte;
            uint8_t upper_byte;
            
            // reserve 0xffff for the end-of-packet marker
            // this might cause some audio artifacts
            // but if you're hitting 0xffff regularly you have bigger problems (clipping)
            if(buffer[i] == 0xffff)
            {
                lower_byte = 0xfe;
                upper_byte = 0xff;
            }
            else
            {
                lower_byte = buffer[i] & 0xff;
                upper_byte = buffer[i] >> 8;
            }

            char_buffer[j] = lower_byte;
            char_buffer[j+1] = upper_byte;
        }

        uart_write_blocking(uart0, char_buffer, BUFFER_SIZE_BYTES);
        uart_write_blocking(uart0, stop_bytes, 2);
    }
}

void dma_handler()
{
    // DMA chan 1:
    if (dma_hw->ints0 & 1u << dma_channel_1)
    {
        irq_counter1++;
        buffer1_is_ready = true;

        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_1;
    }
    // else DMA chan 2:
    else if (dma_hw->ints0 & 1u << dma_channel_2)
    {
        irq_counter2++;
        buffer2_is_ready = true;

        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_2;
    }
    else
    {
        // Should never happen
        irq_counter3++;
    }
}