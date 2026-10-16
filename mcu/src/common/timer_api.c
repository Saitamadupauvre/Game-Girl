
#include "timer_api.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


void timer_start(deadline_t *timer, uint32_t duration_ms)
{
    if (timer == NULL) {
        return;
    }

    timer->deadline_ms = time_now_ms() + duration_ms;
}

bool timer_expired(const deadline_t *timer)
{
    if (timer == NULL) {
        return false;
    }

    return (int32_t)(time_now_ms() - timer->deadline_ms) >= 0;
}
