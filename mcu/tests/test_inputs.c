#include "gpio.h"
#include "inputs.h"
#include "stm32u575_regs.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK_EQUAL(actual, expected) do { \
    const uint16_t actual_value = (actual); \
    const uint16_t expected_value = (expected); \
    if (actual_value != expected_value) { \
        (void)fprintf(stderr, "%s:%d: expected 0x%04" PRIX16 \
            ", got 0x%04" PRIX16 "\n", __FILE__, __LINE__, \
            expected_value, actual_value); \
        return 1; \
    } \
} while (0)

static void sample_input(input_state_t *inputs, uint16_t raw)
{
    inputs->raw = raw;
    update_input_state(inputs);
}

static int test_gpio_decoding(void)
{
    input_state_t inputs = {0};

    decode_gpio_input_pins(&inputs, UINT32_MAX, UINT32_MAX);
    CHECK_EQUAL(inputs.raw, 0u);

    decode_gpio_input_pins(&inputs, 0u, 0u);
    CHECK_EQUAL(inputs.raw, 0x0FFFu);

    const uint32_t gpiof = UINT32_MAX & ~(GPIO_IDR_PIN_BIT(13) |
        GPIO_IDR_PIN_BIT(15) | GPIO_IDR_PIN_BIT(3) | GPIO_IDR_PIN_BIT(5));
    const uint32_t gpioe = UINT32_MAX & ~(GPIO_IDR_PIN_BIT(8) |
        GPIO_IDR_PIN_BIT(11));

    decode_gpio_input_pins(&inputs, gpiof, gpioe);
    CHECK_EQUAL(inputs.raw, BTN_A | BTN_X | BTN_L | BTN_START | BTN_DOWN | BTN_RIGHT);

    return 0;
}

static int test_debounce_and_edges(void)
{
    input_state_t inputs = {0};

    for (uint8_t i = 0; i < 4u; ++i) {
        sample_input(&inputs, BTN_A);
        CHECK_EQUAL(inputs.current, 0u);
        CHECK_EQUAL(inputs.pressed, 0u);
    }

    sample_input(&inputs, BTN_A);
    CHECK_EQUAL(inputs.current, BTN_A);
    CHECK_EQUAL(inputs.pressed, BTN_A);
    CHECK_EQUAL(inputs.released, 0u);

    sample_input(&inputs, BTN_A);
    CHECK_EQUAL(inputs.current, BTN_A);
    CHECK_EQUAL(inputs.pressed, 0u);

    sample_input(&inputs, 0u);
    sample_input(&inputs, BTN_A);
    sample_input(&inputs, 0u);
    sample_input(&inputs, BTN_A);
    CHECK_EQUAL(inputs.current, BTN_A);
    CHECK_EQUAL(inputs.released, 0u);

    for (uint8_t i = 0; i < 5u; ++i) {
        sample_input(&inputs, 0u);
    }

    CHECK_EQUAL(inputs.current, 0u);
    CHECK_EQUAL(inputs.pressed, 0u);
    CHECK_EQUAL(inputs.released, BTN_A);

    return 0;
}

int main(void)
{
    if (test_gpio_decoding() != 0) {
        return 1;
    }

    return test_debounce_and_edges();
}
