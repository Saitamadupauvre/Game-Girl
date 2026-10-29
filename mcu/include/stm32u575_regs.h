
#ifndef STM32U575_REGS_H
    #define STM32U575_REGS_H

    #include <stdint.h>

#define MMIO32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

// RCC

#define RCC_BASE_NS 0x46020C00UL    // STM32 MANUAL page 145
#define RCC_AHB2ENR1_OFFSET 0x0000008CUL    // STM32 MANUAL page 556 
#define RCC_AHB2ENR1 MMIO32(RCC_BASE_NS + RCC_AHB2ENR1_OFFSET)

#define RCC_APB1ENR1_OFFSET 0x0000009CUL    // STM32 MANUAL PAGE 562
#define RCC_APB1ENR1 MMIO32(RCC_BASE_NS + RCC_APB1ENR1_OFFSET)


#define RCC_CR_OFFSET 0x00000000UL // RCC Control clock register 11.8.1 in manual
#define RCC_CR MMIO32(RCC_BASE_NS + RCC_CR_OFFSET)
#define OTG_FS_BASE_NS 0x42040000UL // STM32 MANUAL page 146

// RCC_AHB2ENR1 bits

#define RCC_AHB2ENR1_GPIOAEN        (1UL << 0)
#define RCC_AHB2ENR1_GPIOBEN        (1UL << 1)
#define RCC_AHB2ENR1_GPIOCEN        (1UL << 2)
#define RCC_AHB2ENR1_GPIODEN        (1UL << 3)
#define RCC_AHB2ENR1_GPIOEEN        (1UL << 4)
#define RCC_AHB2ENR1_GPIOFEN        (1UL << 5)
#define RCC_AHB2ENR1_GPIOGEN        (1UL << 6)
#define RCC_AHB2ENR1_GPIOHEN        (1UL << 7)
#define RCC_AHB2ENR1_GPIOIEN        (1UL << 8)
#define RCC_AHB2ENR1_GPIOJEN        (1UL << 9)

// RCC_APB1ENR1 bits

#define RCC_APB1ENR1_TIM2EN         (1UL << 0)

// TIM2

#define TIM2_BASE_NS                0x40000000UL

#define TIM_CR1_OFFSET              0x00000000UL
#define TIM_SR_OFFSET               0x00000010UL
#define TIM_EGR_OFFSET              0x00000014UL
#define TIM_CNT_OFFSET              0x00000024UL
#define TIM_PSC_OFFSET              0x00000028UL
#define TIM_ARR_OFFSET              0x0000002CUL

#define TIM2_CR1                    MMIO32(TIM2_BASE_NS + TIM_CR1_OFFSET)
#define TIM2_SR                     MMIO32(TIM2_BASE_NS + TIM_SR_OFFSET)
#define TIM2_EGR                    MMIO32(TIM2_BASE_NS + TIM_EGR_OFFSET)
#define TIM2_CNT                    MMIO32(TIM2_BASE_NS + TIM_CNT_OFFSET)
#define TIM2_PSC                    MMIO32(TIM2_BASE_NS + TIM_PSC_OFFSET)
#define TIM2_ARR                    MMIO32(TIM2_BASE_NS + TIM_ARR_OFFSET)

#define TIM_CR1_CEN                 (1UL << 0)
#define TIM_SR_UIF                  (1UL << 0)
#define TIM_EGR_UG                  (1UL << 0)

// GPIOF

#define GPIOF_BASE_NS 0x42021400UL  // STM32 MANUAL page 147

#define GPIO_MODER_OFFSET   0x00000000UL   // STM32 Man page 633
#define GPIO_PUPDR_OFFSET   0x0000000CUL  // STM32 Man page 635
#define GPIO_IDR_OFFSET     0x00000010UL    // STM32 Man page 635

#define GPIOF_MODER     MMIO32(GPIOF_BASE_NS + GPIO_MODER_OFFSET)
#define GPIOF_PUPDR     MMIO32(GPIOF_BASE_NS + GPIO_PUPDR_OFFSET)
#define GPIOF_IDR       MMIO32(GPIOF_BASE_NS + GPIO_IDR_OFFSET)

// GPIOE

