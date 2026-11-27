
.syntax unified

.cpu cortex-m33
.thumb

// Creates a named section in memory called .isr_vector: a = allocation, %progbits says this section has content.
.section .isr_vector, "a", %progbits

.type isr_vector, %object

// One table entry whose handler is a weak alias of Default_Handler: a C function with the same
// name (e.g. OTG_FS_IRQHandler in usb.c) replaces it at link time, without editing this table.
.macro vector handler
    .word \handler
    .weak \handler
    .thumb_set \handler, Default_Handler
.endm

// RM0456 Table 186. Core entries, then IRQ n at entry 16 + n (offset 0x40 + 4n).
// Ends at IRQ 125: positions 126+ are peripherals the STM32U575 does not have.
isr_vector:
    .word _estack                   // Initial MSP
    .word Reset_Handler
    vector NMI_Handler
    vector HardFault_Handler
    vector MemManage_Handler
    vector BusFault_Handler
    vector UsageFault_Handler
    vector SecureFault_Handler
    .word 0                         // Reserved
    .word 0                         // Reserved
    .word 0                         // Reserved
    vector SVC_Handler
    vector DebugMon_Handler
    .word 0                         // Reserved
    vector PendSV_Handler
    vector SysTick_Handler

    vector WWDG_IRQHandler          // IRQ 0
    vector PVD_PVM_IRQHandler
    vector RTC_IRQHandler
    vector RTC_S_IRQHandler
    vector TAMP_IRQHandler
    vector RAMCFG_IRQHandler
    vector FLASH_IRQHandler
    vector FLASH_S_IRQHandler
    vector GTZC_IRQHandler
    vector RCC_IRQHandler
    vector RCC_S_IRQHandler         // IRQ 10
    vector EXTI0_IRQHandler
    vector EXTI1_IRQHandler
    vector EXTI2_IRQHandler
    vector EXTI3_IRQHandler
    vector EXTI4_IRQHandler
    vector EXTI5_IRQHandler
    vector EXTI6_IRQHandler
    vector EXTI7_IRQHandler
    vector EXTI8_IRQHandler
    vector EXTI9_IRQHandler         // IRQ 20
    vector EXTI10_IRQHandler
    vector EXTI11_IRQHandler
    vector EXTI12_IRQHandler
    vector EXTI13_IRQHandler
    vector EXTI14_IRQHandler
    vector EXTI15_IRQHandler
    vector IWDG_IRQHandler
    vector SAES_IRQHandler
    vector GPDMA1_CH0_IRQHandler
    vector GPDMA1_CH1_IRQHandler    // IRQ 30
    vector GPDMA1_CH2_IRQHandler
    vector GPDMA1_CH3_IRQHandler
    vector GPDMA1_CH4_IRQHandler
    vector GPDMA1_CH5_IRQHandler
    vector GPDMA1_CH6_IRQHandler
    vector GPDMA1_CH7_IRQHandler
    vector ADC12_IRQHandler
    vector DAC1_IRQHandler
    vector FDCAN1_IT0_IRQHandler
    vector FDCAN1_IT1_IRQHandler    // IRQ 40
    vector TIM1_BRK_IRQHandler
    vector TIM1_UP_IRQHandler
    vector TIM1_TRG_COM_IRQHandler
    vector TIM1_CC_IRQHandler
    vector TIM2_IRQHandler
    vector TIM3_IRQHandler
    vector TIM4_IRQHandler
    vector TIM5_IRQHandler
    vector TIM6_IRQHandler
    vector TIM7_IRQHandler          // IRQ 50
    vector TIM8_BRK_IRQHandler
    vector TIM8_UP_IRQHandler
    vector TIM8_TRG_COM_IRQHandler
    vector TIM8_CC_IRQHandler
    vector I2C1_EV_IRQHandler
    vector I2C1_ER_IRQHandler
    vector I2C2_EV_IRQHandler
    vector I2C2_ER_IRQHandler
    vector SPI1_IRQHandler
    vector SPI2_IRQHandler          // IRQ 60
    vector USART1_IRQHandler
    vector USART2_IRQHandler
    vector USART3_IRQHandler
    vector UART4_IRQHandler
    vector UART5_IRQHandler
    vector LPUART1_IRQHandler
    vector LPTIM1_IRQHandler
    vector LPTIM2_IRQHandler
    vector TIM15_IRQHandler
    vector TIM16_IRQHandler         // IRQ 70
    vector TIM17_IRQHandler
    vector COMP_IRQHandler
    vector OTG_FS_IRQHandler        // IRQ 73, offset 0x164 (usb.c)
    vector CRS_IRQHandler
    vector FMC_IRQHandler
    vector OCTOSPI1_IRQHandler
    vector PWR_S3WU_IRQHandler
    vector SDMMC1_IRQHandler
    vector SDMMC2_IRQHandler
    vector GPDMA1_CH8_IRQHandler    // IRQ 80
    vector GPDMA1_CH9_IRQHandler
    vector GPDMA1_CH10_IRQHandler
    vector GPDMA1_CH11_IRQHandler
    vector GPDMA1_CH12_IRQHandler
    vector GPDMA1_CH13_IRQHandler
    vector GPDMA1_CH14_IRQHandler
    vector GPDMA1_CH15_IRQHandler
    vector I2C3_EV_IRQHandler
    vector I2C3_ER_IRQHandler
    vector SAI1_IRQHandler          // IRQ 90
    vector SAI2_IRQHandler
    vector TSC_IRQHandler
    vector AES_IRQHandler
    vector RNG_IRQHandler
    vector FPU_IRQHandler
    vector HASH_IRQHandler
    vector PKA_IRQHandler
    vector LPTIM3_IRQHandler
    vector SPI3_IRQHandler
    vector I2C4_ER_IRQHandler       // IRQ 100
    vector I2C4_EV_IRQHandler
    vector MDF1_FLT0_IRQHandler
    vector MDF1_FLT1_IRQHandler
    vector MDF1_FLT2_IRQHandler
    vector MDF1_FLT3_IRQHandler
    vector UCPD1_IRQHandler
    vector ICACHE_IRQHandler
    vector OTFDEC1_IRQHandler
    vector OTFDEC2_IRQHandler
    vector LPTIM4_IRQHandler        // IRQ 110
    vector DCACHE1_IRQHandler
    vector ADF1_FLT0_IRQHandler
    vector ADC4_IRQHandler
    vector LPDMA1_CH0_IRQHandler
    vector LPDMA1_CH1_IRQHandler
    vector LPDMA1_CH2_IRQHandler
    vector LPDMA1_CH3_IRQHandler
    vector DMA2D_IRQHandler
    vector DCMI_PSSI_IRQHandler
    vector OCTOSPI2_IRQHandler      // IRQ 120
    vector MDF1_FLT4_IRQHandler
    vector MDF1_FLT5_IRQHandler
    vector CORDIC_IRQHandler
    vector FMAC_IRQHandler
    vector LSECSS_IRQHandler        // IRQ 125

.size isr_vector, . - isr_vector

.section .text

.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:

    ldr r0, =_estack // Writes estack address to register 0.
    mov sp, r0

    // Loads 3 memory addresses into registers before copying. sidata is FLASH mem the rest is RAM
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

copy_data:
    // Jumps to init_bss if data has been copied.
    cmp r1, r2
    bhs init_bss

    ldr r3, [r0]    // Reads the data in r0 into r3.
    str r3, [r1]    // Writes data from r3 into the mem address in r1.
    add r0, r0, #4  // Moves pointer by 4 bytes ie 1 word.
    add r1, r1, #4

    b copy_data

init_bss:
    // Load bss start and endpoint.
    ldr r0, =_sbss
    ldr r1, =_ebss
    mov r2, #0

zero_bss:

    cmp r0, r1
    bhs call_main

    str r2, [r0]
    add r0, r0, #4
    b zero_bss

call_main:
    bl main
    b .

.size Reset_Handler, . - Reset_Handler

// Every unused vector lands here and hangs: the watchdog resets the chip once it runs,
// before that the debugger shows the core stuck here.
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    b .

.size Default_Handler, . - Default_Handler


