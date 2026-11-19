# Загрузчик для El Torito (no-emulation): BIOS грузит весь образ на 0x7C00
# Переход: 16-bit Real Mode -> 32-bit Protected Mode -> 64-bit Long Mode (IA-32e)
.code16
.section .boot, "ax"
.global _start16
_start16:
    .byte 0xeb, 0x05
boot_lba:     .word 23
boot_sectors: .word 160
boot_drive:   .byte 0

loader_init:
    cli
    ljmp $0, $1f
1:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp
    sti

    movb %dl, (boot_drive)

    # Релоцируем загрузчик первой стадии в 0x0600
    movw $0x7C00, %si
    movw $0x0600, %di
    movw $256, %cx
    cld
    rep movsw

    # Дальний переход на исполнение из 0x0600
    ljmp $0, $(0x0600 + (relocated - _start16))

relocated:
    # Загружаем все 2048-байтные сектора cravgos.img с CD в 0x7C00..
    movw (0x0600 + (boot_lba - _start16)), %ax
    movw %ax, (0x0600 + (dap_lba - _start16))
    movw $0x07C0, (0x0600 + (dap_seg - _start16))
    movw (0x0600 + (boot_sectors - _start16)), %cx

load_loop:
    pushw %cx
    movb  $0x42, %ah
    movb  (0x0600 + (boot_drive - _start16)), %dl
    movw  $(0x0600 + (dap - _start16)), %si
    int   $0x13
    jc    load_fail
    addw  $0x80, (0x0600 + (dap_seg - _start16))
    incw  (0x0600 + (dap_lba - _start16))
    popw  %cx
    loop  load_loop

    # Все сектора загружены по адресу 0x7C00+ без переполнений BIOS
    ljmp $0, $stage2_start

load_fail:
    movw $0xB800, %ax
    movw %ax, %es
    movw $0x4F45, %es:0    # 'E'
    movw $0x4F52, %es:2    # 'R'
    movw $0x4F52, %es:4    # 'R'
1:  hlt
    jmp 1b

.align 4
dap:
    .byte 0x10          # размер структуры DAP (16 байт)
    .byte 0             # зарезервировано (0)
    .word 1             # читать по 1 сектору (2048 байт)
    .word 0             # смещение буфера (всегда 0)
dap_seg:
    .word 0x07C0        # сегмент буфера (начиная с 0x07C0 = 0x7C00)
dap_lba:
    .quad 23            # 64-битный начальный LBA

stage2_start:
    movw $0x0003, %ax        # текстовый режим 80x25
    int  $0x10

    # Загружаем кириллический шрифт через BIOS INT 10h, AX=1100h
    pushw %es
    xorw  %ax, %ax
    movw  %ax, %es
    movw  $cyr_font, %bp
    movw  $0x1100, %ax
    movb  $16, %bh          # 16 байт на глиф
    movb  $0, %bl           # блок шрифта 0
    movw  $48, %cx          # 48 глифов (0x80..0xAF)
    movw  $0x0080, %dx      # начиная с 0x80
    int   $0x10

    movw  $(cyr_font + 768), %bp
    movw  $0x1100, %ax
    movb  $16, %bh
    movb  $0, %bl
    movw  $24, %cx          # 24 глифа (0xE0..0xF7)
    movw  $0x00E0, %dx      # начиная с 0xE0
    int   $0x10
    popw  %es

    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp
    sti

    # Пиксельный режим по запросу: одноразовый флаг в CMOS (регистр 0x7D = 'W'), его ставит команда wmboot.
    # Флаг сбрасывается ДО попытки: если что-то пойдёт не так, следующая загрузка снова текстовая.
    movb $0x7D, %al
    outb %al, $0x70
    inb  $0x71, %al
    cmpb $0x57, %al
    jne  1f
    movb $0x7D, %al
    outb %al, $0x70
    xorb %al, %al
    outb %al, $0x71
    call vbe_setup
1:
    cli

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

# VBE: найти линейный 32-битный режим 1024x768 (иначе 800x600, 640x480), включить его, записать fbinfo
# и скопировать 8x16 шрифт BIOS (для текстовой консоли поверх пикселей). DS = ES = 0 на входе и на выходе.
vbe_setup:
    pushal
    pushw %fs
    movw  $0x1130, %ax              # адрес 8x16 ROM-шрифта: ES:BP
    movb  $6, %bh
    int   $0x10
    pushw %ds
    movw  %es, %ax
    movw  %ax, %ds
    movw  %bp, %si
    xorw  %ax, %ax
    movw  %ax, %es
    movw  $romfont, %di
    movw  $2048, %cx
    cld
    rep movsw
    popw  %ds
    xorw  %ax, %ax
    movw  %ax, %es
    movw  $vbe_buf, %di             # информация о контроллере
    movl  $0x32454256, (%di)        # "VBE2"
    movw  $0x4F00, %ax
    int   $0x10
    cmpw  $0x004F, %ax
    jne   vbe_out
    cmpl  $0x41534556, vbe_buf      # "VESA"
    jne   vbe_out
    movw  vbe_buf + 0x10, %ax       # сегмент списка режимов
    movw  %ax, vbe_seg
    movw  vbe_buf + 0x0E, %ax       # смещение списка режимов
    movw  %ax, vbe_lst
    movw  $vbe_want, vbe_wp
