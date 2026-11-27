
/* Memory NAME (attributes ie r/w/x): ORIGIN = start address, LENGTH = size of the region NAME */
MEMORY
{
    /* RM0456 7.3.1 Table 52: two 1 MB banks back to back, ORIGIN address in chapter 4 table 25 */
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2048k

    /* RM0456 Chapter 2.3.2 for the SRAM block ORIGIN, LENGTH in 2.3.3 */
    RAM (rwx) :  ORIGIN = 0x20000000, LENGTH = 768k
}

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.rodata*)
        *(.text*)
        . = ALIGN(4);
    } > FLASH

    /* Unwind tables some libgcc/newlib objects bring in */
    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > FLASH

    /* Flash address of the .data initial values, copied to RAM by Reset_Handler */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}
