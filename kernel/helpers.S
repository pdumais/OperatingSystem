.global getPageDescriptorAddress
.global memcpy64
.global memclear64
.global checksum_1complement
.global yield
.global getCurrentThreadID
.global strcpy
.global atomic_increase_within_range
.global tcp_checksum
.global atomic_cmpxchg
.global atomic_cmpxchg32
.global atomic_set

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


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// tcp_checksum(rdi= buffer, rsi=bufsize, rdx=src, rcx=dst)
// The buffer must contain the tcp header and tcp data
// dst and src must be in bigendian
// return: rax will contain the big-endian checksum
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
tcp_checksum:
    push    %rbx
    push    %rcx
    push    %r8
    push    %r9

    mov     %rcx,%r9
    mov     %rsi,%r8

    mov     %rsi,%rcx
    inc     %rcx        // round up to next 2bytes
    shr     $1,%rcx     // we work on 16bit words, so divide byte count by two.
    mov     $0,%rax
    mov     $0,%rbx
1:  movw    (%rdi),%bx
    xchg    %bl,%bh
    add     %rbx,%rax
    add     $2,%rdi
    loop    1b

    // now add the pseudo header
    add     %rdx,%rax
    add     %r9,%rax
//    bswap   %r8
  //  shr     $48,%r8
    add     %r8,%rax
    add     $0x06,%rax

2:  mov     %rax,%rbx
    shr     $16,%rbx
    jz      3f
    and     $0xFFFF,%rax
    add     %rbx,%rax
    jmp     2b
3:  not     %rax
    xchg    %ah,%al

    pop     %r9
    pop     %r8
    pop     %rcx
    pop     %rbx
    ret

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// strcpy(rdi=source, rsi=destination)
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
strcpy:
    push        %rsi
    push        %rdi
    xchg        %rsi,%rdi

1:  movsb
    cmp         $0,(%rsi)
    jne         1b

    pop         %rdi
    pop         %rsi
    ret
    
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// atomic_increase_within_range(rdi=var_addr,rsi=start,rdx=end)
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
atomic_increase_within_range:
    push        %rbx
1:  mov         (%rdi),%rax
    mov         %rax,%rbx
    inc         %rbx
    cmp         %rdx,%rbx
    jb          2f
    mov         %rsi,%rbx
2:  lock cmpxchgq %rbx,(%rdi)
    jnz         1b
    mov         %rbx,%rax
    pop         %rbx
    ret 

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// atomic_set(rdi=var_addr,rsi=bit)
// Returns 1 if bit was already set, 0 if it was cleared
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
atomic_set:
    xor         %rax,%rax
    lock bts    %rsi,(%rdi)
    setc        %al
    ret

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// atomic_cmpxchg(rdi=var_addr,rsi=value,rdx=oldvalue)
// returns 1 if success
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
atomic_cmpxchg:
    mov         %rdx,%rax
    xor         %rdx,%rdx
    lock cmpxchgq %rsi,(%rdi)
    setz        %dl
    mov         %rdx,%rax
    ret

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
// atomic_cmpxchg32(rdi=var_addr,rsi=value,rdx=oldvalue)
// returns 1 if success
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
atomic_cmpxchg32:
    mov         %rdx,%rax
    xor         %rdx,%rdx
    lock cmpxchg %esi,(%rdi)
    setz        %dl
    mov         %rdx,%rax
    ret