#define GPIOE_BASE_NS   0x04202100UL

#define GPIOE_MODER MMIO32(GPIOE_BASE_NS + GPIO_MODER_OFFSET)
#define GPIOE_PUPDR MMIO32(GPIOE_BASE_NS + GPIO_PUPDR_OFFSET)
#define GPIOE_IDR MMIO32(GPIOE_BASE_NS + GPIO_IDR_OFFSET)


// USB OTG_FS

#define RCC_AHB2ENR1_OTGEN (1UL << 14) 

#define OTG_GUSBCFG_OFFSET  0x0000000CUL 

#define OTG_GUSBCFG MMIO32(OTG_FS_BASE_NS + OTG_GUSBCFG_OFFSET)

#define OTG_GUSBCFG_FDMOD    (1UL << 30)
#define OTG_GUSBCFG_FHMOD    (1UL << 29)

#define OTG_GRSTCTL_OFFSET 0x0000010UL     // STM32 RM0456 Manual 72.15.5 OTG reset register (OTG_GRSTCTL)
#define OTG_GRSTCTL MMIO32(OTG_FS_BASE_NS + OTG_GRSTCTL_OFFSET)

#define OTG_GRSTCTL_AHBIDL (1UL << 31)
#define OTG_GRSTCTL_TXFFLSH (1UL << 5)
#define OTG_GRSTCTL_RXFFLSH (1UL << 4)
#define OTG_GRSTCTL_CSRST (1UL << 0)

#define OTG_GRSTCTL_TXFNUM_Pos  6U
#define OTG_GRSTCTL_TXFNUM_Msk  (0x1FUL << OTG_GRSTCTL_TXFNUM_Pos)

#define OTG_GRXFSIZ_RXFD_Pos    0u
#define OTG_GRXFSIZ_RXFD_Msk    0x0000FFFFUL

#define OTG_GRXFSIZ_OFFSET  0x00000024UL
#define OTG_GRXFSIZ     MMIO32(OTG_FS_BASE_NS + OTG_GRXFSIZ_OFFSET)

#define OTG_GRSTCTL_TXFNUM_ALL  0x10UL     // TXFNUM value that flushes every TX FIFO

#define OTG_GOTGCTL_OFFSET  0x00000000UL
#define OTG_GAHBCFG_OFFSET  0x00000008UL
#define OTG_GINTSTS_OFFSET  0x00000014UL
#define OTG_GINTMSK_OFFSET  0x00000018UL
#define OTG_GRXSTSP_OFFSET  0x00000020UL
#define OTG_DIEPTXF0_OFFSET 0x00000028UL
#define OTG_GCCFG_OFFSET    0x00000038UL
#define OTG_DIEPTXF1_OFFSET 0x00000104UL
#define OTG_DCFG_OFFSET     0x00000800UL
#define OTG_DCTL_OFFSET     0x00000804UL
#define OTG_DSTS_OFFSET     0x00000808UL
#define OTG_DIEPMSK_OFFSET  0x00000810UL
#define OTG_DOEPMSK_OFFSET  0x00000814UL
#define OTG_DAINT_OFFSET    0x00000818UL
#define OTG_DAINTMSK_OFFSET 0x0000081CUL
#define OTG_PCGCCTL_OFFSET  0x00000E00UL

