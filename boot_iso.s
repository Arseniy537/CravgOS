# Загрузчик для El Torito (no-emulation): BIOS грузит весь образ на 0x7C00
# Переход: 16-bit Real Mode -> 32-bit Protected Mode -> 64-bit Long Mode (IA-32e)
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
    movw $0x7C00, %sp

    # Включаем A20 (Fast A20 Gate)
    inb  $0x92, %al
    orb  $2, %al
    andb $0xFE, %al
    outb %al, $0x92

    # Загружаем временный 32-битный GDT и переходим в Protected Mode
    lgdt gdt32_desc
    movl %cr0, %eax
    orl  $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $pm_entry

.align 8
gdt32:
    .quad 0
    .quad 0x00CF9A000000FFFF # 32-bit code: base=0, limit=4G, exec/read
    .quad 0x00CF92000000FFFF # 32-bit data: base=0, limit=4G, read/write
gdt32_desc:
    .word . - gdt32 - 1
    .long gdt32

.code32
pm_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $0x90000, %esp

    # 1. Проверяем поддержку Extended CPUID и Long Mode (LM bit 29 в EDX)
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx   # Long Mode (LM) bit
    jz no_long_mode

    # 2. Настраиваем 4-уровневые таблицы страниц (Paging) для отображения первых 4 ГБ
    # Таблицы страниц размещаем в памяти от 0x1000 до 0x7000 (6 страниц по 4 КБ):
    #   0x1000: PML4 (1 страница)
    #   0x2000: PDPT (1 страница)
    #   0x3000..0x6FFF: 4 Page Directories (PD0, PD1, PD2, PD3 по 1 ГБ каждая через 2 МБ Huge Pages)
    movl $0x1000, %edi
    movl $6144, %ecx         # 6 * 1024 dwords = 24 КБ
    xorl %eax, %eax
    rep stosl

    # PML4[0] -> PDPT (0x2000)
    movl $0x2003, 0x1000     # Present | Writable

    # PDPT[0..3] -> PD0 (0x3000), PD1 (0x4000), PD2 (0x5000), PD3 (0x6000)
    movl $0x3003, 0x2000
    movl $0x4003, 0x2008
    movl $0x5003, 0x2010
    movl $0x6003, 0x2018

    # Заполняем 4 PD по 512 записей 2МБ страницами (итого 2048 записей = 4 ГБ)
    movl $0x3000, %edi
    movl $0x83, %eax         # Base 0 | Present | Writable | Page Size (2MB)
    movl $2048, %ecx
1:
    movl %eax, (%edi)
    movl $0, 4(%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop 1b

    # 3. Включаем PAE в CR4 (бит 5)
    movl %cr4, %eax
    orl  $0x20, %eax
    movl %eax, %cr4

    # 4. Загружаем адрес PML4 (0x1000) в регистр CR3
    movl $0x1000, %eax
    movl %eax, %cr3

    # 5. Включаем Long Mode в EFER MSR (0xC0000080, бит 8 LME)
    movl $0xC0000080, %ecx
    rdmsr
    orl  $(1 << 8), %eax
    wrmsr

    # 6. Включаем Paging в CR0 (бит 31 PG, бит 0 PE)
    movl %cr0, %eax
    orl  $0x80000001, %eax
    movl %eax, %cr0

    # 7. Загружаем 64-битный GDT
    lgdt gdt64_desc

    # 8. Дальний переход в 64-битный Long Mode
    ljmpl $0x08, $long_entry

no_long_mode:
    movl $0xB8000, %edi
    movl $0x4F204F20, %eax
    movl $80, %ecx
    rep stosl
1:  hlt
    jmp 1b

.align 8
gdt64:
    .quad 0
    # 0x08: 64-bit Code: Present=1, DPL=0, S=1, Code=1, Readable=1, L=1 (64-bit), D=0
    .quad 0x00209A0000000000
    # 0x10: 64-bit Data: Present=1, DPL=0, S=1, Data=1, Writable=1
    .quad 0x0000920000000000
gdt64_desc:
    .word . - gdt64 - 1
    .long gdt64

.code64
long_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movq $0x90000, %rsp
    cld

    # Обнуляем .bss
    movq $__bss_start, %rdi
    movq $__bss_end, %rcx
    subq %rdi, %rcx
    xorl %eax, %eax
    rep stosb

    call kmain
1:  cli
    hlt
    jmp 1b

# ---- 64-битные обработчики исключений процессора ----
.macro ISR_NOERR v
isr\v:
    pushq $0                 # фиктивный error code
    pushq $\v                # номер вектора
    jmp isr_common
.endm
.macro ISR_ERR v
isr\v:
    pushq $\v                # номер вектора (error code уже в стеке)
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
    pushq $0
    pushq $255
    jmp isr_common

isr_common:
    cld
    # Сохраняем все 15 64-битных регистров общего назначения
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

    # Передаём указатель на сохранённый кадр в %rdi и %rcx
    movq %rsp, %rdi
    movq %rsp, %rcx
    movq $exc_stack_top, %rsp
    subq $40, %rsp           # выравнивание 16 байт + shadow space
    call exception_handler
2:  hlt
    jmp 2b

.section .rodata
.align 8
.global isr_table
isr_table:
    .quad isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    .quad isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    .quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    .quad isr_generic

.section .bss
.align 16
exc_stack:
    .skip 8192
exc_stack_top:
