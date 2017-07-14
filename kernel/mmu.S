#include "macros.h"
#include "../memorymap.h"
#include "config.h"
#include "tasks.h"
.global setupProcessPageStructureForUserProcess
.global allocateStackPage
.global allocateHeapPage
.global setupPageStructure
.global setupProcessPageStructure
.global destroyProcessPageStructure
.global getPTEntry
.global markPageAsInUse
.global kernelAllocPages
.global kernelReleasePages
.global mapPhysOnVirtualAddressSpace
.global calculateMemoryUsage
.global currentProcessVirt2phys
.global virt2phys
.global releasePhysicalPage
.global emulate_invpcid
.global isMirrorPageDirty
.global release_pages


/*
    The first 128mb in ram are reserved for kernel use. They are mapped as 2mb pages.
    That area contains the kernel code and the kernel's page tables.
    The remainder of memory is identiy-mapped using 4k pages. They will be used to keep track
    of available memory

    Each thread have their own page tables, they don't use the kernel's table. But
    they can read/write the kernel's table in order to reserve memory.  On every
    task switch cr3 is reloaded thus the stack's physical location is changed everytime.

    The 4gig above the 256tb mark is mirroring the first 4gig so that any process have 
    access to the physical identity

DYNAMIC ALLOCATION:
    bits [9-11] in a page directory entry is available for user. We will use it as follow:
        0b000: physical memory pointed to by this page is free to use
        0b001: physical memory pointed to by this page is also mapped somewhere else and is a stack page for a thread
        0b010: physical memory pointed to by this page is used by the kernel
        0b011: physical memory pointed to by this page is used as heap for a thread or the kernel

*/



/*
        stack for each thread will use 0xFE000000 as the TOP. Stack will grow down. 
            that page must point to a physical location starting after 128meg
            so the page after 128meg will be marked as used.
                that means that that physical location will be mapped twice.
                but the page in the bottom is only used to know that that
                address is in use. We could set a flag saying that it is
                used at some other remapping.

*/
// rdi=page count
allocatePhysicalPages:
    push    %r9
    push    %r8
    push    %rdx
    push    %rcx
    push    %rbx
    
    // Loop through all 4k pages. The PAGETABLE is in kernel mem. process sees same memory arrangement
    mov     $PAGETABLES,%rdx
    sub     $8,%rdx
    mov     $((MAX_RAM-KERNEL_RESERVED_END)/4096),%rcx
searchFreePage:
    add     $8,%rdx

    call    reserveConsecutivePages
    cmp     %r8,%rdi
    je      foundPages

    //TODO: must deallocate pages that were allocated.
        // first page would be %rax, and count is in %r8

    loop    searchFreePage
    mov     $0,%rax
    jmp     1f
foundPages:
    mov     $0x000FFFFFFFFFF000,%rbx
    and     %rbx,%r9
    mov     %r9,%rax

1:  pop     %rbx
    pop     %rcx
    pop     %rdx
    pop     %r8
    pop     %r9
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// emulate_invpcid(rdx=PCID)
// will invalidate all TLB entries associated with PCID specified in RDI
// This emulation uses bit63 of cr3 to do the invalidation.
// It loads a temporary address
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
emulate_invpcid:
    push        %rax
    and         $0xFFF,%rdi
    or          $PML4TABLE,%rdi
    btsq        $63,%rdi
    mov         %cr3,%rax

    // We need to mask interrupts because getting preempted
    // while using the temporary page table address
    // things would go bad.
    pushfq
    cli
    mov         %rdi,%cr3
    mov         %rax,%cr3
    popfq
    pop         %rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// kernelReleasePages(rdi=phy_address, rsi=count)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
kernelReleasePages:
    push        %rcx
    push        %rdi
    
    btrq        $MIRROR_BIT,%rdi
    mov         %rsi,%rcx
