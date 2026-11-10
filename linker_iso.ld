ENTRY(_start16)
SECTIONS
{
    . = 0x7C00;
    .text   : { *(.boot) *(.boot_font) *(.text*) }
    .rodata : { *(.rodata*) *(.rdata*) }
    .data   : { *(.data*) }
    __bss_start = .;
    .bss    : { *(COMMON) *(.bss) }
    __bss_end = .;
    /* большие буферы (страницы, HTTP) лежат с 1 МБ и в образ не попадают */
    .bigbss 0x100000 : { __bigbss_start = .; KEEP(*(.bss.xbig)) __bigbss_end = .; }
    /DISCARD/ : { *(.eh_frame) *(.note*) *(.comment) *(.pdata) *(.xdata) }
}
ASSERT(__bss_end <= 0x90000, ".bss section exceeds 0x90000 (conventional memory limit)!");
