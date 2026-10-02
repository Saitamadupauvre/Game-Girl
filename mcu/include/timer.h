
#ifndef TIMER_H
    #define TIMER_H

    #include <stdbool.h>

void enable_hardware_timer_clocks(void);

bool timer_event_elapsed(void);

#endif