1:  call        releasePhysicalPage
    add         $0x1000,%rdi
    loop        1b
    
    pop         %rdi
    pop         %rcx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// releasePhysicalPage(rdi=phy_address)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
releasePhysicalPage:
    push        %rax
    push        %rbx
    push        %rdi

    sub         $KERNEL_RESERVED_END,%rdi
    shr         $12,%rdi                    // convert page index
    shl         $3,%rdi                     // convert to byte index
    add         $PAGETABLES,%rdi

    mov         (%rdi),%rax
    mov         %rax,%rbx
    and         $~0b111000000000,%rbx       // clear AVL flags    
    lock cmpxchg %rbx,(%rdi)
    jnz         releaseError

    pop         %rdi
    pop         %rbx
    pop         %rax
    ret

releaseError:
    // WARNING: Set breakpoint. I have no idea why this would happen
    // The only other thing that could write the AVL bits
    // is a pageAlloc. Why would an alloc write on an allocated
    // page?
    int         $3    


// This is for allocating buffers of consecutive physical memory for drivers
// drivers must use physical memory because their IRQ can be called during a thread
// and each thread has different virtual memory mapping. So it needs to be consistent.
// rdi = number of pages
kernelAllocPages:
    call    allocateHeapPage
    btsq    $MIRROR_BIT,%rax            // set address in mirror space
    ret

// %rdx = PDTABLE entry to start from
// %rdi = count
// RETURN:
//      %r8 = number of pages allocated
//      %rdx = next entry to check (rdx incremented)
//      %r9 = first page
reserveConsecutivePages:
    push    %rdi
    mov     $0,%r8

2:  mov     (%rdx),%rbx             // now rbx = the PTE
    test    $0b111000000000,%rbx    // is the page free? check the 3 AVL bits, should be zero
    jnz     1f
    mov     %rbx,%rax              //rax will be used by cmpxchg
    or      %rsi,%rbx               //rsi=reserve_type that will be set in the AVL bits
    lock cmpxchg %rbx,(%rdx)        //if rax==(esi), copy rbx in (esi). this is to make sure that
    jnz     1f                       //  (rsi) is still free. if not, then another thread reserved it
    cmp     $0,%r8
    cmove   %rax,%r9
    inc     %r8
    dec     %rdi
    jz      1f
    add     $8,%rdx
    jmp     2b
1:  pop     %rdi
    ret


// This function is thread-safe and multi-processor safe
// rdi = number of page
// return rax=physical address
allocateStackPage:
    push    %rsi
    mov     $0b001000000000,%rsi
    call    allocatePhysicalPages
    // be carefull, rax contains a ret value that needs to be passed down
    pop     %rsi
    ret

// This function is thread-safe and multi-processor safe
// rdi = number of page
allocateHeapPage:
    push    %rsi
    mov     $0b011000000000,%rsi
    call    allocatePhysicalPages
    // be carefull, rax contains a ret value that needs to be passed down
    pop     %rsi
    ret




////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// mapPhysOnVirtualAddressSpace(rdi=pageTableBase,rsi=virtualAddress,
//                              rdx=physical_address, rcx=page_count, 
//                              r8=permission_flag
//
// permission_flag:
//      bit63: no execute flag (1= no execute)
//      bit2: non-priviledged (0=supervisor only)
//      bit1: writable flag (1=R/W)
//
// NOTES: Physical pages must be contiguous
//        Virtual pages must be contiguous
//        pageTableBase is the address of the PT for 4k pages at the 128mb mark
//          since below 128mb, we are using 2mb pages (Page Directory Entries)
//          For this reason, the virtual address must be above the 128mb mark
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
mapPhysOnVirtualAddressSpace:
    push        %rdi
    push        %rsi
    push        %rcx
    push        %rdx

    // create PTE out of physical address
    or          $0b001000000001,%rdx
    or          %r8,%rdx

    // PTE index = (virt-128mb)/4096
    sub         $KERNEL_RESERVED_END,%rsi
    shr         $12,%rsi

    // PTE address = PTEIndex*8 + pageTableBase
    lea         (%rdi,%rsi,8),%rdi

    // now store the PTEs in the page table
