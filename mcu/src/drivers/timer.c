
#include "timer.h"
#include "timer_api.h"
#include "stm32u575_regs.h"


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * TIM2 configuration
 *
 * TIM2 input clock: 4 MHz
 * Desired counter frequency: 1 kHz (1 ms per tick)
 * Auto-reload: UINT32_MAX (continuous 32-bit free-running timebase)
 */

#define TIM2_INPUT_CLOCK_HZ       4000000UL
#define TIM2_COUNTER_CLOCK_HZ     1000UL

#define TIM2_PRESCALER \
    ((TIM2_INPUT_CLOCK_HZ / TIM2_COUNTER_CLOCK_HZ) - 1UL)

#define TIM2_AUTO_RELOAD          UINT32_MAX

static void enable_hardware_timer_clock(void)
{
    RCC_APB1ENR1 |= RCC_APB1ENR1_TIM2EN; // ENABLES TIM2 hardware timer clock (bit 0)
}

static void configure_tim2(void)
{
    TIM2_CR1 &= ~TIM_CR1_CEN;   // Stop TIM2 while configuring

    /*
     * 4 MHz / (3999 + 1) = 1 kHz
     *
     * Therefore each counter tick represents 1 ms.
     */
    
    TIM2_PSC = TIM2_PRESCALER;

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

