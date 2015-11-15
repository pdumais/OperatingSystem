#include "../memorymap.h"
#include "macros.h"
#include "intA0.h"
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

//mov $0x08000000,%rdi
//call currentProcessVirt2phys
//mov (%rdi),%rbx
//mov 8(%rdi),%rcx
//int $3

    call    createTextConsole


    mov $7,%rax
    mov $0,%rcx
    cpuid
    //int $3 

    call    initTestIPI

    // This thread will own the screen initially
    mov     $0,%rdi
    call    switchFrontLineProcessByIndex 

    call    initPCI
    call    initKeyboard
    call    net_init
    call    net_start
    mov     $BLOCK_CACHE,%rdi        // blockNumber 242
    call    block_cache_init

    mov     $0xC0A8011C,%rdi        // 192.168.1.28
    mov     $0xFFFFFF00,%rsi        // 255.255.255.0
    mov     $1,%rdx
    mov     $0,%rcx                  // Card 0
    call    net_setIPConfig
    mov     $0xC0A8021C,%rdi        // 192.168.1.28
    mov     $0xFFFFFF00,%rsi        // 255.255.255.0
    mov     $1,%rdx
    mov     $1,%rcx                 // Card 1
    call    net_setIPConfig
    mov     $0xC0A80101,%rdi
    mov     $0,%rsi
    call    net_setDefaultGateway

    mov     $1,%rdi
    call    loadUserApplications

1:   
    call    manageConsoles
    call    manageDeadProcesses



mov $INTA0_POLL_IN,%rax
int $0xA0
cmp $0,%rax
jz 3f
cmp $0x30,%al
jne 4f
call showMem
jmp 3f
4:
cmp $0x31,%al
jne 4f
mov     $1,%rdi
call    loadUserApplications
4:
cmp $'z',%al
jne 4f
call showIPITestResult
4:
cmp $'a',%al
jne 4f
call testIPI
call showIPITestResult
4:
cmp $'2',%al
jne 4f
call  testGetTicksSinceBoot
4:
mov $teststr,%rdi
mov %al,(%rdi)
mov $INTA0_PRINTF,%rax
int $0xA0
3:

    jmp     1b

blockbuffer:
    .skip (5*512)

mainkernelthread_end:
teststr: .ASCIZ " \r\n"


// ====================== THIS IS A TEST THREAD
tr2:
    mov     %rdx,%rdi
    call    testThread
tr2_end:

