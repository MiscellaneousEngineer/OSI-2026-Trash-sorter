#pragma once
#include "pico/stdlib.h"

// speed: 0.0 (slow) to 1.0 (fast)
// dir_pin/step_pin: GPIO numbers

static inline void setup_stepper(uint step_pin, uint dir_pin){
    gpio_init(step_pin); 
    gpio_set_dir(step_pin, GPIO_OUT);
    gpio_init(dir_pin);  
    gpio_set_dir(dir_pin,  GPIO_OUT);
}

void drive_stepper(uint32_t steps, float speed, uint step_pin, uint dir_pin, bool direction) {
    uint32_t delay_us = (uint32_t)((1.0f - speed) * 9900) + 10; // 10us–10ms per step

        gpio_put(dir_pin, direction);

    for (uint32_t i = 0; i < steps; i++) {
        gpio_put(step_pin, 1);
        sleep_us(10); // A4988 minimum 1µs pulse width
        gpio_put(step_pin, 0);
        sleep_us(10);
    }
}