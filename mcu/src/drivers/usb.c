
#include "stm32u575_regs.h"
#include "timer_api.h"
#include "cpu.h"
#include "usb.h"
#include "hid.h"
#include "fault.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#define USB_WORD_BYTES  4u
#define USB_BITS_PER_BYTE 8u

enum {
    USB_EP_COUNT = 6,   // Device endpoints including EP0. RM0456 Table 749

    USB_RX_FIFO_SIZE = 64,
    USB_TX0_FIFO_SIZE = 32,
    USB_TX1_FIFO_SIZE = 16,

    USB_TX0_FIFO_START = USB_RX_FIFO_SIZE,
    USB_TX1_FIFO_START = USB_TX0_FIFO_START + USB_TX0_FIFO_SIZE,
    USB_FIFO_END = USB_TX1_FIFO_START + USB_TX1_FIFO_SIZE,   // 112 words used

    USB_SETUP_PACKET_SIZE = 8,
    USB_SETUP_PACKET_NBR = 3,
    USB_EP0_MAX_PACKET_SIZE = 64,

    USB_HID_EP = 1,                 // Interrupt IN endpoint 0x81
    USB_HID_TX_FIFO = 1,            // Uses DIEPTXF1
    USB_HID_EP_MAX_PACKET_SIZE = 8, // Must match wMaxPacketSize in the endpoint descriptor (hid.c)

    USB_TRDT_24MHZ = 0x8,           // GUSBCFG.TRDT for HCLK 24-27.7 MHz. Must follow clock_init()

    USB_IDLE_RATE_UNIT_MS = 4,      // SET_IDLE counts in 4 ms units. HID 1.11 §7.2.4
};

_Static_assert(USB_FIFO_END <= 320, "FIFOs exceed USB RAM");

static uint8_t setup_pkt[USB_SETUP_PACKET_SIZE];

static hid_state_t hid_state;       // Written by apply_setup() in the ISR
static volatile bool ep1_busy;      // A report is armed on EP1 and not yet sent
static volatile bool report_pending; // Send the current report even if the buttons did not change

static hid_report_t last_sent;      // Main loop only: the last report armed on EP1
static deadline_t idle_deadline;    // Main loop only: when the idle rate asks for a resend


/* Waiting */

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


/* FIFO access and flushing */

static uint32_t usb_get_fifo_words(uint16_t data_len)
{
    return (((uint32_t)data_len + (USB_WORD_BYTES - 1u)) / USB_WORD_BYTES);
}

static void usb_rxfifo_read(uint8_t *dest, uint16_t dest_size, uint16_t byte_cnt)
{
    uint32_t words = usb_get_fifo_words(byte_cnt);

    for (uint32_t i = 0; i < words; i++) {
        uint32_t word = OTG_FIFO(0);    // Pops top of receive FIFO.

        for (uint32_t wb = 0; wb < USB_WORD_BYTES; wb++) {
            uint32_t idx = (i * USB_WORD_BYTES) + wb;
            if (idx < byte_cnt && idx < dest_size) {
                dest[idx] = (uint8_t)(word >> (USB_BITS_PER_BYTE * wb));
            }
        }
    }
}

