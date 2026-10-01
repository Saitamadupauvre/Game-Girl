
#ifndef GPIO_H
    #define GPIO_H

#include "inputs.h"

void enable_GPIO_clocks(void);
void set_GPIO_pins(void);
void read_GPIO_input_pins(input_state_t *inputs);
void decode_gpio_input_pins(input_state_t *inputs, uint32_t gpiof, uint32_t gpioe);


#endif
