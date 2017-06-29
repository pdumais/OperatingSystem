#include "../memorymap.h"
#include "macros.h"
#include "includes/kernel/intA0.h"
.global startKernelThread

.extern loadUserApplications
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
.extern launchUserThread
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
.extern testThread
.extern testThread2

.align 16

startKernelThread:
    mov     $mainkernelthread,%rdi
    mov     $(mainkernelthread_end-mainkernelthread),%rsi
    call    launchKernelThread

    ret



//////////////////////////////////////////////
//////////////////////////////////////////////
/////////// Main Kernel Thread ///////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
// this thread serves no purpose, it's just to test if
// I can launch a thread from another thread
mainkernelthread:

    call    createTextConsole

    call    initTestIPI

    // This thread will own the screen initially
    mov     $0,%rdi
    call    switchFrontLineProcessByIndex 


    call    initPCI

    mov     $BLOCK_CACHE,%rdi        // blockNumber 242
    call    block_cache_init

    call    initKeyboard
    call    net_init
    call    net_start


    mov     $0xC0A8011C,%rdi        // 192.168.1.28
    mov     $0xFFFFFF00,%rsi        // 255.255.255.0
    mov     $1,%rdx
    mov     $0,%rcx                  // Card 0
    call    net_setIPConfig
    mov     $0xC0A8021C,%rdi        // 192.168.2.28
    mov     $0xFFFFFF00,%rsi        // 255.255.255.0
    mov     $1,%rdx
    mov     $1,%rcx                 // Card 1
    call    net_setIPConfig
    mov     $0xC0A80101,%rdi
    mov     $0,%rsi
    call    net_setDefaultGateway

    mov     $4,%rdi             //4= virtio drive
    call    loadUserApplications


    mov     $0xA60,%rsi
    mov     $1,%rdi
    call    create_vm
//    mov     $0xB00,%rsi
//    mov     $6,%rdi
//    call    create_vm

1:
    call    manageConsoles
    call    manageDeadProcesses
    hlt
    jmp     1b

blockbuffer:
    .skip (5*512)

mainkernelthread_end:



