/*
 * nrf52840.ld - Linker script for the nRF52840 (1 MB flash, 256 KB RAM).
 *
 * This tells the linker where code and data live in the chip's address space:
 *   - FLASH at 0x00000000 (executable, read-only). The vector table must sit at
 *     the very start because the CPU fetches the initial SP and reset vector
 *     from address 0 on power-up.
 *   - RAM   at 0x20000000 (read/write). Globals and the stack live here.
 *
 * The one subtlety worth understanding: initialised globals (.data) have their
 * *values* stored in flash (so they survive power-off) but must be *accessed*
 * in RAM. The linker records both addresses; the startup code copies flash->RAM
 * at boot. `AT> FLASH` is what places the load copy in flash while the run-time
 * address stays in RAM.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

/* Stack grows down from the top of RAM. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

SECTIONS
{
    /* Vector table first, at flash offset 0. */
    .vectors :
    {
        KEEP(*(.vectors))
    } > FLASH

    /* Code and read-only data. */
    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* Unwind tables emitted by GCC; keep them consistent even if unused. */
    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > FLASH

    /* Flash address the startup code copies .data from. */
    _sidata = LOADADDR(.data);

    /* Initialised globals: run in RAM, load-image in flash. */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH

    /* Zero-initialised globals. */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Discard C++ exception frames we do not use. */
    /DISCARD/ :
    {
        *(.ARM.extab*)
    }
}
