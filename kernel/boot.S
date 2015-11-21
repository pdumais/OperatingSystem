#include "../memorymap.h"
#include "macros.h"

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
.extern launchKernelThread
.extern mutexLock
.extern mutexUnlock
.extern gotoxy
.extern initPCI
.extern net_init
.extern net_start
.extern net_process
.extern arp_getMAC
.extern net_setIPConfig
.extern net_setDefaultGateway
.extern update_display
.extern initHardware
.extern startKernelThread
.extern initSMPForBST
.extern initSMPForAP

.section .text
// This is where the bootloader throws us. 
// We are in protected mode at this point
main:
    cli
    mov     $8,%ax
    mov     %ax,%ds
    mov     %ax,%es
    mov     %ax,%fs
    mov     %ax,%gs
    mov     %ax,%ss

    GET_APIC_ID(%eax)
    cmp     $64,%eax            // we don't support more than 64 CPUs
    jb      1f
    mov     $0xDEADBEEF,%eax
    mov     $0xDEADBEEF,%ebx
    mov     $0xDEADBEEF,%ecx
    mov     $0xDEADBEEF,%edx
    STALL()

    // Setup the stack for current processor: stack = AP_STACKS + id*STACK_SIZE
1:  AP_STACK(%eax)
    mov     %eax,%esp     

    // Check if the is the bootstrap processor, and if MP is enabled
    // because APs will jump to here also
    mov     $0x1B,%ecx
    rdmsr
    bt      $8,%eax//if bit 8 of eax is set, it measn this is the boostrap processor
    jc      isBST
    
    call    enableLongMode
    ljmpl   $0x18,$longmodeAP

isBST:
    call    setupPageStructure
    call    enableLongMode
    ljmpl   $0x18,$longmode



enableLongMode:
    push    %eax
    push    %ecx

    

    // set PML4 address
    mov     $PML4TABLE,%eax
    mov     %eax,%cr3

    // Enable PAE and PCID
    mov     %cr4,%eax
    or      $0b10100000,%eax
    mov     %eax,%cr4

    // enable long mode
    mov     $0xC0000080,%ecx
    rdmsr
    or      $(1<<8)|(1<<11),%eax
    wrmsr

    //enable paging
    mov     %cr0,%eax
    or      $0x80000001,%eax
    mov     %eax,%cr0 

#ifdef PCID
    mov     %cr4,%eax
    or      $0x20000,%eax           // THIS IS FOR PCID feature
    mov     %eax,%cr4
#endif

    pop     %ecx
    pop     %eax
    ret

//////////////////////////////////////////////
//////////////////////////////////////////////
///////// LONG MODE CODE /////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
.CODE64
prepareGDTAndTSS:
    push    %rdi
    push    %rsi
    push    %rcx
    push    %rax

    movq    $STACK0TOP_VIRTUAL_ADDRESS,%rax
    movq    %rax,(TSS+4)

    mov     $(GDTDATA_END-GDTDATA),%rcx
    mov     $GDTDATA,%rsi
    mov     $GDT,%rdi
    mov     $GDT,%edi
    rep     movsb

    movw    $0x100, GDTLIMIT
    movq    $GDT, GDTADDR
    lgdt    GDTINFO

    // We need to hack the TSS. We want to use the same TSS
    // on all CPUs but LTR doesn't want to load a descriptor that is busy.
    // We don't care about that. So lock the section, and clear the busy flag before
    // calling ltr
1:  lock btsq $0,ltrlock
    jc      1b
    mov     $GDT+TSSSELECTOR+5,%rdi
    btrq    $1,(%rdi)                   // clear the Busy flag
    mov     $TSSSELECTOR|0b11,%rax
    ltr     %ax
    btrq    $0,ltrlock                  // clear the lock

    pop     %rax
    pop     %rcx
    pop     %rsi
    pop     %rdi
    ret

ltrlock: .quad 0

.align 16
enableAVX:
    mov     %cr0,%rax
    or      $0b10,%rax
    and     $0xFFFFFFFFFFFFFFFD,%rax
    mov     %rax,%cr0

    mov     %cr4,%rax
    or      $0x40600,%rax       
    mov     %rax,%cr4

    mov     $0,%edx
    mov     $0b111,%eax
    mov     $0,%ecx
    xsetbv
    ret

.align 16
longmodeAP:
    call    prepareGDTAndTSS
    call    initSMPForAP

    call    loadIDT
    call    enableAVX
    call    initAPICTimer

    // Wait for BST to be ready before enabling interrupts and starting scheduler.
1:  btq     $0,WAITFORBST
    jc      1b
    sti
    // now we don't need to execute in here anymore. 
    // The scheduler will dispatch to another thread eventually.
    STALL()

.align 16
longmode:
    call    prepareGDTAndTSS
    call    setupIDT
    call    loadIDT
    call    enableAVX
    call    initScheduler

    call    initSMPForBST
    call    initAPICTimer

    call    initHardware
    call    init_memory_pools
    call    initConsoles
    call    startKernelThread

    movq    $0,WAITFORBST
    call    smpUp
    sti

    // now we don't need to execute in here anymore. 
    // The scheduler will dispatch to another thread eventually.
    STALL()


WAITFORBST: .quad 1



GDTINFO:
GDTLIMIT: .word 0
GDTADDR: .quad 0
GDTDATA:
    .LONG 0
    .LONG 0

    // Data segment
    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10010010
    .BYTE 0b11001111
    .BYTE 0x00

    // Ring 3 Code segment
    // Non-conforming with DPL=3. Since it is non-conforming, Ring0 cant jump to Ring3 unless
    // the RPL overides to 3 in the selector
    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b11111010            
    .BYTE 0b10101111 
    .BYTE 0x00

    // Ring 0 Code segment
    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10011010
    .BYTE 0b10101111 
    .BYTE 0x00

    // TSS descriptor (this one is 16bytes long
    .WORD 0x67
    .WORD (TSS&0xFFFF)
    .BYTE ((TSS>>16)&0xFF)
    .BYTE 0b11101001
    .BYTE 0b10000000
    .BYTE ((TSS>>24)&0xFF)
    .LONG ((TSS>>32)&0xFFFFFFFF)
    .LONG 0

    // Ring3 Stack segment
    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b11110010
    .BYTE 0b11001111
    .BYTE 0x00

GDTDATA_END:
