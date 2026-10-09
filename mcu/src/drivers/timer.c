
#include "stm32u575_regs.h"

#include <stdint.h>
#include <stdbool.h>

void enable_hardware_timer_clocks(void)
{
    RCC_APB1ENR1 |= RCC_APB1ENR1; // ENABLES TIM2 hardware timer clock (bit 0)

    TIM2_CR1 &= ~TIM_CR1_CEN;   // STOP THE TIM2 clock while we configure the rest of the pins

    TIM2_PSC = 3;   // BEBCAUSE TIM2 input clock is set a 4Mhz so PSC is set to 3 bsaed on counter freq. Read documentation.

    TIM2_ARR = 999; // the arr value is our upper limit of count 0-999 ARR included so 1000 refer to 55.4.15 in the manual

    TIM2_CNT = 0;   // Start point of our counter

    TIM2_EGR = TIM_EGR_UG; // Force PSC to become active.

    TIM2_SR &= ~TIM_SR_UIF; // clears value for UG (update flag), section 55.5.5 bit[0]

    TIM2_CR1 |= TIM_CR1_CEN;    // Start tim2 after setup
}

bool timer_event_elapsed(void)
{
    if ((TIM2_SR & TIM_SR_UIF) == 0) { return false; }    // Havent reached ARR value so timer it hasnt been 1 ms.

    TIM2_SR &= ~ TIM_SR_UIF;    // ARR value reached so we clear the bit which puts us back a counter = 0 for our nrext event.

    return true;

}
