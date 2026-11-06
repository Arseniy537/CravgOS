ENTRY(_start)
SECTIONS
{
    . = 1M;
    .text   : ALIGN(4K) { *(.multiboot) *(.boot_font) *(.text*) }
    .rodata : ALIGN(4K) { *(.rodata*) *(.rdata*) }
    .data   : ALIGN(4K) { *(.data*) }
    __bss_start = .;
    .bss    : ALIGN(4K) { *(COMMON) *(.bss) }
    __bss_end = .;
    .bigbss 0x200000 (NOLOAD) : { __bigbss_start = .; *(.bss.xbig) __bigbss_end = .; }
    /DISCARD/ : { *(.eh_frame) *(.note*) *(.comment) *(.pdata) *(.xdata) }
}
