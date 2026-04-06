#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"

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

void graphfreq(uint16_t x, uint16_t y, uint16_t color, uint32_t freq)
{
    const uint16_t DISPLAY_WIDTH = 128;
    const uint32_t MIN_FREQ = 4100000;
    const uint32_t MAX_FREQ = 4400000;

    // Map freq directly to halfT in pixels (inverse: higher freq = shorter halfT)
    // At MIN_FREQ -> halfT = HALF_T_MAX (stretched out, few cycles)
    // At MAX_FREQ -> halfT = HALF_T_MIN (compressed, more cycles)
    const uint16_t HALF_T_MAX = 32; // pixels per half-cycle at MIN_FREQ
    const uint16_t HALF_T_MIN = 4;  // pixels per half-cycle at MAX_FREQ

    uint16_t halfT = HALF_T_MAX - (uint16_t)((uint32_t)(freq - MIN_FREQ) * (HALF_T_MAX - HALF_T_MIN) / (MAX_FREQ - MIN_FREQ));
    if (halfT < HALF_T_MIN)
        halfT = HALF_T_MIN;
    if (halfT > HALF_T_MAX)
        halfT = HALF_T_MAX;

    // Build waveform sample array
    bool plot[DISPLAY_WIDTH];
    bool state = true;
    uint16_t index = 0;

    while (index < DISPLAY_WIDTH)
    {
        for (uint16_t i = 0; i < halfT && index < DISPLAY_WIDTH; i++)
            plot[index++] = state;
        state = !state;
    }

    // Draw waveform
    for (index = 0; index < DISPLAY_WIDTH; index++)
    {
        bool rising = (index > 0 && !plot[index - 1] && plot[index]);
        bool falling = (index > 0 && plot[index - 1] && !plot[index]);

        if (rising || falling)
        {
            for (uint16_t v = 0; v <= 20; v++)
                tft_draw_pixel(x + index, y - v, color);
        }

        uint16_t draw_y = plot[index] ? y : y - 20;
        tft_draw_pixel(x + index, draw_y, color);
    }
}

void Drive_stepper(uint16_t spdpin, uint16_t dirpin, uint16_t spdrpm, uint16_t dir){
    //Nema 17 - 200 steps for a full revolution
    //1 rps = 200Hz // 1rpm = 12khz
    //maxspeed 2k rpm -> 
    gpio_put(dirpin,dir);

    gpio_set_function(spdpin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(spdpin);

    // 1 kHz PWM @ 150 MHz sysclk
    pwm_set_clkdiv(slice, 100.0f);
    pwm_set_wrap(slice, 14999);

    // 50% duty cycle
    pwm_set_gpio_level(spdpin, 7500);

    pwm_set_enabled(slice, true);



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
    Drive_stepper(StpPins[0],StpPins[1],1,1);

    gpio_set_mask64(maskASpeed); // Full duty cycle PWM pins on TB6612
    Smartdelay = to_ms_since_boot(get_absolute_time());

    while (true)
    {

        T1 = pio_sm_get_blocking(pio, sm);
        sleep_ms(10);
        T2 = pio_sm_get_blocking(pio, sm);
        T = T1 - T2;
        F = T / 0.01;
        // printf("Frequency : %u\n", F);

        tft_swap_sync();

        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%d", F);
        tft_fill(0);
        tft_draw_string(20, 1, 100, buffer);
        graphfreq(0, 40, 100, F);
        char str[32];
        snprintf(buffer, sizeof(buffer), "%d", Smartdelay + 10000 - to_ms_since_boot(get_absolute_time()));
        if (Actdir)
        {
            snprintf(str, sizeof(str), "%s%s", "Out ", buffer);
        }
        else
        { 
            snprintf(str, sizeof(str), "%s%s", "In ", buffer);
        }
        tft_draw_string(0, 50, 100, str);

        // Swap actuator directions every 10 seconds (nonblocking)
        if (Smartdelay + 10000 < to_ms_since_boot(get_absolute_time()))
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
