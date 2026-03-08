#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "freq_counter.pio.h"

#define FRQPIN 15

uint T2 = 0;
uint T1 = 0;
uint T = 0;
uint F = 0;
int main()
{
    stdio_init_all();


    PIO pio = pio0;
    uint offset = pio_add_program(pio, &freq_counter_program);
    uint sm = pio_claim_unused_sm(pio, true);
    freq_counter_program_init(pio, sm, offset, FRQPIN);
    pio->txf[0] = 1000;

    while (true)
    {
        T1 = pio_sm_get_blocking(pio, sm);
        sleep_ms(10);
        T2 = pio_sm_get_blocking(pio, sm);
        T = T1-T2;
        F = T/0.01;
        printf("Frequency : %u\n", F);

        
    }
}