vbe_want_loop:
    movw  vbe_wp, %bx
    movw  (%bx), %ax
    testw %ax, %ax
    jz    vbe_out                   # кандидаты кончились
    movw  %ax, vbe_wx
    movw  2(%bx), %ax
    movw  %ax, vbe_wy
    addw  $4, %bx
    movw  %bx, vbe_wp
    movw  vbe_lst, %si              # начало списка режимов
vbe_scan:
    movw  vbe_seg, %ax
    movw  %ax, %fs
    movw  %fs:(%si), %cx
    cmpw  $0xFFFF, %cx
    je    vbe_want_loop
    addw  $2, %si
    movw  %si, vbe_si
    movw  %cx, vbe_mode
    xorw  %ax, %ax
    movw  %ax, %es
    movw  $vbe_mi, %di
    movw  $0x4F01, %ax
    int   $0x10
    cmpw  $0x004F, %ax
    jne   vbe_resume
    movb  vbe_mi, %al               # поддерживается, графический, линейный буфер: биты 0, 4, 7
    andb  $0x91, %al
    cmpb  $0x91, %al
    jne   vbe_resume
    cmpb  $32, vbe_mi + 0x19        # 32 бита на пиксель
    jne   vbe_resume
    cmpb  $6, vbe_mi + 0x1B         # direct color
    jne   vbe_resume
    movw  vbe_wx, %ax
    cmpw  %ax, vbe_mi + 0x12
    jne   vbe_resume
    movw  vbe_wy, %ax
    cmpw  %ax, vbe_mi + 0x14
    je    vbe_found
vbe_resume:
    movw  vbe_si, %si
    jmp   vbe_scan
vbe_found:
    movl  $0x31494246, fbinfo       # "FBI1"
    movl  vbe_mi + 0x28, %eax
    movl  %eax, fbinfo + 4
    movw  vbe_mi + 0x12, %ax
    movw  %ax, fbinfo + 8
    movw  vbe_mi + 0x14, %ax
    movw  %ax, fbinfo + 10
    movw  vbe_mi + 0x10, %ax
    movw  %ax, fbinfo + 12
    movb  vbe_mi + 0x19, %al
    movb  %al, fbinfo + 14
    movb  vbe_mi + 0x20, %al
    movb  %al, fbinfo + 15
    movb  vbe_mi + 0x22, %al
    movb  %al, fbinfo + 16
    movb  vbe_mi + 0x24, %al
    movb  %al, fbinfo + 17
    movw  vbe_mode, %bx
    orw   $0x4000, %bx              # линейный буфер
    movw  $0x4F02, %ax
    int   $0x10
    cmpw  $0x004F, %ax
    je    vbe_out
    movl  $0, fbinfo                # режим не включился: остаёмся в тексте
vbe_out:
    xorw  %ax, %ax
    movw  %ax, %es
    popw  %fs
    popal
    ret

.align 4
.global fbinfo
fbinfo:  .space 32                  # 0 magic "FBI1", 4 база, 8 ширина, 10 высота, 12 pitch, 14 bpp, 15..17 позиции R,G,B
.global romfont
romfont: .space 4096                # копия 8x16 шрифта BIOS (256 глифов по 16 байт)
vbe_buf: .space 512
vbe_mi:  .space 256
vbe_want: .word 1024, 768, 800, 600, 640, 480, 0, 0
vbe_wx:   .word 0
vbe_wy:   .word 0
vbe_si:   .word 0
vbe_wp:   .word 0
vbe_mode: .word 0
vbe_seg:  .word 0
vbe_lst:  .word 0

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
    movq $0x800000, %rsp
    subq $40, %rsp
    cld

    # Включаем FPU и SSE (CR0.EM=0, CR0.MP=1, CR4.OSFXSR=1, CR4.OSXMMEXCPT=1)
    movq %cr0, %rax
    andq $~0x04, %rax        # сбрасываем CR0.EM
    orq  $0x02, %rax         # устанавливаем CR0.MP
    movq %rax, %cr0
    movq %cr4, %rax
    orq  $(3 << 9), %rax     # CR4.OSFXSR (бит 9) и CR4.OSXMMEXCPT (бит 10)
    movq %rax, %cr4
    fninit                   # сброс/инициализация FPU

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

.include "cyrfont.inc"
