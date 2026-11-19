
#include "stm32u575_regs.h"

#include <stdint.h>


void init_watchdog(void)
{
    IWDG_KR |= 0xCCCC;   // Enables the watchdog.
    IWDG_KR |= 0x5555;   // Enable write access to the registers.
}
