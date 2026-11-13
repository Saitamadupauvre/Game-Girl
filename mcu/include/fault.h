
#ifndef FAULT_H
    #define FAULT_H

    #include <stdint.h>

/*
 * Fault codes: 0xSN
 *
 *   S (high nibble) = subsystem, which part of the firmware failed (fault_subsystem_t)
 *   N (low nibble)  = step,      which wait or check inside it failed
 *
 * The values are fixed on purpose: they are stored in a backup register and sent to the Pi, so an
 * existing code must never change. Add new codes at the end of their subsystem's block.
 * Each subsystem has room for 16 codes (N = 0x0-0xF).
 */

typedef enum {
    FAULT_NONE                  = 0x00,     // Must stay 0: zeroed memory reads as "no fault"

    // 0x1_: clock (hardware_clock.c)
    FAULT_CLOCK_VOS             = 0x10,     // PWR_VOSR.VOSRDY never set after selecting range 3
    FAULT_CLOCK_MSIS_NOT_READY  = 0x11,     // RCC_CR.MSISRDY not set before changing the range
    FAULT_CLOCK_MSIS_RANGE      = 0x12,     // RCC_CR.MSISRDY not set again after switching to 24 MHz

    // 0x2_: USB start-up (usb.c)
    FAULT_USB_HSI48             = 0x20,     // RCC_CR.HSI48RDY never set
    FAULT_USB_AHB_IDLE          = 0x21,     // GRSTCTL.AHBIDL never set before the core soft reset
    FAULT_USB_CORE_RESET        = 0x22,     // GRSTCTL.CSRST never cleared
    FAULT_USB_DEVICE_MODE       = 0x23,     // GINTSTS.CMOD still says host after forcing device mode
    FAULT_USB_TX_FLUSH          = 0x24,     // TX FIFO flush timed out
    FAULT_USB_RX_FLUSH          = 0x25,     // RX FIFO flush timed out

    // 0x3_: system / software
    FAULT_SYS_WATCHDOG_LOOP     = 0x30,     // Too many watchdog resets in a row (#42)
    FAULT_SYS_HARDFAULT         = 0x31,     // Reached the HardFault handler (needs startup code, #46)
} fault_t;

// The high nibble of a fault_t. Use it as an array index: one counter or state per subsystem.
typedef enum {
    FAULT_SUBSYS_NONE   = 0x0,
    FAULT_SUBSYS_CLOCK  = 0x1,
    FAULT_SUBSYS_USB    = 0x2,
    FAULT_SUBSYS_SYS    = 0x3,

    FAULT_SUBSYS_COUNT,                     // Array size, keep last
} fault_subsystem_t;


static inline fault_subsystem_t fault_subsys(fault_t f)
{
    return (fault_subsystem_t)(((uint8_t)f >> 4) & 0xFu);
}

static inline uint8_t fault_step(fault_t f)
{
    return (uint8_t)((uint8_t)f & 0xFu);
}

void fault_log(fault_t f);

#endif
