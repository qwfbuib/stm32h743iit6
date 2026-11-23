
/* SPDX-License-Identifier: Apache-2.0 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2M
    AXI_SRAM (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
}

_estack = ORIGIN(AXI_SRAM) + LENGTH(AXI_SRAM);
__heap_end = _estack - 4K;

SECTIONS
{
    .isr_vector ORIGIN(FLASH) :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.glue_7 .glue_7t)
    } > FLASH

    .msh_commands : ALIGN(4)
    {
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;
    } > FLASH

    .rt_init : ALIGN(4)
    {
        __rt_init_start = .;
        KEEP(*(SORT(.rti_fn*)))
        __rt_init_end = .;
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > AXI_SRAM AT> FLASH
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) : ALIGN(8)
    {
        _sbss = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > AXI_SRAM

    __heap_start = _ebss;
}

ASSERT(__heap_start + 8192 <= __heap_end, "Insufficient AXI SRAM for RT-Thread heap")
ASSERT((ADDR(.isr_vector) & 0x3ff) == 0, "Misaligned vector table")