1:  mov         %rdx,(%rdi)
    add         $8,%rdi
    add         $0x1000,%rdx
    loop        1b    

    pop         %rdx
    pop         %rcx
    pop         %rsi
    pop         %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//  reservePhysicalMemoryForVirtualRange(rdi=virtual_address, rsi=pageCount)
//  Warning: this is not multi-thread friendly. It will change page entries but
//  assumes that the caller is the owner of those pages.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
reservePhysicalMemoryForVirtualRange:
    push        %rdi
    push        %rax
    push        %rcx
    push        %rdx
    push        %rbx

    mov         %rsi,%rcx
1:  push        %rdi
    mov         $1,%rdi
    call        allocateHeapPage        // rdi= number of pages
    mov         %rax,%rbx

    
    pop         %rdi
    call        getPTEntry
    mov         (%rax),%rdi
    and         $0xFFF,%rdi
    or          %rbx,%rdi
    mov         %rdi,(%rax)
    add         $0x1000,%rdx
    loop        1b  

    pop         %rbx
    pop         %rdx
    pop         %rcx
    pop         %rax
    pop         %rdi
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: currentProcessVirt2phys(rdi=virtual_address)
//   RETURN %rax= physical address in mirror addressing space
//                will return 0 if page not present
//   WARNING: do not use this function with memory alocated with malloc.
//            malloc's buffer might cross a page boundary and there is no
//            guarantee that pages are mapped continguously in ram.
//            so using physical address would be dangerous.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
currentProcessVirt2phys:
    push        %rsi
    push        %rcx

    // save page offset (the lower 12bit part)
    mov         %rdi,%rcx
    and         $0xFFF,%rcx

    mov         %cr3,%rax       // virt2phys will clean the address
    mov         %rax,%rsi
    call        virt2phys
    bt          $0,%rax         // check if page is present
    jnc         pageDoesNotExist
    shr         $12,%rax
    shl         $12,%rax
    btsq        $MIRROR_BIT,%rax
    btrq        $63,%rax        // remove the NX bit
    add         %rcx,%rax
    pop         %rcx
    pop         %rsi
    ret
pageDoesNotExist:
    pop         %rcx
    pop         %rsi
    xor         %rax,%rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: virt2phys(rdi=virtual_address, rsi=pml4_address)
//   RETURN %rax= physical address in mirror addressing space
//                will return 0 if page not present
//  WARNING: This returns the page descriptor (including flags) , 
//            NOT the mirror address
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
virt2phys:
    push        %rbx

    mov         %rsi,%rax
    CLEANCR3ADDRESS(%rax)
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    //rax=PML4 address

    // get PML4 offset from virtual address
    mov         %rdi,%rbx
    shr         $(39-3),%rbx
    and         $0xFF8,%rbx
    add         %rbx,%rax
    mov         (%rax),%rax
    bt          $0,%rax         // check if page is present
    jnc         pageDoesNotExist2
    shr         $12,%rax
    shl         $12,%rax
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    // rax = PDP table address

    // get PDP offset from virtual address
    mov         %rdi,%rbx
    shr         $(30-3),%rbx
    and         $0xFF8,%rbx
    add         %rbx,%rax
    mov         (%rax),%rax
    bt          $0,%rax         // check if page is present
    jnc         pageDoesNotExist2
    shr         $12,%rax
    shl         $12,%rax
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    // rax = PD table address

    // get PD offset from virtual address
    mov         %rdi,%rbx
    shr         $(21-3),%rbx
    and         $0xFF8,%rbx
    add         %rbx,%rax
    mov         (%rax),%rax
    bt          $0,%rax         // check if page is present
    jnc         pageDoesNotExist2
    shr         $12,%rax
    shl         $12,%rax
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    // rax = PT table address

    // get PT offset from virtual address
    mov         %rdi,%rbx
    shr         $(12-3),%rbx
    and         $0xFF8,%rbx
    add         %rbx,%rax
    // rax = PT entry address


    mov         (%rax),%rax
    pop         %rbx
    ret

pageDoesNotExist2:  
    xor         %rax,%rax
    pop         %rbx
    ret
    

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: getPTEntry(%rdi=virtual_address)
//   RETURN %rax= address of PTE for that address. Address will be in 
//          a identity address residing over the 256th gig.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
getPTEntry:
    push        %rbx

    // In virtual address:
    // 47:39 - PML4 entry index 
    // 38:30 - PDP  entry index
    // 29:21 - PD entry index
    // 20:12 - PT entry index

    mov         %cr3,%rax
    CLEANCR3ADDRESS(%rax)
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    //rax=PML4 address

    // get PML4 offset from virtual address
    mov         %rdi,%rbx
    shr         $(39-3),%rbx
    and         $0xFF8,%rbx  
    add         %rbx,%rax
    mov         (%rax),%rax
    shr         $12,%rax
    shl         $12,%rax
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    // rax = PDP table address

    // get PDP offset from virtual address
    mov         %rdi,%rbx
    shr         $(30-3),%rbx
    and         $0xFF8,%rbx  
    add         %rbx,%rax
    mov         (%rax),%rax
    shr         $12,%rax
    shl         $12,%rax
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    // rax = PD table address

    // get PD offset from virtual address
    mov         %rdi,%rbx
    shr         $(21-3),%rbx
    and         $0xFF8,%rbx
    add         %rbx,%rax
    mov         (%rax),%rax
    shr         $12,%rax
    shl         $12,%rax
    btsq        $MIRROR_BIT,%rax        // set bit38 to add 256gig for identity mapping
    // rax = PT table address
    
    // get PT offset from virtual address
    mov         %rdi,%rbx
    shr         $(12-3),%rbx
    and         $0xFF8,%rbx
    add         %rbx,%rax
    // rax = PT entry address

    pop         %rbx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
setupProcessPageStructureForUserProcess:
    push        %rdx
    push        %rdi
    push        %r8
    push        %r9

    call        setupProcessPageStructure
    mov         %r8,(%rdi)
    mov         %r9,8(%rdi)

    pop         %r9
    pop         %r8
    pop         %rdi
    pop         %rdx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: setupProcessPageStructure()
// This will create a page structure somewhere in RAM and return the 
// base physical address of the PLM4 table
//
// returns:
//      r8 = pml4 offset
//      r9 = 4k pagetable start 
////////////////////////////////////////////////////////////////////////////////////
setupProcessPageStructure:
    push    %rax
    push    %rdi
    push    %rsi
    push    %rdx 

    mov     $RAM_COUNT_GIG,%rdi
    sub     $8,%rsp
    mov     %rsp,%rsi
    sub     $8,%rsp
    mov     %rsp,%rdx
    call    setup_process_page_structure
    mov     8(%rsp),%r8
    mov     (%rsp),%r9
    add     $16,%rsp

    pop     %rdx
    pop     %rsi
    pop     %rdi
    pop     %rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: destroyProcessPageStructure(rdi=pml4_address)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
destroyProcessPageStructure:
    push        %rsi

    xor         %rsi,%rsi           // level 0: PML4
    call        destroyTable

    // convert descriptor to physical address
    shr         $12,%rdi
    shl         $12,%rdi
    btrq        $63,%rdi
    call        releasePhysicalPage

    pop         %rsi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// destroyTable(rdi= table address, rsi=current page level)
//
// This will destroy a table. Any table (PML4, PDP, PD, PT)
// It will loop though all entries, destroy the table for that 
// entry (this is a recursive function) and release the page for 
// that tbkae
// Levels (rsi):
//      0: PML4
//      1: PDP
//      2: PD
//      3: PT
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
destroyTable:
    // destroyTable(table,level)
    //     for i=0..512
    //         pe = table[i]
    //         if !pe.bit0 continue
    //         if level==2
    //             if pe.bit7==1 continue
    //         else
    //             destroyTable(pe,level++)
    //         releasePage

    push        %rcx
    push        %rdi
    push        %rax
    push        %rbx
    mov         $~0x8000000000000FFF,%rbx  
    mov         $512,%rcx               // there are 512 entries for a table
    btsq        $MIRROR_BIT,%rdi                // need to access this using the mirror area
1:  mov         (%rdi),%rax
    xchg        %rdi,%rax               // temp save

    // Check if entry is "Present". If not, then it doesn't
    // point to sub-table so there is no need to free it.
    bt          $0,%rdi
    jnc         2f

    // Verify if this is a leaf entry, meaning
    // that this is a PDE for a 2mb page or a PTE.
    // In such a case, there is no sub table to free
    cmp         $2,%rsi                 // PD entry? if not, then continue recursively
    jne         3f                      
    bt          $7,%rdi                 // PD entries with bit7 point to leaf 2mb page
    jc          2f                      // so ignore it.

    // At this point, we know we are looking at a PD-e and bit7 not set.
    // This means that the entry points to a page table. No need
    // to go in recursively, just release that page
    jmp         4f

3:  and         %rbx,%rdi               // extract physical address from descriptor
    
    // If the table belongs in kernel memory, it means that this is a reused table
    // and we should not destroy it. For example, the 256th gig mirror are
    // page directories create in kernel mem and reused by all processes.
    cmp         $KERNEL_RESERVED_END,%rdi
    jb          2f

    inc         %rsi
    call        destroyTable
    dec         %rsi

    // Now that that table is cleared, release the page holding it.
4:  and         %rbx,%rdi               // extract physical address from descriptor
    call        releasePhysicalPage

2:  xchg        %rdi,%rax               // temp restore
    add         $8,%rdi
    loop        1b
    pop         %rbx
    pop         %rax
    pop         %rdi
    pop         %rcx
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: markPageAsInUse(rax=address, rdi=pagetable_base_address, rcx=phys_addr)
// WARNING: NO ADRESS HIGHER THAN 4G SINCE PAGE TABLES ARE NOT BIG ENOUGH
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
markPageAsInUse:
    push    %rbx

    cmp     $0xFFFFFFF,%rax
    ja      1f
    sub     $KERNEL_RESERVED_END,%rax
    shr     $12,%rax            // get Page Number
    shl     $3,%rax             // Mul by 8 since each entry is 8byte in length
    add     %rdi,%rax
    mov     (%rax),%ebx
    and     $0xFFFFF1FF,%ebx     // clear AVL
    or      $0x00000400,%ebx     // in use by kernel
    or      %rcx,%rbx
    mov     %ebx,(%rax)
1:  pop     %rbx
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: release_pages(rdi=from, rsi=to)
//  This function is called bu the heap manager to indicate which pages
//  are being freed.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
release_pages:
    //TODO: free those pages and make sure we invalidate them in the TLB
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: calculateMemoryUsage() 
// Return: %rax=number of pages used above the 128mb mark
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
calculateMemoryUsage:
    push        %rdi
    push        %rcx
    
    xor         %rax,%rax
    mov         $PAGETABLES,%rdi
    btsq        $MIRROR_BIT,%rdi
    
    mov         $((PAGETABLES_END-PAGETABLES)/8),%rcx
1:  testq       $0b111000000000,(%rdi)
    jz          2f
    inc         %rax  
