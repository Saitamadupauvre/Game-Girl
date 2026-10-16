
#include "stm32u575_regs.h"
#include "timer_api.h"
#include "cpu.h"

#include <stdint.h>


// static void clear_rx_fifo_reg(void)
// {
//     OTG_GRSTCTL |= OTG_GRSTCTL_RXFFLSH;     // Request the Receive FIFO flush data
//
//     while ((OTG_GRSTCTL & OTG_GRSTCTL_RXFFLSH) != 0u) {     // Wait for it to complete the flush
//         cpu_nop();
//     }
//
//     return;
// }


static void setup_OTG_FS_mode(void)
{
      /*
     * Force the OTG core into USB device mode.
     *
     * Raspberry Pi = USB host
     * STM32        = USB device
     *
     * FHMOD must be cleared before setting FDMOD.
     * RM0456 §72.15.4, OTG_GUSBCFG.
      */

    OTG_GUSBCFG &= ~OTG_GUSBCFG_FHMOD;
    OTG_GUSBCFG |= OTG_GUSBCFG_FDMOD;

    // Forcing OTG_GS into device mode so we have to wait 25 ms for it to change.
    deadline_t usb_mode_timer = {0};

    timer_start(&usb_mode_timer, USB_WAIT_PERIOD_MS);
    while(!timer_expired(&usb_mode_timer)) {
        cpu_nop();
    }
}

static void config_USB_dev(void)
{
    // Idling OTG core
    while ((OTG_GRSTCTL & OTG_GRSTCTL_AHBIDL) == 0u) {
        cpu_nop();
    }

    OTG_GRSTCTL |= OTG_GRSTCTL_CSRST; // Soft reset the USB OTG core -> 72.15.5 OTG reset register for bit 0 CSRST

    // Idle until full reset is done 
    while ((OTG_GRSTCTL & OTG_GRSTCTL_CSRST) != 0u) {
        cpu_nop(); 
    }
}

static void enable_OTG_FS_clock(void)
{
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_OTGEN;  // Enables OTG_FS for our USB port
}

static void enable_OTG_FS_hsi48_clock(void)
{
    RCC_CR |= RCC_CR_HSI48ON;

    while ((RCC_CR & RCC_CR_HSI48RDY) == 0u) {
        cpu_nop();
    }
}

void init_usb(void)
{
    enable_OTG_FS_hsi48_clock();
    enable_OTG_FS_clock();
    config_USB_dev();
    setup_OTG_FS_mode();

    //config_FIFO_regs();
}

