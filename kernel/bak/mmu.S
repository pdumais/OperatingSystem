#include "../memorymap.h"

.global allocateStackPage
.global allocateHeapPage

/*
Virtual addressing works like this:
    when application wants to access 0xABCDEF12, the CPU finds the proper entry in the page directory
    by dividing address by 4meg (because each page table addresses 4meg). And then it finds
    the page table by dividing the remainder by 4k. So basically: the first 10bits represents the page
    directory entry, the next 10 bits represent the entry in the page table and the remainder 12 bits
    represents the offset within that page
        [0-11]: offset
        [12-21]: page table entry
        [22-31]: page directory entry

    It would be possible that each process have similar mappings of memory. I.E: each process would have its
    base code segment start at 0x10000000 but that virtual address would obviously point to a different
    place in physical memory. But to do this, we must use a different page directory for each process
    because we want each process to have its own mapping. So we would need to modify the page directory pointer
    in cr3 on every task switch


DYNAMIC ALLOCATION:
    bits [9-11] in a page directory entry is available for user. We will use it as follor:
        0b000: physical memory pointed to by this page is free to use
        0b001: physical memory pointed to by this page is also mapped somewhere else and is a stack page for a thread
        0b010: physical memory pointed to by this page is used by the kernel
        0b011: physical memory pointed to by this page is used as heap for a thread or the kernel
        

*/



/*
        stack for each thread will use 0xFE000000 (last page in 4gig)
            that page must point to a physical location starting after 10meg
            so the page after 10meg will be marked as used.
                that means that that physical location will be mapped twice.
                but the page in the bottom is only used to know that that
                address is in use. We could set a flag saying that it is
                used at some other remapping.
            after each context switch, that page will have its physical
            location modified and the page will be invalidated.

*/
allocatePage:
    push    %rsi
    push    %rcx
    push    %rbx
    
    mov     $PDTABLE,%rsi
    sub     $8,%rsi
    mov     $2048,%rcx
searchFreePage:
    add     $8,%rsi
    mov     (%rsi),%rbx             // rbx contains the PD-e
    test    $0b111000000000,%rbx    // is the page free? check the 3 AVL bits, should be zero
    jnz     notFree
    mov     %rbx,%rax               //rax will be used by cmpxchg
    or      %rdi,%rbx               //rdi=reserve_type that will be set in the AVL bits
    lock cmpxchg %rbx,(%rsi)        //if rax==(esi), copy rbx in (esi). this is to make sure that
    jz     foundPage                //  (rsi) is still free. if not, then another thread reserved it
notFree:                            //  by the time we got here. Z flag will be set, so find a new page.
    loop    searchFreePage
    mov     $0,%rax
foundPage:
    mov     $0x000FFFFFFFE00000,%rbx
    and     %rbx,%rax
    pop     %rbx
    pop     %rcx
    pop     %rsi
    ret


// This function is thread-safe and multi-processor safe
allocateStackPage:
    push    %rdi
    mov     $0b001000000000,%rdi
    call    allocatePage
    // be carefull, rax contains a ret value that needs to be passed down
    pop     %rdi
    ret

// This function is thread-safe and multi-processor safe
allocateHeapPage:
    push    %rdi
    mov     $0b011000000000,%rdi
    call    allocatePage
    // be carefull, rax contains a ret value that needs to be passed down
    pop     %rdi
    ret
