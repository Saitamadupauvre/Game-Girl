
#ifndef BACKUP_H
    #define BACKUP_H

    #include "fault.h"
    #include <stdint.h>

    // TAMP_BKPxR map. Fixed: the values survive a reset, so a slot must never change meaning.
    #define BKP_HANG_COUNTS     0u      // 4-bit watchdog hang counter per fault_subsystem_t
    #define BKP_LAST_FAULT      1u      // Last fault_t logged
    #define BKP_BREADCRUMB      2u      // fault_subsystem_t of the task running now


void init_backup(void);
void backup_write(const uint32_t idx, const uint32_t value);
void breadcrumb_set(fault_subsystem_t subsys);

uint32_t reset_flags_read_clear(void);
uint32_t backup_read(const uint32_t idx);

fault_subsystem_t breadcrumb_fetch(void);


#endif
