
#ifndef GPIO_H
    #define GPIO_H

#include "inputs.h"
#include "led.h"

void init_GPIO(void);

void read_GPIO_input_pins(input_state_t *inputs);
void decode_gpio_input_pins(input_state_t *inputs, uint32_t gpiof, uint32_t gpioe);

void write_GPIO_leds(const led_output_t *leds);


#endif
