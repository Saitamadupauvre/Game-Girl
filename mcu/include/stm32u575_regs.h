
#ifndef STM32U575_REGS_H
    #define STM32U575_REGS_H


#define MMIO32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

// RCC

#define RCC_BASE_NS 0x46020C00UL    // STM32 MANUAL page 145
#define RCC_AHB2ENR1_OFFSET 0x0000008CUL    // STM32 MANUAL page 556 
#define RCC_AHB2ENR1 MMIO32(RCC_BASE_NS + RCC_AHB2ENR1_OFFSET)  // TO FIND THE REG ADDRES FOR RCC_AHB2ENR1

#define RCC_APB1ENR1_OFFSET 0x0000009CUL    // STM32 MANUAL PAGE 562
#define RCC_APB1ENR1 MMIO32(RCC_BASE_NS + RCC_APB1ENR1_OFFSET) // Address for RCC_APB1ENR1


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

#define GPIOE_BASE_NS   0x042021000UL

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

// RCC_CR

#define RCC_CR_HSI48ON (1UL << 12)
#define RCC_CR_HSI48RDY (1UL << 13)


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

// HELPFULL MACROS

#define GPIO_2BIT_SHIFT(pin)        ((uint32_t)(pin) * 2UL)
#define GPIO_2BIT_MASK(pin)         (3UL << GPIO_2BIT_SHIFT(pin))

#define GPIO_MODE_INPUT(pin)        (0UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_MODE_OUTPUT(pin)       (1UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_MODE_ALT(pin)          (2UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_MODE_ANALOG(pin)       (3UL << GPIO_2BIT_SHIFT(pin))

#define GPIO_PULL_NONE(pin)         (0UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_PULL_UP(pin)           (1UL << GPIO_2BIT_SHIFT(pin))
#define GPIO_PULL_DOWN(pin)         (2UL << GPIO_2BIT_SHIFT(pin))

/* IDR uses one bit per pin. */
#define GPIO_IDR_PIN_BIT(pin)           (1UL << (uint32_t)(pin))


#endif
