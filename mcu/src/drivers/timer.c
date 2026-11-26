/**
  ******************************************************************************
  * @file    timer.c
  * @brief   1 ms timebase on TIM2.
  *
  *          This file provides functions to:
  *           + Start TIM2 as a free-running 1 kHz counter
  *           + Read the time in milliseconds
  ******************************************************************************
  *
  * TIM2 free-runs as a 32-bit up-counter at 1 kHz, so time_now_ms() is just TIM2_CNT. The
  * prescaler comes from hardware_clock_hz(), so the tick stays 1 ms at 24 MHz and at the 4 MHz
  * fallback. Must run after init_hardware_clock().
  *
  * No interrupts: tasks poll deadlines through timer_api.c. The counter wraps after ~49 days,
  * which timer_expired() handles with a signed difference.
  ******************************************************************************
  */

#include "hardware_clock.h"
#include "timer.h"
#include "timer_api.h"
#include "stm32u575_regs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * TIM2 configuration
 *
 * TIM2 input clock: hardware_clock_hz(), 24 MHz (or 4 MHz if init_hardware_clock() failed)
 * Desired counter frequency: 1 kHz (1 ms per tick)
 * Auto-reload: UINT32_MAX (continuous 32-bit free-running timebase)
 */

#define TIM2_COUNTER_CLOCK_HZ     1000UL

#define TIM2_AUTO_RELOAD          UINT32_MAX

static void enable_hardware_timer_clock(void)
{
    RCC_APB1ENR1 |= RCC_APB1ENR1_TIM2EN; // ENABLES TIM2 hardware timer clock (bit 0)
}

static void configure_tim2(void)
{
    TIM2_CR1 &= ~TIM_CR1_CEN;   // Stop TIM2 while configuring

    /*
     * 24 MHz / (23999 + 1) = 1 kHz (4 MHz / (3999 + 1) if the clock stayed at reset)
     *
     * Therefore each counter tick represents 1 ms. PSC is 16 bits, so the input clock must stay
     * below 65.536 MHz for a 1 kHz tick.
     */

    TIM2_PSC = (hardware_clock_hz() / TIM2_COUNTER_CLOCK_HZ) - 1UL;

    TIM2_ARR = TIM2_AUTO_RELOAD;    // 32-bit free-running up-counter to UINT32_MAX
    TIM2_CNT = 0u;                  // Start point of our counter

    TIM2_EGR = TIM_EGR_UG;          // Force PSC to become active immediately
    TIM2_SR &= ~TIM_SR_UIF;         // Clear update flag from UG

    TIM2_CR1 |= TIM_CR1_CEN;        // Start TIM2 free-running counter
}

void init_timer(void)
{
    enable_hardware_timer_clock();
    configure_tim2();
}

uint32_t time_now_ms(void)
{
    return TIM2_CNT;
}

