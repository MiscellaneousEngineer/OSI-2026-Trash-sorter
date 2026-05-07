#pragma once
#include "pico/stdlib.h"

// speed: 0.0 (slow) to 1.0 (fast)
// dir_pin/step_pin: GPIO numbers

static inline void setup_stepper(uint step_pin, uint dir_pin){
    gpio_init(step_pin); 
    gpio_set_dir(step_pin, GPIO_OUT);
    gpio_init(dir_pin);  
    gpio_set_dir(dir_pin,  GPIO_OUT);
    sleep_ms(100);
}


//nonblocking stepper motor driving function
void drive_stepper(uint32_t steps, float speed, uint step_pin, uint dir_pin, bool direction) {

        gpio_put(dir_pin, direction);

    for (uint32_t i = 0; i < steps; i++) {

        gpio_put(step_pin, 1);
        sleep_us(speed * 1000);
        gpio_put(step_pin, 0);
        sleep_us(500);
    }
}

uint32_t bin(uint16_t value){
    uint32_t steps = value * 1050;
    return steps;
}