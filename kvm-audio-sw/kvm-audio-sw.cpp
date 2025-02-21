// Author: Calvin Ng 2025

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/uart.h"

#define ADC_CHANNEL 0

volatile uint16_t buffer[UINT8_MAX+1];
volatile bool readFlag = false;
uint8_t i = 0;
uint32_t count = 0;

bool timer_callback(repeating_timer_t *rt);

int main()
{
    stdio_init_all();

    int speed = uart_init(uart0, 3000000);

    repeating_timer_t timer;

    if (!add_repeating_timer_us(-1000000, timer_callback, NULL, &timer)) {
        printf("Failed to add timer\n");
        return 1;
    }



    adc_init();

    adc_gpio_init(26 + ADC_CHANNEL);
    adc_select_input(ADC_CHANNEL);

    while (1) {
        buffer[i] = adc_read();
        i++;
        count++;

        if(readFlag)
        {
            printf("%lu vscode!\n", count);
            count = 0;
            readFlag = false;
        }
    }

    return 0;
}

bool timer_callback(__unused repeating_timer_t* rt)
{
    readFlag = true;
    return true;
}