static void usb_txfifo_write(uint8_t ep, const uint8_t *data, uint16_t len)
{
    if (len == 0) {
        return;
    }

    uint32_t words = usb_get_fifo_words(len);

    for (uint32_t i = 0; i < words; i++) {
        uint32_t word = 0;

        for (uint32_t wb = 0; wb < USB_WORD_BYTES; wb++) {
            uint32_t idx = (i * USB_WORD_BYTES) + wb;
            if (idx < len) {
                word |= (uint32_t)data[idx] << (USB_BITS_PER_BYTE * wb);
            }
        }

        OTG_FIFO(ep) = word;
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


/* Endpoint control */

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


/* EP0 helpers */

static void ep0_arm_setup(void)
{
    OTG_DOEPTSIZ(0) = ((uint32_t)USB_SETUP_PACKET_NBR << OTG_DOEPTSIZ_STUPCNT_Pos)
                        | (1UL << OTG_DOEPTSIZ_PKTCNT_Pos)
                        | ((uint32_t)(USB_SETUP_PACKET_NBR * USB_SETUP_PACKET_SIZE) << OTG_DOEPTSIZ_XFRSIZ_Pos);
}

static void ep0_arm_status_out(void)
{
    OTG_DOEPTSIZ(0) = ((uint32_t)USB_SETUP_PACKET_NBR << OTG_DOEPTSIZ_STUPCNT_Pos)
                        | (1UL << OTG_DOEPTSIZ_PKTCNT_Pos)
                        | ((uint32_t)USB_EP0_MAX_PACKET_SIZE << OTG_DOEPTSIZ_XFRSIZ_Pos);

    OTG_DOEPCTL(0) |= OTG_DOEPCTL_EPENA | OTG_DOEPCTL_CNAK;
}

static void ep0_stall(void)
{
    OTG_DIEPCTL(0) |= OTG_DIEPCTL_STALL;
    OTG_DOEPCTL(0) |= OTG_DOEPCTL_STALL;
}

/*
 * Queues one packet on IN endpoint ep. Returns false, without touching the
 * endpoint, if the TX FIFO does not have room for data_len bytes.
 */
static bool usb_in_packet_tx(uint8_t ep, const uint8_t *data, uint16_t data_len)
{
    uint32_t words = usb_get_fifo_words(data_len);
    if (words > (OTG_DTXFSTS(ep) & OTG_DTXFSTS_INEPTFSAV_Msk)) {
        return false;
    }

    OTG_DIEPTSIZ(ep) = (1UL << OTG_DIEPTSIZ_PKTCNT_Pos) | data_len;
    OTG_DIEPCTL(ep) |= OTG_DIEPCTL_EPENA | OTG_DIEPCTL_CNAK;

    usb_txfifo_write(ep, data, data_len);
    return true;
}

/* Sends one EP0 reply packet (data_len <= 64), or stalls EP0 if it cannot. */
static bool ep0_send(const uint8_t *data, uint16_t data_len)
{
    if (data_len > USB_EP0_MAX_PACKET_SIZE || !usb_in_packet_tx(0, data, data_len)) {
        ep0_stall();
        return false;
    }

    if (data_len > 0u) {
        ep0_arm_status_out();
    }

    return true;
}

static void ep0_send_zlp(void)
{
    (void)ep0_send(NULL, 0);
}


/* Control requests and EP1 */

static void ep1_activate(void)
{
    (void)tx_fifo_flush(USB_HID_TX_FIFO);   // Drops a report left from an earlier configuration

    OTG_DIEPCTL(USB_HID_EP) = OTG_DIEPCTL_USBAEP            // Endpoint active in this configuration
                            | OTG_DIEPCTL_EPTYP_INTR        // Interrupt transfers
                            | ((uint32_t)USB_HID_TX_FIFO << OTG_DIEPCTL_TXFNUM_Pos)
                            | OTG_DIEPCTL_SD0PID            // First packet is DATA0
                            | OTG_DIEPCTL_SNAK              // NAK the host until a report is armed
                            | USB_HID_EP_MAX_PACKET_SIZE;   // MPSIZ

    OTG_DAINTMSK |= OTG_DAINT_IEP(USB_HID_EP);
    ep1_busy = false;
    report_pending = true;  // Send the current state straight away, even if unchanged
}

static void ep1_deactivate(void)
{
    if ((OTG_DIEPCTL(USB_HID_EP) & OTG_DIEPCTL_EPENA) != 0u) {
        OTG_DIEPCTL(USB_HID_EP) |= OTG_DIEPCTL_EPDIS | OTG_DIEPCTL_SNAK;   // Aborts an armed report
    }

    OTG_DIEPCTL(USB_HID_EP) &= ~OTG_DIEPCTL_USBAEP;
    OTG_DAINTMSK &= ~OTG_DAINT_IEP(USB_HID_EP);
    ep1_busy = false;
}

/* Runs a SETUP packet through hid_handle_setup(), then applies the answer to the hardware. */
static void apply_setup(const uint8_t setup[USB_SETUP_PACKET_SIZE])
{
    const uint8_t old_config = hid_state.configuration;
    const usb_reply_t reply = hid_handle_setup(setup, &hid_state);

    // Written now, before the status stage: the core only starts using the new address after it
    OTG_DCFG = (OTG_DCFG & ~OTG_DCFG_DAD_Msk) | ((uint32_t)hid_state.address << OTG_DCFG_DAD_Pos);

    if (hid_state.configuration != old_config) {
        if (hid_state.configuration == 1u) {
            ep1_activate();
        } else {
            ep1_deactivate();
        }
    }

    switch (reply.type) {
        case USB_REPLY_DATA:
            (void)ep0_send(reply.data, reply.len);
            break;
        case USB_REPLY_ZLP:
            ep0_send_zlp();
            break;
        case USB_REPLY_STALL:
        default:
            ep0_stall();
            break;
    }
}

/* Interrupt event handlers */

static void usb_on_rxfifo(void)
{
    OTG_GINTMSK &= ~OTG_GINTMSK_RXFLVLM;    // Masks RXFLVLM (Receive FIFO non-empty) while popping

    uint32_t reg_status = OTG_GRXSTSP;  // Reads rx FIFO status and pops top rx FIFO.
    uint16_t byte_count = (uint16_t)((reg_status & OTG_GRXSTSP_BCNT_Msk) >> OTG_GRXSTSP_BCNT_Pos);
    uint32_t packet_status = (reg_status & OTG_GRXSTSP_PKTSTS_Msk) >> OTG_GRXSTSP_PKTSTS_Pos;

    switch (packet_status) {
        case OTG_PKTSTS_SETUP_DATA:
            usb_rxfifo_read(setup_pkt, USB_SETUP_PACKET_SIZE, byte_count);
            break;
        case OTG_PKTSTS_OUT_DATA:
            usb_rxfifo_read(NULL, 0, byte_count);   // Drain unexpected data so the FIFO stays in step
            break;
        default:
            break;
    }

    OTG_GINTMSK |= OTG_GINTMSK_RXFLVLM; // Unmasks RXFLVLM again
}

/* The host held the bus in reset: forget the address and configuration, wait for SETUP on EP0. */
static void usb_on_reset(void)
{
    OTG_DCTL &= ~OTG_DCTL_RWUSIG;   // Stop any remote wakeup signalling
    (void)tx_fifo_flush(OTG_GRSTCTL_TXFNUM_ALL);

    ep1_deactivate();

    for (uint8_t i = 0; i < USB_EP_COUNT; i++) {
        OTG_DIEPINT(i) = OTG_DIEPINT_CLEAR_ALL;
        OTG_DIEPCTL(i) &= ~OTG_DIEPCTL_STALL;
        OTG_DOEPINT(i) = OTG_DOEPINT_CLEAR_ALL;
        OTG_DOEPCTL(i) &= ~OTG_DOEPCTL_STALL;
        OTG_DOEPCTL(i) |= OTG_DOEPCTL_SNAK;
    }

    // Plain `=`: only EP0 can interrupt until SET_CONFIGURATION activates EP1
    OTG_DAINTMSK = OTG_DAINT_OEP(0) | OTG_DAINT_IEP(0);
    OTG_DOEPMSK |= OTG_DOEPINT_STUP | OTG_DOEPINT_XFRC;
    OTG_DIEPMSK |= OTG_DIEPINT_XFRC | OTG_DIEPINT_TOC;

    OTG_DCFG &= ~OTG_DCFG_DAD_Msk;  // Back to address 0
    hid_reset(&hid_state);
    report_pending = false;

    ep0_arm_setup();
}

/* Reset is over and full speed is negotiated: EP0 can be used. */
static void usb_on_enum_done(void)
{
    OTG_DIEPCTL(0) &= ~OTG_DIEPCTL_EP0_MPSIZ_Msk;  // MPSIZ = 0b00, EP0 max packet 64 bytes
    OTG_DCTL |= OTG_DCTL_CGINAK;                    // Clear global IN NAK

    OTG_GUSBCFG = (OTG_GUSBCFG & ~OTG_GUSBCFG_TRDT_Msk) | ((uint32_t)USB_TRDT_24MHZ << OTG_GUSBCFG_TRDT_Pos);
}

static void usb_on_out_ep(void)
{
    if ((OTG_DAINT & OTG_DAINT_OEP(0)) == 0u) {
        return;     // EP0 is the only OUT endpoint
    }

    const uint32_t flags = OTG_DOEPINT(0);
    OTG_DOEPINT(0) = flags;     // Write-1-to-clear every flag that was read

    if ((flags & OTG_DOEPINT_STUP) != 0u) {
        ep0_arm_setup();            // Re-arm first: apply_setup() may arm the status OUT stage
        apply_setup(setup_pkt);
    }
    // XFRC alone means the status OUT ZLP arrived: nothing to do
}

static void usb_on_in_ep(void)
{
    const uint32_t pending = OTG_DAINT;

    if ((pending & OTG_DAINT_IEP(0)) != 0u) {
        OTG_DIEPINT(0) = OTG_DIEPINT(0);    // EP0 reply sent: just clear the flags
    }

    if ((pending & OTG_DAINT_IEP(USB_HID_EP)) != 0u) {
        const uint32_t flags = OTG_DIEPINT(USB_HID_EP);
        OTG_DIEPINT(USB_HID_EP) = flags;

        if ((flags & OTG_DIEPINT_XFRC) != 0u) {
            ep1_busy = false;               // The host collected the report
        }
    }
}

void OTG_FS_IRQHandler(void)
{
    const uint32_t sts = OTG_GINTSTS & OTG_GINTMSK;

    if ((sts & OTG_GINTSTS_USBRST) != 0u) {
        usb_on_reset();
        OTG_GINTSTS = OTG_GINTSTS_USBRST;   // GINTSTS flags are write-1-to-clear
    }

    if ((sts & OTG_GINTSTS_ENUMDNE) != 0u) {
        usb_on_enum_done();
        OTG_GINTSTS = OTG_GINTSTS_ENUMDNE;
    }

    // Before OEPINT: the SETUP bytes must be popped before STUP is handled
    if ((sts & OTG_GINTSTS_RXFLVL) != 0u) {
        usb_on_rxfifo();    // Popping GRXSTSP clears RXFLVL
    }

    if ((sts & OTG_GINTSTS_OEPINT) != 0u) {
        usb_on_out_ep();    // Cleared through DOEPINT
    }

    if ((sts & OTG_GINTSTS_IEPINT) != 0u) {
        usb_on_in_ep();     // Cleared through DIEPINT
    }

    // Suspend and wakeup need no action yet, but an unmasked flag left set would retrigger forever
    if ((sts & OTG_GINTSTS_USBSUSP) != 0u) {
        OTG_GINTSTS = OTG_GINTSTS_USBSUSP;
    }

    if ((sts & OTG_GINTSTS_WKUINT) != 0u) {
        OTG_GINTSTS = OTG_GINTSTS_WKUINT;
    }
}


/* Report sending (main loop) */

static void usb_irq_disable(void)
{
    NVIC_ICER2 = 1UL << (OTG_FS_IRQ_NUM - 64U);
    cpu_dsb();  // The mask must take effect before the next instruction, not a few cycles later
    cpu_isb();
}

static void usb_irq_enable(void)
{
    NVIC_ISER2 = 1UL << (OTG_FS_IRQ_NUM - 64U);
}

void usb_hid_send_if_changed(const input_state_t *inputs)
{
    if (inputs == NULL) {
        return;
    }

    const hid_report_t report = hid_pack(inputs->current);
    const uint8_t bytes[HID_REPORT_SIZE] = {report.buttons, report.hat};

    // The ISR writes hid_state, ep1_busy and DIEPCTL(1) (reset, SET_CONFIGURATION), so all of
    // the check-and-arm runs with the USB interrupt masked
    usb_irq_disable();

    hid_state.last_report[0] = bytes[0];    // GET_REPORT returns the current state
    hid_state.last_report[1] = bytes[1];

    const bool changed = (report.buttons != last_sent.buttons) || (report.hat != last_sent.hat);
    const uint8_t rate = hid_state.idle_rate;
    const bool idle_due = (rate != 0u) && timer_expired(&idle_deadline);   // Rate 0 = never resend

    if ((hid_state.configuration == 1u) && !ep1_busy
            && (changed || idle_due || report_pending)
            && usb_in_packet_tx(USB_HID_EP, bytes, HID_REPORT_SIZE)) {
        ep1_busy = true;    // The IN handler clears it when the host collects the report
        report_pending = false;
        last_sent = report;

        if (rate != 0u) {
            timer_start(&idle_deadline, (uint32_t)rate * USB_IDLE_RATE_UNIT_MS);
        }
    }

    usb_irq_enable();
}


/* Initialisation, in call order */

static void init_usb_power(void)
{
    RCC_AHB3ENR |= RCC_AHB3ENR_PWREN;   // PWR clock, needed to write PWR_SVMCR
    PWR_SVMCR |= PWR_SVMCR_USV;         // VDDUSB valid, removes the USB isolation
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

static fault_t soft_reset_usb_core(void)
{
    // The AHB master must be idle before a core reset
    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_AHBIDL, OTG_GRSTCTL_AHBIDL)) {
        return FAULT_USB_AHB_IDLE;
    }

    OTG_GRSTCTL |= OTG_GRSTCTL_CSRST;   // Core soft reset, the core clears CSRST when done. RM0456 72.15.5
    if (!wait_comp_setup(&OTG_GRSTCTL, OTG_GRSTCTL_CSRST, 0u)) {
        return FAULT_USB_CORE_RESET;
    }

    return FAULT_NONE;
}

static fault_t enable_usb_cores(void)
{
    OTG_GUSBCFG |= OTG_GUSBCFG_PHYSEL;  // Selects the embedded full-speed PHY, normally already set. RM0456 72.15.4

    const fault_t fault = soft_reset_usb_core();
    if (fault != FAULT_NONE) {
        return fault;
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
    if ((OTG_GINTSTS & OTG_GINTSTS_CMOD) != 0u) {   // CMOD: 0 = device mode, 1 = host mode. RM0456 72.15.6
        return FAULT_USB_DEVICE_MODE;
    }

    return FAULT_NONE;
}

static fault_t init_usb_device(void)
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

    if (!tx_fifo_flush(OTG_GRSTCTL_TXFNUM_ALL)) {
        return FAULT_USB_TX_FLUSH;
    }

    if (!rx_fifo_flush()) {
        return FAULT_USB_RX_FLUSH;
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

    OTG_DCTL &= ~OTG_DCTL_SDIS;     // Enables the D+ pull-up so the host can see our device.

    return FAULT_NONE;
}

// Returns FAULT_NONE once the device is connected, or the fault of the first step that timed out.
fault_t init_usb(void)
{
    init_usb_power();
    enable_usb_pins();

    if (!enable_OTG_FS_clocks()) {
        return FAULT_USB_HSI48;
    }

    const fault_t fault = enable_usb_cores();
    if (fault != FAULT_NONE) {
        return fault;
    }

    return init_usb_device();
}
