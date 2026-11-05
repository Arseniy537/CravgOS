.set MAGIC,    0x1BADB002
.set FLAGS,    0
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .text
.code32
.global _start
_start:
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

    # 2. Таблицы страниц 0x1000..0x7000 (4 ГБ)
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

    # 5. EFER.LME
    movl $0xC0000080, %ecx
    rdmsr
    orl  $(1 << 8), %eax
    wrmsr

    # 6. CR0.PG
    movl %cr0, %eax
    orl  $0x80000001, %eax
    movl %eax, %cr0

    # 7. GDT64
    lgdt gdt64_mb_desc
    ljmpl $0x08, $mb_long_entry

no_lm_mb:
1:  hlt
    jmp 1b

.align 8
gdt64_mb:
    .quad 0
    .quad 0x00209A0000000000
    .quad 0x0000920000000000
gdt64_mb_desc:
    .word . - gdt64_mb - 1
    .long gdt64_mb

.code64
mb_long_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
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

    call kmain
2:  cli
    hlt
    jmp 2b
