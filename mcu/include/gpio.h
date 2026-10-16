
#ifndef GPIO_H
    #define GPIO_H

#include "inputs.h"

void init_GPIO(void);

void read_GPIO_input_pins(input_state_t *inputs);
void decode_gpio_input_pins(input_state_t *inputs, uint32_t gpiof, uint32_t gpioe);


#endif
