#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/uart.h"



#include "freq_counter.pio.h"

#include "tft.h"

#define FREQ_PIN 15
#define WINDOW_MS 100

uint pulsecount;
uint FRQ = 1;

uint T;

int main()
{

    stdio_init_all();

    tft_init();
    tft_fill(0);

    while (true)
    {

        tft_swap_sync();
        tft_draw_string(1,1,100,"TEST");
    }
}
