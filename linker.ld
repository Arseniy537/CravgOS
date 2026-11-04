ENTRY(_start)
SECTIONS
{
    . = 1M;
    .text   : ALIGN(4K) { *(.multiboot) *(.text*) }
    .rodata : ALIGN(4K) { *(.rodata*) *(.rdata*) }
    .data   : ALIGN(4K) { *(.data*) }
    .bss    : ALIGN(4K) { *(COMMON) *(.bss*) }
    /DISCARD/ : { *(.eh_frame) *(.note*) *(.comment) *(.pdata) *(.xdata) }
}
