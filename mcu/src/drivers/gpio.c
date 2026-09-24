
#include "../../include/stm32u575_regs.h"

#include <stdint.h>

// Enables the GPIO pins for the F port -> page 556 of the reference manual

void enable_GPIOF_clock(void)
{
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOFEN;   // Sets bit 5 to ENABLED GPIOF CLOCK(1)
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;   // ENABLES GPIOE CLOCK 
}

// Sets specific GPIO (General Purpose I/O) pins to input mod -> page 111 of the Datasheet in ../../../docs/hardware/DS_stm32u575cg.pdf
// Configure all button GPIOs as inputs with internal pull-ups.
//
// Active-low:
// released -> 1
// pressed  -> 0

void set_GPIO_pins(void)
{
    /*
     * GPIOF
     *
     * PF3  -> L
     * PF4  -> R
     * PF5  -> Start
     * PF10 -> Select
     * PF11 -> Y
     * PF13 -> A
     * PF14 -> B
     * PF15 -> X
     */

    GPIOF_MODER &= ~GPIO_2BIT_MASK(3);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(3);
    GPIOF_PUPDR |= GPIO_PULL_UP(3);

    GPIOF_MODER &= ~GPIO_2BIT_MASK(4);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(4);
    GPIOF_PUPDR |= GPIO_PULL_UP(4);

    GPIOF_MODER &= ~GPIO_2BIT_MASK(5);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(5);
    GPIOF_PUPDR |= GPIO_PULL_UP(5);

    GPIOF_MODER &= ~GPIO_2BIT_MASK(10);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(10);
    GPIOF_PUPDR |= GPIO_PULL_UP(10);

    GPIOF_MODER &= ~GPIO_2BIT_MASK(11);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(11);
    GPIOF_PUPDR |= GPIO_PULL_UP(11);

    GPIOF_MODER &= ~GPIO_2BIT_MASK(13);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(13);
    GPIOF_PUPDR |= GPIO_PULL_UP(13);

    GPIOF_MODER &= ~GPIO_2BIT_MASK(14);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(14);
    GPIOF_PUPDR |= GPIO_PULL_UP(14);

    GPIOF_MODER &= ~GPIO_2BIT_MASK(15);
    GPIOF_PUPDR &= ~GPIO_2BIT_MASK(15);
    GPIOF_PUPDR |= GPIO_PULL_UP(15);

    /*
     * GPIOE
     *
     * PE7  -> Up
     * PE8  -> Down
     * PE9  -> Left
     * PE11 -> Right
     */

    GPIOE_MODER &= ~GPIO_2BIT_MASK(7);
    GPIOE_PUPDR &= ~GPIO_2BIT_MASK(7);
    GPIOE_PUPDR |= GPIO_PULL_UP(7);

    GPIOE_MODER &= ~GPIO_2BIT_MASK(8);
    GPIOE_PUPDR &= ~GPIO_2BIT_MASK(8);
    GPIOE_PUPDR |= GPIO_PULL_UP(8);

    GPIOE_MODER &= ~GPIO_2BIT_MASK(9);
    GPIOE_PUPDR &= ~GPIO_2BIT_MASK(9);
    GPIOE_PUPDR |= GPIO_PULL_UP(9);

    GPIOE_MODER &= ~GPIO_2BIT_MASK(11);
    GPIOE_PUPDR &= ~GPIO_2BIT_MASK(11);
    GPIOE_PUPDR |= GPIO_PULL_UP(11);
}

void read_GPIO_input_pints(void)
{

}
