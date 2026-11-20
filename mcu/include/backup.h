
#ifndef BACKUP_H
    #define BACKUP_H

    #include "fault.h"
    #include <stdint.h>


void init_backup(void);
void backup_write(const uint32_t idx, uint32_t value);
void breadcrumb_set(fault_subsystem_t subsys);

uint32_t backup_read(const uint32_t idx);
fault_subsystem_t breadcrumb_fetch(void);


#endif
