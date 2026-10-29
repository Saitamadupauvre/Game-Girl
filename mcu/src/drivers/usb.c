
#include "stm32u575_regs.h"
#include "timer_api.h"
#include "cpu.h"
#include "usb.h"

#include <stdint.h>
#include <stdbool.h>


enum {
    USB_EP_COUNT = 6,   // Device endpoints including EP0. RM0456 Table 749

    USB_RX_FIFO_SIZE = 64,
    USB_TX0_FIFO_SIZE = 32,
    USB_TX1_FIFO_SIZE = 16,

    USB_TX0_FIFO_START = USB_RX_FIFO_SIZE,
    USB_TX1_FIFO_START = USB_TX0_FIFO_START + USB_TX0_FIFO_SIZE,
    USB_FIFO_END = USB_TX1_FIFO_START + USB_TX1_FIFO_SIZE,   // 112 words used
};

_Static_assert(USB_FIFO_END <= 320, "FIFOs exceed USB RAM");

static void wait_delay(const uint32_t cd)
{
    deadline_t delay = {0};

    timer_start(&delay, cd);
    for (;!timer_expired(&delay);) {
        cpu_nop();
    }
}

/*
 * Busy-waits until the bits of *reg selected by msk equal expected_res.
 *
 *   reg          register to poll
 *   msk          which bits to look at, all other bits are ignored
 *   expected_res value those bits must have; pass msk to wait for them to be set,
 *                or 0 to wait for them to clear
 *
 * Returns true once the condition holds, or false if it still does not after
 * USB_CORE_TIMEOUT_MS, so a dead core or clock cannot hang boot.
 */
static bool wait_comp_setup(volatile uint32_t *reg, uint32_t msk, uint32_t expected_res)
{
    deadline_t timeout = {0};

    timer_start(&timeout, USB_CORE_TIMEOUT_MS);
    for(;(*reg & msk) != expected_res;) {
        if (timer_expired(&timeout)) {
            return false;
        }

        cpu_nop();
    }

    return true;
}

static void disable_in_ep(uint8_t ep)
{
    if ((OTG_DIEPCTL(ep) & OTG_DIEPCTL_EPENA) != 0u) {
        OTG_DIEPCTL(ep) = (ep == 0u) ? OTG_DIEPCTL_SNAK : (OTG_DIEPCTL_EPDIS | OTG_DIEPCTL_SNAK);
    } else {
        OTG_DIEPCTL(ep) = 0;
    }
}

static void disable_out_ep(uint8_t ep)
{
    if ((OTG_DOEPCTL(ep) & OTG_DOEPCTL_EPENA) != 0u) {
        OTG_DOEPCTL(ep) = (ep == 0u) ? OTG_DOEPCTL_SNAK : (OTG_DOEPCTL_EPDIS | OTG_DOEPCTL_SNAK);
    } else {
        OTG_DOEPCTL(ep) = 0;
    }
}

static void disable_usb_endpoints(void)
{
    for (uint8_t i = 0; i < USB_EP_COUNT; i++) {
        disable_in_ep(i);
        disable_out_ep(i);

        OTG_DIEPTSIZ(i) = 0;
        OTG_DOEPTSIZ(i) = 0;
        OTG_DIEPINT(i) = OTG_DIEPINT_CLEAR_ALL;
        OTG_DOEPINT(i) = OTG_DOEPINT_CLEAR_ALL;
    }
}

static bool rx_fifo_flush(void)
{
    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_AHBIDL, OTG_GRSTCTL_AHBIDL)) {
        return false;
    }

    OTG_GRSTCTL = OTG_GRSTCTL_RXFFLSH;  // Flush the RX FIFO
    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_RXFFLSH, 0u)) {
        return false;
    }

    return true;
}

static bool tx_fifo_flush(uint32_t fifo_nbr)
{
    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_AHBIDL, OTG_GRSTCTL_AHBIDL)) {
        return false;
    }

    OTG_GRSTCTL = OTG_GRSTCTL_TXFFLSH | (fifo_nbr << OTG_GRSTCTL_TXFNUM_Pos);   // Flushes contents of transmit FIFO number

    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_TXFFLSH, 0u)) {
        return false;
    }

    return true;
}

