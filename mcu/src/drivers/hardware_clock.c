
#include "hardware_clock.h"
#include "stm32u575_regs.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * TIM2 is not running yet, so the waits below count loop passes instead of milliseconds.
 * At 4 MHz this is well over 10 ms, far longer than VOS or MSIS need (microseconds).
 */
#define CLOCK_READY_SPINS   100000UL

#define MSIS_RESET_HZ       4000000UL   // Range 4, the reset value
#define MSIS_24MHZ_HZ       24000000UL  // Range 1

static uint32_t hclk_hz = MSIS_RESET_HZ;

static bool wait_flag_set(volatile uint32_t *reg, uint32_t flag)
{
    for (uint32_t i = 0; i < CLOCK_READY_SPINS; i++) {
        if ((*reg & flag) != 0u) {
            return true;
        }
    }

    return false;
}

/*
 * USB only works in voltage range 1-3 (RM0456 10.5.4, note 12), and the chip resets in range 4.
 * Range 3 allows up to 55 MHz with 0 flash and SRAM wait states up to 24 MHz
 * (RM0456 Table 54 and Table 47), so FLASH_ACR and RAMCFG keep their reset value of 0 WS.
 */
static bool raise_core_voltage(void)
{
    RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;   // PWR clock, needed to write PWR_VOSR
    (void)RCC_AHB3ENR;                  // Read back so the clock is running before PWR is accessed

    PWR_VOSR = (PWR_VOSR & ~PWR_VOSR_VOS_Msk) | PWR_VOSR_VOS_RANGE3;

    return wait_flag_set(&PWR_VOSR, PWR_VOSR_VOSRDY);
}

/*
 * MSIS is already the system clock after reset, so changing its range speeds up HCLK, the APB
 * buses and TIM2 at once. RM0456 11.8.2: the range must not change while MSIS is on but not ready.
 */
static bool set_msis_24mhz(void)
{
    if (!wait_flag_set(&RCC_CR, RCC_CR_MSISRDY)) {
        return false;
    }

    // MSIRGSEL = 1 takes the range from ICSCR1; at 0 (reset) the range in RCC_CSR is used instead
    RCC_ICSCR1 = (RCC_ICSCR1 & ~RCC_ICSCR1_MSISRANGE_Msk) | RCC_ICSCR1_MSISRANGE_24MHZ | RCC_ICSCR1_MSIRGSEL;

    return wait_flag_set(&RCC_CR, RCC_CR_MSISRDY);
}

bool init_hardware_clock(void)
{
    // Voltage first, then frequency. Going down would be the reverse order (RM0456 10.5.4)
    if (!raise_core_voltage() || !set_msis_24mhz()) {
        return false;
    }

    hclk_hz = MSIS_24MHZ_HZ;
    return true;
}

uint32_t hardware_clock_hz(void)
{
    return hclk_hz;
}
