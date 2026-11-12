
#ifndef TIMER_API_H
    #define TIMER_API_H

    #include <stdbool.h>
    #include <stdint.h>

#define INPUT_UPDATE_PERIOD_MS    1UL
#define USB_WAIT_PERIOD_MS        25UL


typedef struct {
    uint32_t deadline_ms;
} deadline_t;

uint32_t time_now_ms(void);

void timer_start(deadline_t *timer, uint32_t duration_ms);
bool timer_expired(const deadline_t *timer);


#endif /* TIMER_API_H */
