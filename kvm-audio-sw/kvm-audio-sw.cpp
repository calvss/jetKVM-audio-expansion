// Author: Calvin Ng 2025

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/dma.h"

#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1
#define CAPTURE_DEPTH 200'000

volatile uint16_t buffer[CAPTURE_DEPTH];

int main()
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    sleep_ms(3000);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    printf("Startup\n");
    sleep_ms(10000);
    uart_init(uart0, 3'000'000);

    adc_gpio_init(26 + ADC_CHANNEL_0);
    adc_gpio_init(26 + ADC_CHANNEL_1);

    adc_init();

    // bit mask for first 2 ADC channels
    adc_set_round_robin(0b00000011);

    // switch channel to 0 explicitly so the round robin always runs 0-1-0-1... instead of 1-0-1-0...
    adc_select_input(ADC_CHANNEL_0);

    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // Disable the ERR bit
        false    // Don't shift each sample to 8 bits when pushing to FIFO
    );

    // adc_set_clkdiv(0);
    // set clock to 40 kHz (1199 + 1 clock cycles at 48MHz = 25 microseconds)
    adc_set_clkdiv(599);

    unsigned int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_channel);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(dma_channel, &cfg,
        buffer,         // dst
        &adc_hw->fifo,  // src
        CAPTURE_DEPTH,  // transfer count
        true            // start immediately
    );

    sleep_ms(1000);
    printf("Starting capture\n");

    adc_run(true);

    dma_channel_wait_for_finish_blocking(dma_channel);
    printf("Capture finished\n");
    adc_run(false);
    adc_fifo_drain();

    // Print samples to stdout so you can display them in pyplot, excel, matlab
    for (int i = 0, j = 0; i < CAPTURE_DEPTH; i += 2, j++) {
        printf("%d, %d, %d\n", j, buffer[i], buffer[i+1]);
    }

    return 0;
}