
.syntax unified

.cpu cortex-m33
.thumb

// Creates a named section in memory called .isr_vector: a = allocation, %progbits says this section has content.
.section .isr_vector, "a", %progbits

.type isr_vector, %object

// Cortex-M33 core entries (PM0264), then IRQ n at entry 16 + n (RM0456 NVIC table).
// Stops at OTG_FS: an IRQ above 73 must not be enabled until the table is extended.
isr_vector:
    .word _estack           // 0: initial MSP
    .word Reset_Handler     // 1
    .word Default_Handler   // 2: NMI
    .word Default_Handler   // 3: HardFault
    .word Default_Handler   // 4: MemManage
    .word Default_Handler   // 5: BusFault
    .word Default_Handler   // 6: UsageFault
    .word Default_Handler   // 7: SecureFault
    .word 0                 // 8: reserved
    .word 0                 // 9: reserved
    .word 0                 // 10: reserved
    .word Default_Handler   // 11: SVCall
    .word Default_Handler   // 12: DebugMonitor
    .word 0                 // 13: reserved
    .word Default_Handler   // 14: PendSV
    .word Default_Handler   // 15: SysTick

    // IRQ 0-72: unused
    .rept 73
    .word Default_Handler
    .endr

    .word OTG_FS_IRQHandler // IRQ 73, offset 0x164 (usb.c)

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


