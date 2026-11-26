/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   GPIO driver: button inputs and status LED outputs.
  *
  *          This file provides functions to:
  *           + Configure the button and LED pins
  *           + Read and decode the button pins
  *           + Drive the LEDs
  ******************************************************************************
  *
  * Buttons: 12 inputs on ports E (D-pad) and F, internal pull-ups, active low (pressed = 0).
  * Pin table: docs/mcu/drivers/gpio.md. read_GPIO_input_pins() samples both IDRs, and
  * decode_gpio_input_pins() turns them into btn_t bits. The decode is pure and tested on the host
  * (tests/test_inputs.c). Debouncing is done in inputs.c.
  *
  * LEDs: Nucleo LD1 green PC7, LD3 red PG2, LD2 blue PB7 (UM2861 7.5). Push-pull outputs, active
  * high. PG2 is on the VDDIO2 supply, so init_GPIO() sets PWR_SVMCR.IO2SV before using it.
  * write_GPIO_leds() applies a led_output_t, one BSRR write per LED. What the LEDs show is decided
  * in led.c.
  ******************************************************************************
  */

#include "stm32u575_regs.h"
#include "gpio.h"
#include "led.h"

#include <stdint.h>


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

void read_GPIO_input_pins(input_state_t *inputs)
{
    const uint32_t gpiof = GPIOF_IDR;
    const uint32_t gpioe = GPIOE_IDR;

    decode_gpio_input_pins(inputs, gpiof, gpioe);
}

static void set_GPIO_pins(void)
{

    // Sets specific GPIO (General Purpose I/O) pins to input mod -> page 111 of the Datasheet in ../../../docs/hardware/DS_stm32u575cg.pdf
    // Configure all button GPIOs as inputs with internal pull-ups.
    //
    // Active-low:
    // released -> 1
    // pressed  -> 0

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

static void set_GPIO_led_pins(void)
{
    /*
     * User LEDs (UM2861 7.5), push-pull outputs, active high (driven through a transistor)
     *
     * PC7 -> green, charging (LD1)
     * PG2 -> red, battery (LD3)
     * PB7 -> blue, fault (LD2)
     *
     * MODER resets to analog (11) on most pins: clear before setting output (01).
     * ODR resets to 0, so every LED starts off.
     */

    GPIOC_MODER = (GPIOC_MODER & ~GPIO_2BIT_MASK(LED_GREEN_PIN)) | GPIO_MODE_OUTPUT(LED_GREEN_PIN);
    GPIOG_MODER = (GPIOG_MODER & ~GPIO_2BIT_MASK(LED_RED_PIN)) | GPIO_MODE_OUTPUT(LED_RED_PIN);
    GPIOB_MODER = (GPIOB_MODER & ~GPIO_2BIT_MASK(LED_BLUE_PIN)) | GPIO_MODE_OUTPUT(LED_BLUE_PIN);
}

static void enable_GPIO_clocks(void)
{
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOFEN;   // Buttons
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOEEN;   // D-pad
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOBEN;   // Blue LED
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOCEN;   // Green LED
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOGEN;   // Red LED
}

// PG[15:2] are on the VDDIO2 supply and stay isolated until IO2SV is set (RM0456 PWR_SVMCR).
// No voltage monitor check: on the Nucleo VDDIO2 follows VDD_MCU (SB28 ON, UM2861 7.4.5 Table 9).
static void enable_vddio2(void)
{
    RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;
    PWR_SVMCR |= PWR_SVMCR_IO2SV;
}

void init_GPIO(void)
{
    enable_GPIO_clocks();
    enable_vddio2();
    set_GPIO_pins();
    set_GPIO_led_pins();
}

void write_GPIO_leds(const led_output_t *leds)
{
    // BSRR sets or resets one pin per write, no read-modify-write
    GPIOC_BSRR = leds->charging_led ? GPIO_BSRR_SET(LED_GREEN_PIN) : GPIO_BSRR_RESET(LED_GREEN_PIN);
    GPIOG_BSRR = leds->battery_led  ? GPIO_BSRR_SET(LED_RED_PIN)   : GPIO_BSRR_RESET(LED_RED_PIN);
    GPIOB_BSRR = leds->fault_led    ? GPIO_BSRR_SET(LED_BLUE_PIN)  : GPIO_BSRR_RESET(LED_BLUE_PIN);
}
