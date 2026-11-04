# Загрузчик для El Torito (no-emulation): BIOS грузит весь образ на 0x7C00
.code16
.section .boot, "ax"
.global _start16
_start16:
    movw $0x0003, %ax        # текстовый режим 80x25
    int  $0x10
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7000, %sp
    inb  $0x92, %al          # A20
    orb  $2, %al
    andb $0xFE, %al
    outb %al, $0x92
    lgdt gdt_desc
    movl %cr0, %eax
    orl  $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $pm_entry

.align 8
gdt:
    .quad 0
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt_end:
gdt_desc:
    .word gdt_end - gdt - 1
    .long gdt

.code32
pm_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $0x90000, %esp
    cld
    movl $__bss_start, %edi  # обнуляем .bss
    movl $__bss_end, %ecx
    subl %edi, %ecx
    xorl %eax, %eax
    rep stosb
    call kmain
1:  cli
    hlt
    jmp 1b

# ---- обработчики исключений процессора: вместо тройного сбоя показываем экран с причиной ----
.macro ISR_NOERR v
isr\v:
    pushl $0
    pushl $\v
    jmp isr_common
.endm
.macro ISR_ERR v
isr\v:
    pushl $\v
    jmp isr_common
.endm
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31
isr_generic:
    pushl $0
    pushl $255
    jmp isr_common
isr_common:
    cli
    cld
    movl %esp, %eax              # eax -> [vector, error, eip, cs, eflags]
    movl $exc_stack_top, %esp    # отдельный стек: исключение могло быть из-за стека
    pushl %eax
    call exception_handler
2:  hlt
    jmp 2b

.section .rodata
.align 4
.global isr_table
isr_table:
    .long isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    .long isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    .long isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .long isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    .long isr_generic

.section .bss
.align 16
exc_stack:
    .skip 4096
exc_stack_top:
