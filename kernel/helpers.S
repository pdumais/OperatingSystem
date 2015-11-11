.global getPageDescriptorAddress
.global memcpy64
.global memclear64
.global checksum_1complement
.global yield
.global getCurrentThreadID

#include "../memorymap.h"
#include "macros.h"
#include "tasks.h"

// threadID is physical address of stack
// Note that the PAGETABLE entry is changed on every context switch
getCurrentThreadID:
    mov     %cr3,%rax
    CLEANCR3ADDRESS(%rax)
    ret

// The problem with this function is that the task could switch anytime before the mov $1,(%rdi)
// so yielding would have been done and we do it again
yield:
    push    %rdi
    mov     $TIME_SLICE_COUNT,%rdi    // This variable is mapped in thread space. So multi-thread safe
    movq    $1,(%rdi)                 // next time we get scheduled, count will decrease to 0 and a switch will occur
1:  hlt
    //cmpq    $1,(%rdi)  if another int triggered, hlt will return. Avoid that
    //je      1b
    pop     %rdi
    ret


/*
    returns address of page directory entry associated with virtual address passed in %rdi
*/
getPageDescriptorAddress:
    mov     %rdi,%rax
    sub     $KERNEL_RESERVED_END,%rax // because PAGETABLES maps memory after KERNEL_RESERVED_END only
    shr     $12,%rax    /* divide by 4k, then mul by 8bytes per entry = >>12, <<3 */ 
    shl     $3,%rax    /* divide by 4k, then mul by 8bytes per entry = >>12, <<3 */ 
    add     $PAGETABLES,%rax
    ret


// rdi = source, rsi = destination, rdx = size
memcpy64:
    push    %rcx
    xchg    %rdi,%rsi
    mov     %rdx,%rcx
    shr     $3,%rcx
    rep     movsq
    mov     %rdx,%rcx
    and     $0x07,%rcx
    rep     movsb
    pop     %rcx
    ret

// rdi = buffer, rsi = size
memclear64:
    push    %rcx
    mov     $0,%rax
    mov     %rsi,%rcx
    shr     $3,%rcx
    rep     stosq
    mov     %rsi,%rcx
    and     $0x07,%rcx
    rep     stosb
    pop     %rcx
    ret


// rdi= buffer, rsi=size. return: rax will contain the big-endian checksum
checksum_1complement:
    push    %rbx
    push    %rcx
    mov     %rsi,%rcx
    shr     $1,%rcx     // we work on 16bit words, so divide byte count by two.
    mov     $0,%rax
    mov     $0,%rbx
1:  movw   (%rdi),%bx
    xchg    %bl,%bh
    add     %rbx,%rax
    add     $2,%rdi
    loop    1b
2:  mov     %rax,%rbx
    shr     $16,%rbx
    jz      sumdone
    and     $0xFFFF,%rax
    add     %rbx,%rax
    jmp     2b
sumdone:
    not     %rax
    xchg    %ah,%al
    pop     %rcx
    pop     %rbx
    ret


