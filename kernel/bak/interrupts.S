#include "../memorymap.h"
#define PUSHA   push    %rax; \
    push    %rbx; \
    push    %rcx; \
    push    %rdx; \
    push    %rdi; \
    push    %rsi; \
    push    %r8; \
    push    %r9; \
    push    %r10; \
    push    %r11; \
    push    %r12; \
    push    %r13; \
    push    %r14; \
    push    %r15

#define POPA    pop %r15; \
    pop %r14; \
    pop %r13; \
    pop %r12; \
    pop %r11; \
    pop %r10; \
    pop %r9; \
    pop %r8; \
    pop %rsi; \
    pop %rdi; \
    pop %rdx; \
    pop %rcx; \
    pop %rbx; \
    pop %rax


.global setupIDT
.global registerIRQ

.extern writeNumberUnsafe
.extern scheduler
.extern enableIRQ

// %rdi = address of handler, %rsi = IRQ number
registerIRQ:
    push    %rsi
    mov     $IRQHandlers,%rax
    shl     $3,%rsi             // mul by 8
    add     %rsi,%rax
    mov     %rdi,(%rax)
    pop     %rsi
    mov     %rsi,%rdi
    and     $0xFF,%rdi
    or      $0x100,%rdi     // enable
    call    enableIRQ
    ret


setupIDT:
    push    %rax
    push    %rbx
    push    %rcx
    push    %rdi

    mov     $IDTSPACE,%rdi
    mov     $32,%rcx
    mov     $InterruptHandlers,%rax
prepareExceptions:
    mov     (%rax),%rbx
    movw    %bx,(%rdi)
    movw    $0x18,2(%rdi)
    mov     %rbx,4(%rdi)
    movw    $0x8F00,4(%rdi)
    movl    $0,12(%rdi)
    add     $16,%rdi
    add     $8,%rax
    loop    prepareExceptions    

    mov     $16,%ecx
    mov     $IRQs,%rax
prepareIRQs:
    mov     (%rax),%rbx
    movw    %bx,(%rdi)
    movw    $0x18,2(%rdi)
    mov     %rbx,4(%rdi)
    movw    $0x8E00,4(%rdi)
    movl    $0,12(%rdi)
    add     $16,%rdi
    add     $8,%rax
    loop    prepareIRQs


    //setup int 40h
    mov     $IDTSPACE,%rdi
    add     $0x400,%rdi         /*40h * 16 bytes*/
    mov     $yieldHandler,%rbx
    movw    %bx,(%rdi)
    movw    $0x18,2(%rdi)
    mov     %rbx,4(%rdi)
    movw    $0x8E00,4(%rdi)
    movl    $0,12(%rdi)


    // Now load the IDT register
    mov     $IDTRegister,%rax
    lidt   (%rax)

    pop     %rdi
    pop     %rcx
    pop     %rbx
    pop     %rax
    ret


.align 4
IDTRegister:
    .WORD 0x1000
    .QUAD IDTSPACE


.align 4
InterruptHandlers:
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad int3Handler
    .quad defaultHandler
    .quad defaultHandler
    .quad int6Handler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad exceptionDHandler
    .quad exceptionEHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler

// These are the handler addresses stored in the IDT
IRQs:
    .quad schedulerHandler
    .quad irq1Handler
    .quad irq2Handler
    .quad irq3Handler
    .quad irq4Handler
    .quad irq5Handler
    .quad irq6Handler
    .quad irq7Handler
    .quad irq8Handler
    .quad irq9Handler
    .quad irq10Handler
    .quad irq11Handler
    .quad irq12Handler
    .quad irq13Handler
    .quad irq14Handler
    .quad irq15Handler


// These are for the dynamically assigned IRQs for device drivers
IRQHandlers:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0

schedulerHandler:
    // Do we need to ignore this because there was a yield last time?
    btr     $0,ignoreNextIRQ0
    jnc     1f
    push    %rax
    mov     $0x20,%al   // we will no schedule, but we still need to send EOI
    outb    %al,$0x20
    pop     %rax
    iretq
1:  stc     /* set the carry flag to indicate that this was invoked because of IRQ0*/
    jmp     scheduler

yieldHandler:
    movq    $1,ignoreNextIRQ0   // ignore next scheduling so that next thread has at least one full time slice
    clc     /* clear the carry flag to indicate that this was invoked because of int 40h*/
    jmp     scheduler

ignoreNextIRQ0: .QUAD 0


defaultHandler:
    mov     $defaultText,%rdi
    call    writeStringUnsafe
   
    pop     %rax    /*Error code*/ 
    pop     %rdi    /* RIP */
    call    writeNumberUnsafe
    a1: jmp     a1
defaultText: .ASCIZ "Unimplemented exception handler. RIP="

exceptionDHandler:
    mov     $txt1,%rdi
    call    writeStringUnsafe

    pop     %rdi    /* error */
    call    writeNumberUnsafe
    pop     %rdi    /* RIP */
    call    writeNumberUnsafe
    1: jmp     1b
txt1: .ASCIZ "General Protection Failure. RIP="

exceptionEHandler:
    mov     $txt2,%rdi
    call    writeStringUnsafe

    pop     %rdi    /* error */
    call    writeNumberUnsafe
    pop     %rdi    /* RIP */
    call    writeNumberUnsafe
    1: jmp     1b
txt2: .ASCIZ "Page Fault. RIP="


int3Handler:
    push    %rax
    push    %rdi
    mov     $int3text,%rdi
    call    writeStringUnsafe
    pop     %rdi
    pop     %rax
    1:jmp 1b
    iretq
int3text: .ASCIZ "INT 3"

int6Handler:
    push    %rax
    push    %rdi
    mov     $int6text,%rdi
    call    writeStringUnsafe
    pop     %rdi
    pop     %rax
    1:jmp 1b
    iretq
int6text: .ASCIZ "Invalid Opcode"



irq1Handler:
    push    %rax
    mov     $0x20,%al
    outb    %al,$0x20
    pop     %rax
    iretq

irq2Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $16,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0x20
    POPA
    iretq
irq3Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $24,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0x20
    POPA
    iretq
irq4Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $32,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0x20
    POPA
    iretq
irq5Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $40,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0x20
    POPA
    iretq
irq6Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $48,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0x20
    POPA
    iretq
irq7Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $56,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0x20
    POPA
    iretq
irq8Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $64,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq
irq9Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $72,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq
irq10Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $80,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq
irq11Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $88,%rdi        
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq
irq12Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $96,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq
irq13Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $104,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq
irq14Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $112,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq
irq15Handler:
    PUSHA
    mov     $IRQHandlers,%rdi
    add     $120,%rdi
    mov     (%rdi),%rdi
    call    %rdi
    mov     $0x20,%al
    outb    %al,$0xa0
    outb    %al,$0x20
    POPA
    iretq


