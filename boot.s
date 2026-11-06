/* boot.s - Загрузчик для GRUB Multiboot 1 & Multiboot 2 с переходом в 64-битный Long Mode */

.set MB1_MAGIC,    0x1BADB002
.set MB1_FLAGS,    0x00000003  /* 4KB align modules, memory info */
.set MB1_CHECKSUM, -(MB1_MAGIC + MB1_FLAGS)

.section .multiboot, "a"
.align 4
/* Multiboot 1 header */
.long MB1_MAGIC
.long MB1_FLAGS
.long MB1_CHECKSUM

.align 8
/* Multiboot 2 header */
multiboot2_header_start:
    .long 0xe85250d6                        /* magic number */
    .long 0                                 /* architecture: 0 = i386 32-bit protected mode */
    .long multiboot2_header_end - multiboot2_header_start /* header length */
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header_start)) /* checksum */

    /* Tag: Information request (optional) */
    .align 8
    /* Tag: End tag */
    .short 0
    .short 0
    .long 8
multiboot2_header_end:

.section .text
.code32
.global _start
.global _start16
_start:
_start16:
    cli
    movl $0x90000, %esp

    # 1. Проверяем поддержку Long Mode
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_lm_mb
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz no_lm_mb

    # 2. Таблицы страниц 0x1000..0x7000 (отображаем первые 4 ГБ через 2 МБ Huge Pages)
    movl $0x1000, %edi
    movl $6144, %ecx
    xorl %eax, %eax
    rep stosl

    movl $0x2003, 0x1000
    movl $0x3003, 0x2000
    movl $0x4003, 0x2008
    movl $0x5003, 0x2010
    movl $0x6003, 0x2018

    movl $0x3000, %edi
    movl $0x83, %eax
    movl $2048, %ecx
1:  movl %eax, (%edi)
    movl $0, 4(%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop 1b

    # 3. CR4.PAE
    movl %cr4, %eax
    orl  $0x20, %eax
    movl %eax, %cr4

    # 4. CR3 = 0x1000
    movl $0x1000, %eax
    movl %eax, %cr3

    # 5. EFER.LME (Long Mode Enable)
    movl $0xC0000080, %ecx
    rdmsr
    orl  $(1 << 8), %eax
    wrmsr

    # 6. CR0.PG + CR0.PE
    movl %cr0, %eax
    orl  $0x80000001, %eax
    movl %eax, %cr0

    # 7. Загружаем 64-битный GDT и переходим в Long Mode
    lgdt gdt64_mb_desc
    ljmpl $0x08, $mb_long_entry

no_lm_mb:
1:  hlt
    jmp 1b

.align 8
gdt64_mb:
    .quad 0
    .quad 0x00209A0000000000   /* 0x08: 64-bit code segment (L=1, D=0) */
    .quad 0x0000920000000000   /* 0x10: 64-bit data segment */
gdt64_mb_desc:
    .word . - gdt64_mb - 1
    .long gdt64_mb

.code64
mb_long_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs
    movq $0x800000, %rsp
    cld

    # Включаем FPU и SSE (CR0.EM=0, CR0.MP=1, CR4.OSFXSR=1, CR4.OSXMMEXCPT=1)
    movq %cr0, %rax
    andq $~0x04, %rax
    orq  $0x02, %rax
    movq %rax, %cr0
    movq %cr4, %rax
    orq  $(3 << 9), %rax
    movq %rax, %cr4
    fninit

    # Обнуляем .bss
    movq $__bss_start, %rdi
    movq $__bss_end, %rcx
    subq %rdi, %rcx
    xorl %eax, %eax
    rep stosb

    # Обнуляем .bigbss
    movq $__bigbss_start, %rdi
    movq $__bigbss_end, %rcx
    subq %rdi, %rcx
    xorl %eax, %eax
    rep stosb

    call kmain
2:  cli
    hlt
    jmp 2b

# ---- 64-битные обработчики исключений процессора ----
.macro ISR_NOERR v
isr_mb\v:
    pushq $0                 # фиктивный error code
    pushq $\v                # номер вектора
    jmp isr_common_mb
.endm
.macro ISR_ERR v
isr_mb\v:
    pushq $\v                # номер вектора (error code уже в стеке)
    jmp isr_common_mb
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
isr_generic_mb:
    pushq $0
    pushq $255
    jmp isr_common_mb

isr_common_mb:
    cld
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdi
    pushq %rsi
    pushq %rbp
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    movq %rsp, %rdi
    movq %rsp, %rcx
    movq $exc_stack_top_mb, %rsp
    subq $40, %rsp
    call exception_handler
3:  hlt
    jmp 3b

.section .rodata
.align 8
.global isr_table
isr_table:
    .quad isr_mb0, isr_mb1, isr_mb2, isr_mb3, isr_mb4, isr_mb5, isr_mb6, isr_mb7
    .quad isr_mb8, isr_mb9, isr_mb10, isr_mb11, isr_mb12, isr_mb13, isr_mb14, isr_mb15
    .quad isr_mb16, isr_mb17, isr_mb18, isr_mb19, isr_mb20, isr_mb21, isr_mb22, isr_mb23
    .quad isr_mb24, isr_mb25, isr_mb26, isr_mb27, isr_mb28, isr_mb29, isr_mb30, isr_mb31
    .quad isr_generic_mb

.section .bss
.align 16
exc_stack_mb:
    .skip 8192
exc_stack_top_mb:

.include "cyrfont.inc"
