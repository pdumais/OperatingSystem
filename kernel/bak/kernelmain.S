#include "../memorymap.h"

.CODE32

.extern writeString
.extern writeNumber
.extern setupIDT
.extern initPIC
.extern enableIRQ
.extern initPIT
.extern initScheduler
.extern allocateStackPage
.extern mapVirtualAddress
.extern launchThread
.extern mutexLock
.extern mutexUnlock
.extern gotoxy
.extern initPCI
.extern net_init
.extern net_start
.extern net_process
.extern arp_getMAC
.extern net_setIPConfig

.section .text
// This is where the bootloader throws us. 
// We are in protected mode at this point
main:
    mov     $8,%ax
    mov     %ax,%ds
    mov     %ax,%es
    mov     %ax,%fs
    mov     %ax,%gs
    mov     %ax,%ss


    call    builKernelPageStructures
    call    enableLongMode

    mov      $0xB8000,%edi
    movb     $0x47,%ds:(%edi)
    movb     $0x04,%ds:1(%edi)
 
    //jump to long section using GDT entry #2
    //This is it! We going into long mode
    ljmpl   $0x18,$longmode


builKernelPageStructures:
    /*
        WE ARE USING 2MB pages
        there are 4 levels: 
            PLM4 is a table with 512 64bit entries that references PDPTs. 1 PDPT references 512 gig.
                the PLM4 table is 4k
            PDPT: table of 512 64bit entries that references PDs. 1PD references 1gig. Each PDPT is 4k
            PD: 512 64entries that references a 2mb page. Each PD is 4k

        We will only address 4gig of mem. So:
            in the PLM4, we will use the 1st entry and mark the remaining 511 entries as invalid.
                we will thus create only 1 PDPT. In the PDPT we will use the 1st 4 entries and mark all other 508 as invalid
                    we will thus create only 4 PDs.
        1 PLM4 + 1 PDPT + 4 PD = 4k+4k+4*4k = 32k for all structures.

    */

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

    // Create the PDPT with 4 entries
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


    // create the page directory entries. Note that each PD are one after the other in memory
    // This is the actual mapping of memory
    mov     $PDTABLE,%edi
    xor     %ebx,%ebx           /*We will not have any page that will address over 4gig. so it is all zeros here*/
    mov     $0b0000010000111,%eax           /*Start at address 0. present, rw, US, */
    mov     $2048,%ecx              /*4 PD with 512 entries to fill*/
createPDEntry:
    mov     %eax,(%edi)
    mov     %ebx,4(%edi)
    add     $8,%edi
    add     $0x200000,%eax        /* next page start 2mb further */
    loop    createPDEntry

    // now we must mark the first 10megs as "in use by kernel". See notes in mmu.c
    mov     $PDTABLE,%edi
    mov     $5,%ecx
markused:
    mov     (%edi),%eax
    or      $0b010000000000,%eax    /*in use by kernel*/
    mov     %eax,(%edi)
    add     $8,%edi
    loop    markused

    //TODO:  should also marke the STAK virtual memory to be busy

    pop     %ecx
    pop     %edi
    pop     %ebx
    pop     %eax
    ret

enableLongMode:
    push    %eax
    push    %ecx

    // set PML4 address
    mov     $PML4TABLE,%eax
    mov     %eax,%cr3

    // Enable PAE
    mov     %cr4,%eax
    or      $0b10100000,%eax
    mov     %eax,%cr4

    // enable long mode
    mov     $0xC0000080,%ecx
    rdmsr
    or      $0b100000000,%eax
    wrmsr

    //enable paging
    mov     %cr0,%eax
    or      $0x80000001,%eax
    mov     %eax,%cr0 

    pop     %ecx
    pop     %eax
    ret






//////////////////////////////////////////////
//////////////////////////////////////////////
///////// LONG MODE CODE /////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
.CODE64
.align 16
longmode:
    cli
    mov     $0x400,%rsp         /* temporary stack location */
    mov     %rsp,%rbp

    // Setup the IDT structure
    call    setupIDT

    call    initPIC
    call    initPIT
    mov     $0x100,%rdi //enable IRQ 0 (int 0x20)
    call    enableIRQ
    call    initScheduler
    sti

    mov     $mainkernelthread,%rdi
    call    launchThread
    // now we don't need to execute in here anymore. 
    // Wait until the scheduler dispatches the kernel thread
waityield:
    hlt
    jmp waityield












//////////////////////////////////////////////
//////////////////////////////////////////////
/////////// Main Kernel Thread ///////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
mainkernelthread:
    mov     $tr1,%rdi   //this is a test
    call    launchThread//this is a test
    mov     $tr1,%rdi//this is a test
    call    launchThread//this is a test

    call    initPCI
    call    net_init
    call    net_start

    //TODO: this is temporary. Hardcoded network settings
    mov     $0xC0A8011C,%rdi        // 192.168.1.28
    mov     $0xFFFFFF00,%rsi        // 255.255.255.0
    mov     $0xC0A80101,%rdx        // 192.168.1.1
    mov     $1,%rcx
    call    net_setIPConfig

1:  call    net_process
    int     $0x40           // yield
    jmp     1b



// ====================== THIS IS A TEST THREAD
tr1:
    mov     $trIndex,%rbx
    mov     (%rbx),%rax
    inc     %rax
    mov     %rax,(%rbx)
    push    %rax
    mov     $txttr1,%rdi
    call    writeString

//mov $0xC0A80101,%rdi        // 192.168.1.1
//call arp_getMAC
//mov $0xC0A80102,%rdi        // 192.168.1.1
//call arp_getMAC

    pop     %rax
    shl     $2,%rax
    add     $0xB8F00,%rax
    movb    $65,(%rax)
    mov     $65,%cl
1:  mov     (%rax),%rbx
    inc     %rbx
    cmp     $90,%bl
    cmovz   %rcx,%rbx
    mov     %bl,(%rax) 
    jmp     1b
txttr1:
    .ASCIZ "Running thread\r\n"
trIndex:
    .QUAD 0
// =============================== END OF TEST THREAD

safeguard: jmp safeguard


