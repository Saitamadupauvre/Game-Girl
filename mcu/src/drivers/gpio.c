
#include "stm32u575_regs.h"
#include "gpio.h"

#include <stdint.h>

// Enables the GPIO pins for the F port -> page 556 of the reference manual

void enable_GPIO_clocks(void)
{
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOFEN;   // Sets bit 5 to ENABLED GPIOF CLOCK(1)
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOEEN;   // ENABLES GPIOE CLOCK 
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

    const uint32_t gpiof_mode_mask =
        GPIO_2BIT_MASK(3)  | GPIO_2BIT_MASK(4)  | GPIO_2BIT_MASK(5)  |
        GPIO_2BIT_MASK(10) | GPIO_2BIT_MASK(11) | GPIO_2BIT_MASK(13) |
        GPIO_2BIT_MASK(14) | GPIO_2BIT_MASK(15);
    const uint32_t gpiof_pullup_mask =
        GPIO_PULL_UP(3)  | GPIO_PULL_UP(4)  | GPIO_PULL_UP(5)  |
        GPIO_PULL_UP(10) | GPIO_PULL_UP(11) | GPIO_PULL_UP(13) |
        GPIO_PULL_UP(14) | GPIO_PULL_UP(15);

    GPIOF_MODER &= ~gpiof_mode_mask;
    GPIOF_PUPDR = (GPIOF_PUPDR & ~gpiof_mode_mask) | gpiof_pullup_mask;

    /*
     * GPIOE
     *
     * PE7  -> Up
     * PE8  -> Down
     * PE9  -> Left
     * PE11 -> Right
     */

    const uint32_t gpioe_mode_mask =
        GPIO_2BIT_MASK(7) | GPIO_2BIT_MASK(8) |
        GPIO_2BIT_MASK(9) | GPIO_2BIT_MASK(11);
    const uint32_t gpioe_pullup_mask =
        GPIO_PULL_UP(7) | GPIO_PULL_UP(8) |
        GPIO_PULL_UP(9) | GPIO_PULL_UP(11);

    GPIOE_MODER &= ~gpioe_mode_mask;
    GPIOE_PUPDR = (GPIOE_PUPDR & ~gpioe_mode_mask) | gpioe_pullup_mask;
}

void read_GPIO_input_pins(input_state_t *inputs)
{
    const uint32_t gpiof = GPIOF_IDR;
    const uint32_t gpioe = GPIOE_IDR;

    decode_gpio_input_pins(inputs, gpiof, gpioe);
}

void decode_gpio_input_pins(input_state_t *inputs, uint32_t gpiof, uint32_t gpioe)
{
    uint16_t raw = 0;

    if ((gpiof & GPIO_IDR_PIN_BIT(13)) == 0u) {
        raw |= BTN_A;
    }

    if ((gpiof & GPIO_IDR_PIN_BIT(14)) == 0u) {
        raw |= BTN_B;
    }

    if ((gpiof & GPIO_IDR_PIN_BIT(15)) == 0u) {
        raw |= BTN_X;
    }

    if ((gpiof & GPIO_IDR_PIN_BIT(11)) == 0u) {
        raw |= BTN_Y;
    }

    if ((gpiof & GPIO_IDR_PIN_BIT(3)) == 0u) {
        raw |= BTN_L;
    }

    if ((gpiof & GPIO_IDR_PIN_BIT(4)) == 0u) {
        raw |= BTN_R;
    }

    if ((gpiof & GPIO_IDR_PIN_BIT(5)) == 0u) {
        raw |= BTN_START;
    }

    if ((gpiof & GPIO_IDR_PIN_BIT(10)) == 0u) {
        raw |= BTN_SELECT;
    }

    if ((gpioe & GPIO_IDR_PIN_BIT(7)) == 0u) {
        raw |= BTN_UP;
    }

    if ((gpioe & GPIO_IDR_PIN_BIT(8)) == 0u) {
        raw |= BTN_DOWN;
    }

    if ((gpioe & GPIO_IDR_PIN_BIT(9)) == 0u) {
        raw |= BTN_LEFT;
    }

    if ((gpioe & GPIO_IDR_PIN_BIT(11)) == 0u) {
        raw |= BTN_RIGHT;
    }

    inputs->raw = raw;
}