#define OTG_GOTGCTL     MMIO32(OTG_FS_BASE_NS + OTG_GOTGCTL_OFFSET)
#define OTG_GAHBCFG     MMIO32(OTG_FS_BASE_NS + OTG_GAHBCFG_OFFSET)
#define OTG_GINTSTS     MMIO32(OTG_FS_BASE_NS + OTG_GINTSTS_OFFSET)
#define OTG_GINTMSK     MMIO32(OTG_FS_BASE_NS + OTG_GINTMSK_OFFSET)
#define OTG_GRXSTSP     MMIO32(OTG_FS_BASE_NS + OTG_GRXSTSP_OFFSET)
#define OTG_DIEPTXF0    MMIO32(OTG_FS_BASE_NS + OTG_DIEPTXF0_OFFSET)
#define OTG_GCCFG       MMIO32(OTG_FS_BASE_NS + OTG_GCCFG_OFFSET)
#define OTG_DIEPTXF1    MMIO32(OTG_FS_BASE_NS + OTG_DIEPTXF1_OFFSET)
#define OTG_DCFG        MMIO32(OTG_FS_BASE_NS + OTG_DCFG_OFFSET)
#define OTG_DCTL        MMIO32(OTG_FS_BASE_NS + OTG_DCTL_OFFSET)
#define OTG_DSTS        MMIO32(OTG_FS_BASE_NS + OTG_DSTS_OFFSET)
#define OTG_DIEPMSK     MMIO32(OTG_FS_BASE_NS + OTG_DIEPMSK_OFFSET)
#define OTG_DOEPMSK     MMIO32(OTG_FS_BASE_NS + OTG_DOEPMSK_OFFSET)
#define OTG_DAINT       MMIO32(OTG_FS_BASE_NS + OTG_DAINT_OFFSET)
#define OTG_DAINTMSK    MMIO32(OTG_FS_BASE_NS + OTG_DAINTMSK_OFFSET)
#define OTG_PCGCCTL     MMIO32(OTG_FS_BASE_NS + OTG_PCGCCTL_OFFSET)

// Per-endpoint registers, n = endpoint number
#define OTG_DIEPCTL(n)  MMIO32(OTG_FS_BASE_NS + 0x900UL + ((uint32_t)(n) * 0x20UL))
#define OTG_DIEPINT(n)  MMIO32(OTG_FS_BASE_NS + 0x908UL + ((uint32_t)(n) * 0x20UL))
#define OTG_DIEPTSIZ(n) MMIO32(OTG_FS_BASE_NS + 0x910UL + ((uint32_t)(n) * 0x20UL))
#define OTG_DTXFSTS(n)  MMIO32(OTG_FS_BASE_NS + 0x918UL + ((uint32_t)(n) * 0x20UL))
#define OTG_DOEPCTL(n)  MMIO32(OTG_FS_BASE_NS + 0xB00UL + ((uint32_t)(n) * 0x20UL))
#define OTG_DOEPINT(n)  MMIO32(OTG_FS_BASE_NS + 0xB08UL + ((uint32_t)(n) * 0x20UL))
#define OTG_DOEPTSIZ(n) MMIO32(OTG_FS_BASE_NS + 0xB10UL + ((uint32_t)(n) * 0x20UL))

// 32-bit push (TX) / pop (RX) port for endpoint n, not RAM
#define OTG_FIFO(n)     MMIO32(OTG_FS_BASE_NS + 0x1000UL + ((uint32_t)(n) * 0x1000UL))

#define OTG_GOTGCTL_BVALOEN     (1UL << 6)
#define OTG_GOTGCTL_BVALOVAL    (1UL << 7)

#define OTG_GAHBCFG_GINTMSK     (1UL << 0)

#define OTG_GUSBCFG_PHYSEL      (1UL << 6)
#define OTG_GUSBCFG_TRDT_Pos    10U
#define OTG_GUSBCFG_TRDT_Msk    (0xFUL << OTG_GUSBCFG_TRDT_Pos)

// GINTSTS bits, same positions in GINTMSK
#define OTG_GINTSTS_CMOD        (1UL << 0)
#define OTG_GINTSTS_RXFLVL      (1UL << 4)
#define OTG_GINTSTS_USBSUSP     (1UL << 11)
#define OTG_GINTSTS_USBRST      (1UL << 12)
#define OTG_GINTSTS_ENUMDNE     (1UL << 13)
#define OTG_GINTSTS_IEPINT      (1UL << 18)
#define OTG_GINTSTS_OEPINT      (1UL << 19)
#define OTG_GINTSTS_WKUINT      (1UL << 31)

// GINTMSK bits, same position as the GINTSTS flag of the same event.
// Names follow ST's HAL (a trailing M marks the mask version of a flag).
#define OTG_GINTMSK_RXFLVLM             (1UL << 4)
#define OTG_GINTMSK_USBSUSPM            (1UL << 11)
#define OTG_GINTMSK_USBRST              (1UL << 12)
#define OTG_GINTMSK_ENUMDNEM            (1UL << 13)
#define OTG_GINTMSK_IEPINT              (1UL << 18)
#define OTG_GINTMSK_OEPINT              (1UL << 19)
#define OTG_GINTMSK_IISOIXFRM           (1UL << 20)     // incomplete isochronous IN transfer
#define OTG_GINTMSK_PXFRM_IISOOXFRM     (1UL << 21)     // incomplete periodic / isochronous OUT transfer
#define OTG_GINTMSK_WUIM                (1UL << 31)

