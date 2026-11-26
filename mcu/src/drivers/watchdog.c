/**
  ******************************************************************************
  * @file    watchdog.c
  * @brief   Independent watchdog (IWDG): resets the chip when the main loop stops.
  *
  *          This file provides functions to:
  *           + Start the IWDG with a 500 ms timeout
  *           + Refresh it
  ******************************************************************************
  *
  * LSI 32 kHz / 32 = 1 ms per tick, RLR = 499: 500 ms timeout. LSI is independent of MSIS and
  * HSI48, so a clock fault can't stop it. Once started, only a reset stops it.
  *
  * init_watchdog() follows RM0456 61.4 (window option disabled) and is the last call in
  * init_system(). If IWDG_SR doesn't clear within 10 ms it returns FAULT_SYS_WATCHDOG_CONFIG: the
  * watchdog still runs, on whichever values were applied. It is frozen while the debugger halts
  * the core (DBGMCU).
  *
  * kick_watchdog() is called once per pass, at the top of the main loop only. Never in an ISR or
  * a wait loop, or a stuck loop could keep it happy. After a watchdog reset, reset_guard.c finds
  * out which task hung.
  ******************************************************************************
  */

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
