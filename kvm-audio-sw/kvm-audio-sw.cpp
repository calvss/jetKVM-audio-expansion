// Author: Calvin Ng 2025

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/dma.h"

#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1
#define BUFFER_SIZE 1024
#define BUFFER_SIZE_BYTES BUFFER_SIZE*2 // 16-bits per array element
#define CAPTURE_RING_BITS 11 // Number of bits to store the address of 1024*2 bytes

static uint16_t buffer1[BUFFER_SIZE] __attribute__((aligned(BUFFER_SIZE_BYTES)));
static uint16_t buffer2[BUFFER_SIZE] __attribute__((aligned(BUFFER_SIZE_BYTES)));

static uint32_t irq_counter1 = 0;
static uint32_t irq_counter2 = 0;
static uint32_t irq_counter3 = 0;

unsigned int dma_channel_1;
unsigned int dma_channel_2;

repeating_timer_t timer;
bool print = false;

uint16_t alternateSum(uint16_t *array, size_t n);
void dma_handler();
bool timer_callback(repeating_timer_t *rt);

int main()
{
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    sleep_ms(1000);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    sleep_ms(5000);
    printf("Start up\n");
    uart_init(uart0, 3'000'000);

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
    adc_set_clkdiv(499);
    // adc_set_clkdiv(2999);

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
            true            // do not start immediately
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
            false            // do not start immediately
        );
    }

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);

    // enable irq
    irq_set_mask_enabled(1 << DMA_IRQ_0, true);
    adc_run(true);

    if (!add_repeating_timer_us(1000000, timer_callback, NULL, &timer)) {
        printf("Failed to add timer\n");
        return 1;
    }

    while(true)
    {
        if(print)
        {    
            printf("1: %5lu, 2: %5lu, 3: %5lu\n", irq_counter1, irq_counter2, irq_counter3);
            print = false;
        }
    }
}

/*! \brief Sum of every other element of an array.
 * \return Sums every other element to handle interleaved left-right ADC samples.
 */
uint16_t alternateSum(uint16_t *array, size_t n)
{
    uint16_t total = 0;
    for(int i = 0, j = 0; i < n; i++, j += 2)
    {
        total += array[j];
    }
    return total;
}

void dma_handler()
{
    // DMA chan 1:
    if (dma_hw->ints0 & 1u << dma_channel_1)
    {
        irq_counter1++;
        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_1;
    }
    // else DMA chan 2:
    else if (dma_hw->ints0 & 1u << dma_channel_2)
    {
        irq_counter2++;
        // Clear the interrupt request.
        dma_hw->ints0 = 1u << dma_channel_2;
    }
    else
    {
        // Should never happen
        irq_counter3++;
    }
}

bool timer_callback(repeating_timer_t *rt)
{
    print = true;
    return true;
}