#define OTG_GRXSTSP_EPNUM_Msk   0xFUL
#define OTG_GRXSTSP_BCNT_Pos    4U
#define OTG_GRXSTSP_BCNT_Msk    (0x7FFUL << OTG_GRXSTSP_BCNT_Pos)
#define OTG_GRXSTSP_PKTSTS_Pos  17U
#define OTG_GRXSTSP_PKTSTS_Msk  (0xFUL << OTG_GRXSTSP_PKTSTS_Pos)

#define OTG_PKTSTS_OUT_DATA     2UL
#define OTG_PKTSTS_SETUP_DATA   6UL

#define OTG_DIEPTXF_DEPTH_Pos   16U

#define OTG_GCCFG_PWRDWN        (1UL << 16)
#define OTG_GCCFG_VBDEN         (1UL << 21)

#define OTG_DCFG_DSPD_Msk       (0x3UL << 0)
#define OTG_DCFG_DSPD_FULL      (0x3UL << 0)
#define OTG_DCFG_DAD_Pos        4U
#define OTG_DCFG_DAD_Msk        (0x7FUL << OTG_DCFG_DAD_Pos)
#define OTG_DCFG_PFIVL_Msk      (0x3UL << 11)

#define OTG_DCTL_RWUSIG         (1UL << 0)
#define OTG_DCTL_SDIS           (1UL << 1)
#define OTG_DCTL_CGINAK         (1UL << 8)

// DIEPCTL bits (IN endpoint n)
#define OTG_DIEPCTL_MPSIZ_Msk       0x7FFUL
#define OTG_DIEPCTL_EP0_MPSIZ_Msk   0x3UL
#define OTG_DIEPCTL_USBAEP          (1UL << 15)
#define OTG_DIEPCTL_EPTYP_Pos       18U
#define OTG_DIEPCTL_EPTYP_INTR      (3UL << OTG_DIEPCTL_EPTYP_Pos)
#define OTG_DIEPCTL_STALL           (1UL << 21)
#define OTG_DIEPCTL_TXFNUM_Pos      22U
#define OTG_DIEPCTL_CNAK            (1UL << 26)
#define OTG_DIEPCTL_SNAK            (1UL << 27)
#define OTG_DIEPCTL_SD0PID          (1UL << 28)
#define OTG_DIEPCTL_EPDIS           (1UL << 30)
#define OTG_DIEPCTL_EPENA           (1UL << 31)

// DOEPCTL bits (OUT endpoint n)
#define OTG_DOEPCTL_STALL           (1UL << 21)
#define OTG_DOEPCTL_CNAK            (1UL << 26)
#define OTG_DOEPCTL_SNAK            (1UL << 27)
#define OTG_DOEPCTL_EPDIS           (1UL << 30)
#define OTG_DOEPCTL_EPENA           (1UL << 31)

// DIEPINT bits, same positions in DIEPMSK
#define OTG_DIEPINT_XFRC            (1UL << 0)
#define OTG_DIEPINT_TOC             (1UL << 3)
#define OTG_DIEPINT_CLEAR_ALL       0x0000FB7FUL

// DOEPINT bits, same positions in DOEPMSK
#define OTG_DOEPINT_XFRC            (1UL << 0)
#define OTG_DOEPINT_STUP            (1UL << 3)
#define OTG_DOEPINT_CLEAR_ALL       0x0000FB7FUL

// DIEPTSIZ fields
#define OTG_DIEPTSIZ_PKTCNT_Pos     19U

// DOEPTSIZ fields
#define OTG_DOEPTSIZ_PKTCNT_Pos     19U
#define OTG_DOEPTSIZ_STUPCNT_Pos    29U

#define OTG_DTXFSTS_INEPTFSAV_Msk 0x0000FFFFUL

