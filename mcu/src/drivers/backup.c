/**
  ******************************************************************************
  * @file    backup.c
  * @brief   Backup registers and reset cause: memory that survives a reset.
  *
  *          This file provides functions to:
  *           + Enable access to the backup domain
  *           + Read and clear the reset flags
  *           + Read and write the TAMP backup registers
  *           + Set and fetch the task breadcrumb
  ******************************************************************************
  *
  * The 32 TAMP backup registers (TAMP_BKP0R..TAMP_BKP31R) keep their value through watchdog, pin
  * and software resets. They are cleared by a backup-domain reset: on the Nucleo, unplugging it
  * (VBAT is tied to VDD).
  *
  * Slot map (backup.h):
  *   - BKP_HANG_COUNTS 0: watchdog hang count per subsystem (reset_guard.c)
  *   - BKP_LAST_FAULT  1: last fault_t logged (fault.c), readable in GDB after a reset
  *   - BKP_BREADCRUMB  2: subsystem running now (main.c before each task, the USB ISR)
  *
  * init_backup() runs first in init_system(): without RTCAPBEN reads return 0, and without
  * PWR_DBPR.DBP writes are silently dropped. DBP stays set so the breadcrumb can be written on
  * every loop pass.
  *
  * reset_flags_read_clear() returns RCC_CSR and clears it with RMVF. It does not interpret the
  * flags: reset_guard.c does.
  *
  * Register access only, no host tests. Board checks: docs/todos/board_testing_TODO.md §8.
  ******************************************************************************
  */

#include "stm32u575_regs.h"
#include "backup.h"
#include "fault.h"

#include <stdint.h>

#define BACKUP_REG_COUNT    32u     // TAMP_BKP0R..TAMP_BKP31R


fault_subsystem_t breadcrumb_fetch(void)
{
    return (fault_subsystem_t)backup_read(BKP_BREADCRUMB);
}

void breadcrumb_set(fault_subsystem_t subsys)
{
    backup_write(BKP_BREADCRUMB, (uint32_t)subsys);
}

void backup_write(const uint32_t idx, const uint32_t val)
{
    if (idx >= BACKUP_REG_COUNT) {
        return;
    }

    TAMP_BKPR(idx) = val;
}

uint32_t backup_read(const uint32_t idx)
{
    if (idx >= BACKUP_REG_COUNT) {
        return 0u;
    }

    return TAMP_BKPR(idx);
}

uint32_t reset_flags_read_clear(void)
{
    uint32_t rst_flag = RCC_CSR;
    RCC_CSR |= RCC_CSR_RMVF;

    return rst_flag;
}

void init_backup(void)
{
    RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;       // Enable pwr clock (enabled in hardware_clock.c and usb.c too). 
    (void)RCC_AHB3ENR; 

    PWR_DBPR |= PWR_DBPR_DBP;               // Enables write access to backup domains (TAMP).
    RCC_APB3ENR |= RCC_APB3ENR_RTCAPBEN;    // Enable the RTC/TAMP APB clock.
    (void)RCC_APB3ENR;
}
