#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "freq_counter.pio.h"
#include "tft.h"

#define FRQPIN 15
#define WINDOW_MS 10

uint T2 = 0;
uint T1 = 0;
uint T = 0;
uint F = 0;
uint Smartdelay;

uint AcPins[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
uint32_t maskADir = (1u << 0) | (1u << 3) | (1u << 4) | (1u << 7) | (1u << 8) | (1u << 11);
uint32_t maskASpeed = (1u << 1) | (1u << 2) | (1u << 5) | (1u << 6) | (1u << 9) | (1u << 10); // Actuator GPIO speed mask
bool Actdir = 0;
uint StpPins[2] = {12, 13};

void graphfreq(uint16_t x, uint16_t y, uint16_t color, uint16_t freq)
{
    uint32_t period = 1000000UL / freq; // µs
    uint32_t halfT = period / 2;

    bool plot[128];

    bool state = 1;
    uint16_t index = 0;

    while (index < 128)
    {
        for (uint32_t i = 0; i < halfT && index < 128; i++)
        {
            plot[index++] = state;
        }

        state = !state;
    }

    for (index = 0; index < 128; index++)
    {
        if (plot[index])
        {
            tft_draw_pixel(x + index, y, color);
        }
        else
        {
            for (uint32_t i = y + 20; i != y; i--)
            {
                tft_draw_pixel(x + index, i - y + 20, color);
            }
            while (!plot[index])
            {
                tft_draw_pixel(x + index, y - 20, color);
                index++;
            }
            for (uint32_t i = y + 20; i != y; i--)
            {
                tft_draw_pixel(x + index, i - y + 20, color);
            }
        }
    }
}

int main()
{

    stdio_init_all();

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &freq_counter_program);
    uint sm = pio_claim_unused_sm(pio, true);
    freq_counter_program_init(pio, sm, offset, FRQPIN);

    tft_init();
    tft_fill(0);

    for (uint32_t i = 0; i < 12; i++)
    {
        gpio_init(AcPins[i]);
        gpio_set_dir(AcPins[i], 1);
    }
    for (uint32_t i = 0; i < 2; i++)
    {
        gpio_init(StpPins[i]);
        gpio_set_dir(StpPins[i], 1);
    }

    gpio_set_mask64(maskASpeed); //Full duty cycle PWM pins on TB6612
    Smartdelay = to_ms_since_boot(get_absolute_time());

    while (true)
    {

        T1 = pio_sm_get_blocking(pio, sm);
        sleep_ms(10);
        T2 = pio_sm_get_blocking(pio, sm);
        T = T1 - T2;
        F = T / 0.01;
        printf("Frequency : %u\n", F);

        tft_swap_sync();

        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", F);
        tft_fill(0);
        tft_draw_string(20, 1, 100, buffer);
        graphfreq(0, 40, 100, F);

        if(Actdir){
            tft_draw_string(0,80,100,"Extending");
        }else{
            tft_draw_string(0,80,100,"Retracting");
        }

        // Swap actuator directions every 10 seconds (nonblocking)
        if(Smartdelay + 10000 < to_ms_since_boot(get_absolute_time()))
        {
            if (Actdir)
            {
                gpio_clr_mask64(maskADir);
                Actdir = 0;
            }
            else
            {
                gpio_set_mask64(maskADir);
                Actdir = 1;
            }
            Smartdelay = to_ms_since_boot(get_absolute_time());
        }
    }
}
