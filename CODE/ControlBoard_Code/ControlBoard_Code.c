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

void graphfreq(uint16_t x, uint16_t y, uint16_t x color,uint16_t freq){
uint localT;
localT=1/freq;
int plot[];
bool state = 1;
    
if(freq>1000000)//MHZ operation
{//screen witdh is equal to 128 µS in MHZ mode
localT = localT * 1000000; //convert from S to µs
for(i = 127,i != 0, i--){

    for(i = localT,i !=0,i--)
    {
        
    }

}

    
}elif(freq>1000)//KHZ mode
{

}else//HZ mode
{
    
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
    }
}
