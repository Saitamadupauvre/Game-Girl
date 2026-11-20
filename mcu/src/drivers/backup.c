
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
