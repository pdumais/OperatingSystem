# 1 "bootloader.S"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 1 "<command-line>" 2
# 1 "bootloader.S"



# 1 "../memorymap.h" 1
# 5 "bootloader.S" 2

.include "../sizekernel.inc"

.CODE16
.ORG 0

.EQU HEAP, 0x0500
.EQU STACK_END,0x7C00
.EQU STACKSEG, 0
.EQU KERNEL_BASE_ADDR, 0x00100000

start:
 .BYTE 0xEA
 .WORD 5
 .WORD 0x07C0

.org 5
main:
 cli

 pushw $STACKSEG
 popw %ss
 mov $STACK_END,%sp
    mov %sp,%bp



    push %cs
    pop %ds
    pushw $0
    pop %es
    mov $0x00001000,%di
    mov $JUMPCODETOAP,%si
    mov $8,%cx
    rep movsb

    mov $0,%ax
    mov %ax,%gs


 call a20wait
 mov $0xAD,%al
 out %al,$0x64
 call a20wait
 mov $0xD0,%al
 out %al,$0x64
 call a20wait2
 in $0x60,%al
 push %eax
 call a20wait
 mov $0xD1,%al
 out %al,$0x64
 call a20wait
 pop %eax
 or $2,%al
 out %al,$0x60
 call a20wait
 mov $0xAE,%al
 out %al,$0x64
 call a20wait


 push %cs
 pop %ds
    mov $GDTINFO,%eax
    lgdtl (%eax)
    mov %cr0,%eax
    or $1,%al
    mov %eax,%cr0
    mov $0x08,%bx
    mov %bx,%fs
    and $0xFE,%al
    mov %eax,%cr0


    movl $0x1FA,%eax
    movl (%eax),%ecx
    shrl $9,%ecx
    incl %ecx
    xorl %ebx,%ebx

    mov $KERNEL_BASE_ADDR,%edi
readNextSector:
    push %edi
    movl $DAP,%esi
    movl %ebx,%eax
    addl $3,%eax
    movl %eax,8(%esi)
    mov $0x42,%ah
    mov $0x80,%dl
    int $0x13


    pop %edi
    mov $HEAP,%esi
    push %ecx
    mov $512,%ecx
copykernel:
    mov %gs:(%esi),%al
    mov %al,%fs:(%edi)
    inc %edi
    inc %esi
    loop copykernel
    pop %ecx

    incl %ebx
    cmpl %ebx,%ecx
    jne readNextSector


 mov %cr0,%eax
 or $1,%al
    mov %eax,%cr0



    ljmpl $0x10,$KERNEL_BASE_ADDR

a20wait:
    in $0x64,%al
    test $2,%al
    jnz a20wait
    ret
a20wait2:
    in $0x64,%al
    test $1,%al
    jz a20wait2
    ret

printchar:
    pushl %ebx
    pushl %ecx
    mov $0x09,%ah
    mov $0x0004,%bx
    mov $10,%cx
    int $0x10
    popl %ecx
    popl %ebx
    ret



apmain:
    cli

    mov %cs,%ax
    mov %ax,%ds
    mov $GDTINFO,%eax
    lgdtl (%eax)
    mov %cr0,%eax
    or $1,%al
    mov %eax,%cr0
    ljmpl $0x10,$KERNEL_BASE_ADDR





.align 4
JUMPCODETOAP:
    .BYTE 0x90
    .BYTE 0x90
    .BYTE 0x90
 .BYTE 0xEA
 .WORD apmain
 .WORD 0x07C0
.align 4
DAP:
    .BYTE 0x10
    .BYTE 0x00
    .WORD 0x01
    .WORD HEAP
    .WORD 0
    sector: .LONG 0x02
    .LONG 0x00

.align 4
GDTINFO:

    .WORD 0x20
    .LONG . + 0x7C04


    .LONG 00
    .LONG 00


    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10010010
    .BYTE 0b11001111
    .BYTE 0x00


    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10011010
    .BYTE 0b11001111
    .BYTE 0x00
# 218 "bootloader.S"
    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10011010
    .BYTE 0b10101111
    .BYTE 0x00


.ORG 0x01FA
    .LONG KERNEL_SIZE
.ORG 0x01FE
    .BYTE 0x55
    .BYTE 0xAA

.org 0x200
