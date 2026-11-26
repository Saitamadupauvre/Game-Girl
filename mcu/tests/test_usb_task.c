/*
 * usb_task() with the hardware faked at link time: usb.c and hardware_clock.c are not built,
 * this file defines the functions usb_task.c calls instead. fault.c and timer_api.c are real.
 */

#include "usb.h"
#include "backup.h"
#include "fault.h"
#include "hardware_clock.h"
#include "inputs.h"
#include "timer_api.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define CLOCK_24MHZ     24000000UL
#define CLOCK_4MHZ      4000000UL

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: assertion failed: %s\n", \
            __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define CHECK_EQUAL_U32(actual, expected) do { \
    const uint32_t actual_val = (uint32_t)(actual); \
    const uint32_t expected_val = (uint32_t)(expected); \
    if (actual_val != expected_val) { \
        (void)fprintf(stderr, "%s:%d: expected 0x%02" PRIX32 \
            ", got 0x%02" PRIX32 "\n", __FILE__, __LINE__, \
            expected_val, actual_val); \
        return 1; \
    } \
} while (0)


/* ---- Fakes ---- */

static uint32_t now_ms;
static uint32_t clock_hz;
static input_state_t fake_inputs;

static uint32_t init_failures_left;     // init_usb() fails this many times, then succeeds
static fault_t init_fault;              // What init_usb() returns while failing
static uint32_t init_calls;
static uint32_t hard_reset_calls;
static uint32_t send_calls;
static const input_state_t *last_sent;

static bool host_waiting;               // usb_waiting_for_host()
static bool host_configured;            // usb_is_configured()
static uint32_t host_activity;          // usb_host_activity()
static bool pullup_on;
static uint32_t disconnect_calls;
static uint32_t connect_calls;

void backup_write(const uint32_t idx, const uint32_t value)
{
    (void)idx;      // fault_log() saves the last fault; not checked here
    (void)value;
}

uint32_t time_now_ms(void)
{
    return now_ms;
}

uint32_t hardware_clock_hz(void)
{
    return clock_hz;
}

const input_state_t *inputs_get(void)
{
    return &fake_inputs;
}

fault_t init_usb(void)
{
    init_calls++;
    if (init_failures_left > 0) {
        init_failures_left--;
        return init_fault;
    }

    return FAULT_NONE;
}

void hard_reset_usb(void)
{
    hard_reset_calls++;
}

void usb_hid_send_if_changed(const input_state_t *inputs)
{
    send_calls++;
    last_sent = inputs;
}

bool usb_waiting_for_host(void)
{
    return host_waiting;
}

uint32_t usb_host_activity(void)
{
    return host_activity;
}

bool usb_is_configured(void)
{
    return host_configured;
}

void usb_soft_disconnect(void)
{
    disconnect_calls++;
    pullup_on = false;
}

void usb_soft_connect(void)
{
    connect_calls++;
    pullup_on = true;
}


/* ---- Helpers ---- */

static void setup(uint32_t failures, fault_t fault)
{
    usb_task_reset_test();
    fault_reset_test();

    now_ms = 0;
    clock_hz = CLOCK_24MHZ;
    init_failures_left = failures;
    init_fault = fault;
    init_calls = 0;
    hard_reset_calls = 0;
    send_calls = 0;
    last_sent = NULL;

    // Healthy host by default: configured, nothing pending
    host_waiting = false;
    host_configured = true;
    host_activity = 0;
    pullup_on = true;
    disconnect_calls = 0;
    connect_calls = 0;
}

// Host stuck: bus active, a report armed on EP1, and the host never collects it
static void host_stalls(void)
{
    host_waiting = true;
    host_configured = true;
}

static void run_until_disconnects(uint32_t count)
{
    for (uint32_t i = 0; (i < 100000u) && (disconnect_calls < count); i++) {
        usb_task();
        now_ms++;
    }
}

// One main-loop pass per millisecond, like the firmware's superloop.
static void run_for_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        usb_task();
        now_ms++;
    }
}


/* ---- Tests ---- */

int test_first_attempt_succeeds(void)
{
    setup(0, FAULT_NONE);

    run_for_ms(10);

    CHECK_EQUAL_U32(init_calls, 1);
    CHECK_EQUAL_U32(hard_reset_calls, 0);
    CHECK(send_calls > 0);
    CHECK(last_sent == &fake_inputs);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_NONE);

    return 0;
}

int test_late_success(void)
{
    setup(2, FAULT_USB_VDDUSB);

    run_for_ms(1000);

    CHECK_EQUAL_U32(init_calls, 3);
    CHECK_EQUAL_U32(hard_reset_calls, 2);
    CHECK(send_calls > 0);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_NONE);

    return 0;
}

int test_gives_up_after_three_attempts(void)
{
    setup(UINT32_MAX, FAULT_USB_BSESSION);

    run_for_ms(1000);

    CHECK_EQUAL_U32(init_calls, 3);
    CHECK_EQUAL_U32(hard_reset_calls, 3);     // The last attempt is reset too
    CHECK_EQUAL_U32(send_calls, 0);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_BSESSION);

    // FAILED is final: no more attempts however long the loop runs
    run_for_ms(10000);
    CHECK_EQUAL_U32(init_calls, 3);
    CHECK_EQUAL_U32(send_calls, 0);

    return 0;
}

int test_active_fault_is_last_attempts_cause(void)
{
    // Fails 3 times; only the cause that made it give up is active
    setup(UINT32_MAX, FAULT_USB_HSI48);
    run_for_ms(150);                    // 2 attempts with HSI48
    init_fault = FAULT_USB_VDDUSB;
    run_for_ms(1000);

    CHECK_EQUAL_U32(init_calls, 3);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_VDDUSB);

    return 0;
}

