#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "freq_counter.pio.h"

#define INPUT_PIN 15

int main()
{
    stdio_init_all();

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &freq_counter_program);
    uint sm = pio_claim_unused_sm(pio, true);

    pio_sm_config c = freq_counter_program_get_default_config(offset);

    sm_config_set_in_pins(&c, INPUT_PIN);
    pio_sm_set_consecutive_pindirs(pio, sm, INPUT_PIN, 1, false);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_gpio_init(pio, INPUT_PIN);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);


    while (true)
    {
        uint32_t count = pio_sm_get_blocking(pio, sm);

        printf("Edge batch received: %u\n", count);
    }
}