2:  add         $8,%rdi
    loop        1b

    pop         %rcx
    pop         %rdi
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: isMirrorPageDirty(rdi=virtual_address) 
// This will lookup an PTE in the mirror page tables. Those page tables
// are common to all processes. This will return the dirty bit of the PTE
// This will reset the dirty flag
// Return: dirty bit is stored in carry flag
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
isMirrorPageDirty:
    push    %rdi

    btr     $MIRROR_BIT,%rdi

    // the page tables start at 128mb, because before that, 
    // the mapping is in the the PDE. The PDP is shared for all 
    // processes, so we access the same place.

    // get page entry in table: addr/4096 * 8
    shr         $9,%rdi   

    // now add PATABLES - (128meg/4096)*8
    btr         $6,(PAGETABLES-0x40000)(%rdi)
    mov (PAGETABLES-0x40000)(%rdi),%rax
    pop         %rdi
    ret










/*
    This structure is temporary. It is set while still in 32bit protected mode before
    doing the switch to long mode. Once in long mode, we will setup another paging
    structure that will be more versatile
        there are 4 levels:
            PLM4 is a table with 512 64bit entries that references PDPTs. 1 PDPT references 512 gig.
                the PLM4 table is 4k
            PDPT: table of 512 64bit entries that references PDs. 1PD references 1gig. Each PDPT is 4k
            PD: 512 64entries that references a 2mb page or a PT. Each PD is 4k

*/
.code32
setupPageStructure:
    push    %eax
    push    %ebx
    push    %edi
    push    %ecx

    // create 1 PML4 entry (because it is good for 512gig)
    mov     $PML4TABLE,%edi
    mov     $1024,%ecx          /*clear the table first*/
    xor     %eax,%eax
    rep     stosl
    mov     $PML4TABLE,%edi
    mov     $PDPTTABLE,%eax /* This address is aligned on 4kboundary. So bit0-11 are cleared*/
    or      $0b000000000111,%eax /* present, r/w, user/supervisor access, no pwt nor pcd*/
    xor     %ebx,%ebx       /* upper 32bit of PML4 entry. address=0 because below 4G. NX=0*/
    mov     %eax,(%edi)
    mov     %ebx,4(%edi)
    mov     %eax,(8*128)(%edi)   // same entry for #256 for mirror

    // Create the PDPT with 4 entries. Each entry describres 1 gig
    mov     $PDPTTABLE,%edi
    mov     $1024,%ecx          /*clear the table first*/
    xor     %eax,%eax
    rep     stosl
    mov     $PDPTTABLE,%edi
    mov     $PDTABLE,%eax /* This address is aligned on 4kboundary. So bit0-11 are cleared*/
    or      $0b000000000111,%eax /* present, r/w, user/supervisor access, no pwt nor pcd*/
    xor     %ebx,%ebx       /* upper 32bit of PML4 entry. address=0 because below 4G. NX=0*/
    mov     $4,%ecx         /* we need 4 of those entries that can address 1g each */
createPDPTEntry:
    mov     %eax,(%edi)
    mov     %ebx,4(%edi)
    add     $8,%edi
    add     $0x1000,%eax        /* next PD table is 4k further */
    loop    createPDPTEntry

    // 2mb PDEs. The first 128mb are 2mb pages reserved for kernel
    // 64 first entries in the first Page Directory point to 1st 128mb
    mov     $PDTABLE,%edi
    xor     %ebx,%ebx           /*We will not have any page that will address over 4gig. so it is all zeros here*/
    mov     $0b0010010000111,%eax           /*Start at address 0. present, rw, US, in use by kernel */
    mov     $(KERNEL_RESERVED_END/(2*1024*1024)),%ecx   // kernel space is in 2mb page           
1:  mov     %eax,(%edi)
    mov     %ebx,4(%edi)
    add     $8,%edi
    add     $0x200000,%eax        /* next page start 2mb further */
    loop    1b

    pop     %ecx
    pop     %edi
    pop     %ebx
    pop     %eax
    ret