#define OTG_FS_IRQ_NUM          73U

// RCC_CR

#define RCC_CR_HSI48ON (1UL << 12)
#define RCC_CR_HSI48RDY (1UL << 13)

// RCC AHB3 clock enable and kernel clock selection

#define RCC_AHB3ENR_OFFSET  0x00000094UL
#define RCC_AHB3ENR     MMIO32(RCC_BASE_NS + RCC_AHB3ENR_OFFSET)
#define RCC_AHB3ENR_PWREN   (1UL << 2)

// ICLKSEL to select clock src for USB

#define RCC_CCIPR1_OFFSET   0x000000E0UL
#define RCC_CCIPR1      MMIO32(RCC_BASE_NS + RCC_CCIPR1_OFFSET)
#define RCC_CCIPR1_ICLKSEL_Msk  (0x3UL << 26)    // 00 = HSI48
#define RCC_CCIPR1_ICLKSEL_HSI48 (0UL << 26)

// PWR

#define PWR_BASE_NS     0x46020800UL
#define PWR_SVMCR_OFFSET    0x00000010UL
#define PWR_SVMCR       MMIO32(PWR_BASE_NS + PWR_SVMCR_OFFSET)
#define PWR_SVMCR_USV   (1UL << 28)     // VDDUSB valid: removes USB isolation

// GPIOA

#define GPIOA_BASE_NS   0x42020000UL

#define GPIO_OSPEEDR_OFFSET 0x00000008UL
#define GPIO_AFRH_OFFSET    0x00000024UL

#define GPIOA_MODER     MMIO32(GPIOA_BASE_NS + GPIO_MODER_OFFSET)
#define GPIOA_OSPEEDR   MMIO32(GPIOA_BASE_NS + GPIO_OSPEEDR_OFFSET)
#define GPIOA_AFRH      MMIO32(GPIOA_BASE_NS + GPIO_AFRH_OFFSET)

#define GPIO_AF10_USB   0xAUL

// NVIC

#define NVIC_ISER2      MMIO32(0xE000E108UL)    // IRQ 64..95
#define NVIC_ICER2      MMIO32(0xE000E188UL)


/*
 * Generic GPIO bit-field helpers
 *
 * MODER and PUPDR use two bits per pin:
 *
 *   pin n -> bits [2n+1 : 2n]
 *
 * MODER:
 *   00 = input
 *   01 = general-purpose output
 *   10 = alternate function
 *   11 = analog
 *
 * PUPDR:
 *   00 = no pull
 *   01 = pull-up
 *   10 = pull-down
 *   11 = reserved
 */

// Helper macros

#define GPIO_2BIT_SHIFT(pin)        ((uint32_t)(pin) * 2UL)
#define GPIO_2BIT_MASK(pin)         (3UL << GPIO_2BIT_SHIFT(pin))

#define GPIO_4BIT_SHIFT(pin)        (((uint32_t)(pin)) * 4UL)
#define GPIO_4BIT_MASK(pin)         (0xFUL << GPIO_4BIT_SHIFT(pin))

#define GPIO_MODE_INPUT(pin)        (0UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_MODE_OUTPUT(pin)       (1UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_MODE_ALT(pin)          (2UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_MODE_ANALOG(pin)       (3UL << GPIO_2BIT_SHIFT(pin))

#define GPIO_PULL_NONE(pin)         (0UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_PULL_UP(pin)           (1UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_PULL_DOWN(pin)         (2UL << GPIO_2BIT_SHIFT(pin))

#define GPIO_SPEED_VERY_HIGH(pin)   (3UL << GPIO_2BIT_SHIFT(pin))

/* AFRH uses four bits per pin, for pins 8..15. */
#define GPIO_AFRH_SHIFT(pin)        (((uint32_t)(pin) - 8UL) * 4UL)
#define GPIO_AFRH_MASK(pin)         (0xFUL << GPIO_AFRH_SHIFT(pin))
#define GPIO_AFRH_AF(pin, af)       ((uint32_t)(af) << GPIO_AFRH_SHIFT(pin))

/* IDR uses one bit per pin. */
#define GPIO_IDR_PIN_BIT(pin)           (1UL << (uint32_t)(pin))


#endif
