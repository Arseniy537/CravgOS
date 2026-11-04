ENTRY(_start16)
SECTIONS
{
    . = 0x7C00;
    .text   : { *(.boot) *(.text*) }
    .rodata : { *(.rodata*) *(.rdata*) }
    .data   : { *(.data*) }
    __bss_start = .;
    .bss    : { *(COMMON) *(.bss) }
    __bss_end = .;
    /* большие буферы (страницы, HTTP) лежат выше 2 МБ и в образ не попадают */
    .bigbss 0x200000 (NOLOAD) : { __bigbss_start = .; *(.bss.xbig) __bigbss_end = .; }
    /DISCARD/ : { *(.eh_frame) *(.note*) *(.comment) *(.pdata) *(.xdata) }
}
