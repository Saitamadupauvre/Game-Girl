/**
  ******************************************************************************
  * @file    timer_api.c
  * @brief   Non-blocking deadlines on top of time_now_ms().
  *
  *          This file provides functions to:
  *           + Start a deadline
  *           + Check whether it has passed
  ******************************************************************************
  *
  * Tasks never wait: they start a deadline and check it on later loop passes. timer_expired()
  * compares with a signed difference, so it stays correct when the 32-bit millisecond counter
  * wraps (~49 days), as long as a deadline is under ~24 days away.
  *
  * time_now_ms() comes from timer.c on the board and from a fake in the host tests.
  ******************************************************************************
  */

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