int test_backoff_waits_100ms(void)
{
    setup(1, FAULT_USB_CORE_RESET);

    usb_task();                         // t = 0: attempt 1 fails, backoff starts
    CHECK_EQUAL_U32(init_calls, 1);

    now_ms = 99;
    usb_task();
    usb_task();
    CHECK_EQUAL_U32(init_calls, 1);     // Still waiting

    now_ms = 100;
    usb_task();                         // Backoff over
    usb_task();                         // Attempt 2
    CHECK_EQUAL_U32(init_calls, 2);

    return 0;
}

int test_clock_fallback_skips_usb(void)
{
    setup(0, FAULT_NONE);
    clock_hz = CLOCK_4MHZ;

    run_for_ms(1000);

    CHECK_EQUAL_U32(init_calls, 0);
    CHECK_EQUAL_U32(hard_reset_calls, 0);
    CHECK_EQUAL_U32(send_calls, 0);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_NONE);   // The clock owns this fault

    return 0;
}

int test_success_clears_earlier_usb_fault(void)
{
    setup(0, FAULT_NONE);
    fault_set_failure(FAULT_USB_VDDUSB);

    run_for_ms(10);

    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_NONE);

    return 0;
}

int test_stalled_host_is_replugged(void)
{
    setup(0, FAULT_NONE);
    run_for_ms(10);
    host_stalls();

    run_for_ms(1980);
    CHECK_EQUAL_U32(disconnect_calls, 0);

    run_for_ms(30);
    CHECK_EQUAL_U32(disconnect_calls, 1);
    CHECK(!pullup_on);

    return 0;
}

int test_pullup_off_for_100ms(void)
{
    setup(0, FAULT_NONE);
    run_for_ms(10);
    host_stalls();
    run_until_disconnects(1);

    const uint32_t sends = send_calls;
    run_for_ms(99);
    CHECK_EQUAL_U32(connect_calls, 0);
    CHECK_EQUAL_U32(send_calls, sends);     // No reports while disconnected

    run_for_ms(2);
    CHECK_EQUAL_U32(connect_calls, 1);
    CHECK(pullup_on);

    return 0;
}

int test_host_activity_prevents_replug(void)
{
    // Slow host: waiting, but it makes progress every second
    setup(0, FAULT_NONE);
    run_for_ms(10);
    host_stalls();

    for (uint32_t i = 0; i < 10; i++) {
        run_for_ms(1000);
        host_activity++;
    }

    CHECK_EQUAL_U32(disconnect_calls, 0);

    return 0;
}

int test_idle_bus_never_replugs(void)
{
    // Suspended, or the Pi is booting: the device is not waiting on the host
    setup(0, FAULT_NONE);
    host_waiting = false;
    host_configured = false;

    run_for_ms(20000);

    CHECK_EQUAL_U32(disconnect_calls, 0);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_NONE);

    return 0;
}

int test_replug_limit_fails(void)
{
    setup(0, FAULT_NONE);
    run_for_ms(10);
    host_stalls();

    run_for_ms(20000);

    CHECK_EQUAL_U32(disconnect_calls, 3);
    CHECK_EQUAL_U32(hard_reset_calls, 1);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_HOST_LOST);

    const uint32_t sends = send_calls;
    run_for_ms(10000);
    CHECK_EQUAL_U32(disconnect_calls, 3);
    CHECK_EQUAL_U32(send_calls, sends);

    return 0;
}

int test_recovery_resets_replug_count(void)
{
    setup(0, FAULT_NONE);
    run_for_ms(10);
    host_stalls();
    run_until_disconnects(2);
    run_for_ms(200);                    // Reconnected

    host_waiting = false;               // The host enumerated and collected the report
    run_for_ms(10);
    host_stalls();

    // Without the reset it would fail after 1 more replug; with it, 3 more
    run_for_ms(30000);
    CHECK_EQUAL_U32(disconnect_calls, 5);
    CHECK_EQUAL_U32(fault_read_active(FAULT_SUBSYS_USB), FAULT_USB_HOST_LOST);

    return 0;
}

int test_unconfigured_does_not_reset_replug_count(void)
{
    // Bus going idle between replugs is not a recovery
    setup(0, FAULT_NONE);
    run_for_ms(10);
    host_stalls();
    run_until_disconnects(2);
    run_for_ms(200);

    host_waiting = false;
    host_configured = false;
    run_for_ms(10);
    host_stalls();

    run_for_ms(30000);
    CHECK_EQUAL_U32(disconnect_calls, 3);

    return 0;
}

int main(void)
{
    if (test_first_attempt_succeeds() != 0) {
        return 1;
    }

    if (test_late_success() != 0) {
        return 1;
    }

    if (test_gives_up_after_three_attempts() != 0) {
        return 1;
    }

    if (test_active_fault_is_last_attempts_cause() != 0) {
        return 1;
    }

    if (test_backoff_waits_100ms() != 0) {
        return 1;
    }

    if (test_clock_fallback_skips_usb() != 0) {
        return 1;
    }

    if (test_success_clears_earlier_usb_fault() != 0) {
        return 1;
    }

    if (test_stalled_host_is_replugged() != 0) {
        return 1;
    }

    if (test_pullup_off_for_100ms() != 0) {
        return 1;
    }

    if (test_host_activity_prevents_replug() != 0) {
        return 1;
    }

    if (test_idle_bus_never_replugs() != 0) {
        return 1;
    }

    if (test_replug_limit_fails() != 0) {
        return 1;
    }

    if (test_recovery_resets_replug_count() != 0) {
        return 1;
    }

    if (test_unconfigured_does_not_reset_replug_count() != 0) {
        return 1;
    }

    return 0;
}
