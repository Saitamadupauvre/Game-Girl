/**
  ******************************************************************************
  * @file    hardware_clock.c
  * @brief   System clock: 24 MHz from MSIS, with a 4 MHz fallback.
  *
  *          This file provides functions to:
  *           + Raise the core voltage and switch MSIS to 24 MHz
  *           + Report the frequency actually running
  ******************************************************************************
  *
  * After reset the STM32U575 runs from MSIS at 4 MHz in voltage range 4. init_hardware_clock()
  * raises the core voltage to range 3 (USB does not work in range 4), then switches MSIS to
  * 24 MHz. MSIS is already the system clock, so HCLK, the APB buses and TIM2 all speed up at once.
  * 24 MHz in range 3 needs no flash or SRAM wait states.
  *
  * On failure it returns the fault_t of the step that timed out and the chip stays at 4 MHz until
  * the next reset (no retry: a VOS or MSIS timeout is a hardware fault). main.c marks it active.
  * hardware_clock_hz() returns the frequency actually running: timer.c sets its prescaler from it,
  * and usb_task.c refuses to start USB below 24 MHz.
  *
  * Runs before init_timer(), so the ready waits count loop passes, not milliseconds.
  ******************************************************************************
  */

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

uint32_t hardware_clock_hz(void)
{
    return hclk_hz;
}

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
static fault_t raise_core_voltage(void)
{
    RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;   // PWR clock, needed to write PWR_VOSR
    (void)RCC_AHB3ENR;                  // Read back so the clock is running before PWR is accessed

    PWR_VOSR = (PWR_VOSR & ~PWR_VOSR_VOS_Msk) | PWR_VOSR_VOS_RANGE3;

    if (!wait_flag_set(&PWR_VOSR, PWR_VOSR_VOSRDY)) {
        return FAULT_CLOCK_VOS;
    }

    return FAULT_NONE;
}

/*
 * MSIS is already the system clock after reset, so changing its range speeds up HCLK, the APB
 * buses and TIM2 at once. RM0456 11.8.2: the range must not change while MSIS is on but not ready.
 */
static fault_t set_msis_24mhz(void)
{
    if (!wait_flag_set(&RCC_CR, RCC_CR_MSISRDY)) {
        return FAULT_CLOCK_MSIS_NOT_READY;
    }

    // MSIRGSEL = 1 takes the range from ICSCR1; at 0 (reset) the range in RCC_CSR is used instead
    RCC_ICSCR1 = (RCC_ICSCR1 & ~RCC_ICSCR1_MSISRANGE_Msk) | RCC_ICSCR1_MSISRANGE_24MHZ | RCC_ICSCR1_MSIRGSEL;

    if (!wait_flag_set(&RCC_CR, RCC_CR_MSISRDY)) {
        return FAULT_CLOCK_MSIS_RANGE;
    }

    return FAULT_NONE;
}

fault_t init_hardware_clock(void)
{
    fault_t fault = raise_core_voltage();
    if (fault != FAULT_NONE) {
        return fault;
    }

    fault = set_msis_24mhz();
    if (fault != FAULT_NONE) {
        return fault;   // hclk_hz stays at 4 MHz
    }

    hclk_hz = MSIS_24MHZ_HZ;
    return FAULT_NONE;
}
