.global getPageDescriptorAddress
.global mapVirtualAddress
#include "../memorymap.h"


/*
    returns address of page directory entry associated with virtual address passed in %rdi
*/
getPageDescriptorAddress:
    shr     $18,%rdi    /* divide by 2meg, then mul by 8bytes per entry = >>21, <<3 = >> 18 */ 
    add     $PDTABLE,%rdi
    mov     %rdi,%rax
    ret


/*
finds the page directory entry associated with virtual address in %rdi
and changes its physical location to address in %rsi and invalidates the page
*/
mapVirtualAddress:
    mov     %rdi,%rax
    shr     $18,%rdi    /* divide by 2meg, then mul by 8bytes per entry = >>21, <<3 = >> 18 */
    add     $PDTABLE,%rdi
    and     $0xFFFFFFFFFFE00000,%rsi        // 2meg pages, so clear the rest
    or      $0b001010000111,%rsi           // mark it as a Stack page
    mov     %rsi,(%rdi)
    invlpg  (%rax)
    ret


