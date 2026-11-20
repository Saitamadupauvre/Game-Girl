
#include "stm32u575_regs.h"
#include "watchdog.h"
#include "fault.h"
#include "timer_api.h"

#define WATCHDOG_SYNC_TIMEOUT_MS    10UL    // LSI start (~260 us) + PR/RLR sync (~6 LSI cycles each) is under 1 ms


void kick_watchdog(void)
{
    IWDG_KR = IWDG_KEY_REFRESH; // Resets counter to RL value (500ms).
}

fault_t init_watchdog(void)
{
    DBGMCU_APB1LFZR |= DBGMCU_APB1LFZR_DBG_IWDG_STOP;   // Stops the decount when in debug mode.

    IWDG_KR = IWDG_KEY_START;   // Enables the watchdog.
    IWDG_KR = IWDG_KEY_UNLOCK;   // Enable write access to the registers.

    IWDG_PR = 3;     // Sets prescaler value to divider / 32: 32 kHz LSI / 32 = 1 ms per watchdog tick.
    IWDG_RLR = 499;   // Sets reload value to 499, which sets the time to 500ms.

    deadline_t wdg_delay;
    timer_start(&wdg_delay, WATCHDOG_SYNC_TIMEOUT_MS);
    
    // Waits for the changed registers to be updated.
    while (IWDG_SR != 0u) {
        if (timer_expired(&wdg_delay)) {
            IWDG_KR = IWDG_KEY_REFRESH;   // Locks the watchdog registers, with current values that managed to get written.
            return FAULT_SYS_WATCHDOG_CONFIG;
        }
    }

    IWDG_KR = IWDG_KEY_REFRESH;   // Locks and refresh with the new written values.
    return FAULT_NONE;
}
