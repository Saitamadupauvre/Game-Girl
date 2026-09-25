
#include "stm32u575_regs.h"

#include <stdint.h>

void enable_hardware_timer_clocks(void)
{
    RCC_APB1ENR1 |= RCC_APB1ENR1;   // ENABLES TIM2 hardware timer clock (bit 0)
}