static bool init_usb_device(void)
{
    OTG_GCCFG |= OTG_GCCFG_VBDEN;   // B-session valid comes from VBUS sensing on PA9 (OTG_FS_VBUS)

    OTG_DCTL |= OTG_DCTL_SDIS;      // Soft-disconnect from the host until the rest of the setup is done
    OTG_PCGCCTL = 0;                // Clears the power and clock gating bits

    // Clear device speed, device address and periodic frame interval, then select full speed
    OTG_DCFG &= ~(OTG_DCFG_DSPD_Msk | OTG_DCFG_DAD_Msk | OTG_DCFG_PFIVL_Msk);
    OTG_DCFG |= OTG_DCFG_DSPD_FULL;     // Full speed on the embedded PHY

    // FIFO setup: set the depths, then flush the FIFOs
    OTG_GRXFSIZ = USB_RX_FIFO_SIZE; // sets the RX FIFO depth in 32-bit words, shared by the OUT endpoints (28 is the minimum).
    OTG_DIEPTXF0 = ((uint32_t)USB_TX0_FIFO_SIZE << OTG_DIEPTXF_DEPTH_Pos) | USB_TX0_FIFO_START;
    OTG_DIEPTXF1 = ((uint32_t)USB_TX1_FIFO_SIZE << OTG_DIEPTXF_DEPTH_Pos) | USB_TX1_FIFO_START;

    if (!tx_fifo_flush(OTG_GRSTCTL_TXFNUM_ALL) || !rx_fifo_flush()) {
        return false;
    }

    // Mask the endpoint interrupts while the endpoints are cleared
    OTG_DIEPMSK = 0;    // IN endpoint interrupt causes. RM0456 72.15.36
    OTG_DOEPMSK = 0;    // OUT endpoint interrupt causes. RM0456 72.15.37
    OTG_DAINTMSK = 0;   // Per-endpoint masks for every endpoint (bits 15:0 IN, bits 31:16 OUT). RM0456 72.15.39

    disable_usb_endpoints();

    OTG_GINTMSK = 0;    // Disable all interrupts.
    OTG_GINTSTS = 0xBFFFFFFFU;  // Clear every interrupt flag except bit 30 (SRQINT, VBUS valid status).

    // Enable interrupts matching to the Device mode ONLY
    OTG_GINTMSK |= OTG_GINTMSK_USBSUSPM | OTG_GINTMSK_USBRST | OTG_GINTMSK_RXFLVLM |
                   OTG_GINTMSK_ENUMDNEM | OTG_GINTMSK_IEPINT |
                   OTG_GINTMSK_OEPINT   | OTG_GINTMSK_WUIM;

    OTG_GAHBCFG |= OTG_GAHBCFG_GINTMSK;     // Enables global core interrupt.
    NVIC_ISER2 = 1UL << (OTG_FS_IRQ_NUM - 64U);

    OTG_DCTL &= ~OTG_DCTL_SDIS;     // Enables the d+ pull up so the host device can see our usb.

    return true;
}

static bool soft_reset_usb_core(void)
{
    // The AHB master must be idle before a core reset
    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_AHBIDL, OTG_GRSTCTL_AHBIDL)) {
        return false;
    }

    OTG_GRSTCTL |= OTG_GRSTCTL_CSRST;   // Core soft reset, the core clears CSRST when done. RM0456 72.15.5
    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_CSRST, 0u)) {
        return false;
    }

    return true;
}

static bool enable_usb_cores(void)
{
    OTG_GUSBCFG |= OTG_GUSBCFG_PHYSEL;  // Selects the embedded full-speed PHY, normally already set. RM0456 72.15.4

    if (!soft_reset_usb_core()) {
        return false;
    }
    OTG_GCCFG |= OTG_GCCFG_PWRDWN;  // 1 = transceiver powered. RM0456 72.15.15

    /*
     * Force the OTG core into USB device mode.
     *
     * Raspberry Pi = USB host
     * STM32        = USB device
     *
     * FHMOD must be cleared before setting FDMOD.
     * RM0456 72.15.4, OTG_GUSBCFG.
     */
    OTG_GUSBCFG &= ~OTG_GUSBCFG_FHMOD;
    OTG_GUSBCFG |= OTG_GUSBCFG_FDMOD;

    // The mode change takes up to 25 ms
    wait_delay(USB_WAIT_PERIOD_MS);
    if ((OTG_GINTSTS & OTG_GINTSTS_CMOD) != 0u) {   // CMOD: 0 = device mode, 1 = host mode, so 1 means the force failed. RM0456 72.15.6
        return false;
    }

    return true;
}

static bool enable_OTG_FS_clocks(void)
{
    RCC_CR |= RCC_CR_HSI48ON;

    if (!wait_comp_setup(&RCC_CR, RCC_CR_HSI48RDY, RCC_CR_HSI48RDY)) {
        return false;
    }

    RCC_CCIPR1 &= ~RCC_CCIPR1_ICLKSEL_Msk;  // ICLKSEL = 00 selects HSI48 as the USB kernel clock

    RCC_AHB2ENR1 |= RCC_AHB2ENR1_OTGEN;     // Enables the OTG_FS peripheral clock
    return true;
}

static void enable_usb_pins(void)
{
    RCC_AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;

    // PA11 = USB_DM, PA12 = USB_DP. See docs/hardware/DS_stm32u575cg.pdf page 141
    GPIOA_MODER &= ~(GPIO_2BIT_MASK(11) | GPIO_2BIT_MASK(12));
    GPIOA_MODER |= GPIO_MODE_ALT(11) | GPIO_MODE_ALT(12);

    GPIOA_OSPEEDR |= GPIO_SPEED_VERY_HIGH(11) | GPIO_SPEED_VERY_HIGH(12);  // Full-speed USB needs fast edges

    // AFRH holds a 4-bit AF field per pin for pins 8-15, so each pin is cleared then set to AF10
    GPIOA_AFRH &= ~(GPIO_AFRH_MASK(11) | GPIO_AFRH_MASK(12));
    GPIOA_AFRH |= GPIO_AFRH_AF(11, GPIO_AF10_USB) | GPIO_AFRH_AF(12, GPIO_AF10_USB);    // AF10 routes the pins to OTG_FS
}

static void init_usb_power(void)
{
    RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;   // PWR clock, needed to write PWR_SVMCR
    PWR_SVMCR |= PWR_SVMCR_USV;         // VDDUSB valid, removes the USB isolation
}

void init_usb(void)
{
    init_usb_power();
    enable_usb_pins();

    if (!enable_OTG_FS_clocks() || !enable_usb_cores()) {
        return;
    }

    if (!init_usb_device()) {
        return;
    }
}
