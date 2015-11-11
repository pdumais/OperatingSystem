#include "macros.h"
#include "../memorymap.h"

.global initHardware

.extern launchSoftIRQThread
.extern net_process



initHardware:
    push    %rdi
    push    %rsi

    mov     $softirqthread,%rdi  
    mov     $(softirqthread_end-softirqthread),%rsi           
    call    launchSoftIRQThread

    call    initIntA0

    pop     %rsi
    pop     %rdi
    ret



/////////////////////////////////////////////////////////////////////////////////////////
// The softIRQ thread is a thread that sleeps all the time until
// an IRQ handlers wakes it up. In order to avoid heavy processing inside
// an IRQ handler, the IRQ handler will set one of the 64 possible softIRQ 
// flag and re-enable the softIRQ thread so that it gets executed upon the
// next schedule. 
//
// The 64bit bitfield located at $SOFTIRQLIST represents the enabled softIRQ
// (ie: bit0 represents softIRQ0. When the softIRQ thread runs, it will
//  call the handlers for all softIRQ that are enabled and will disable the bit
//
// The softIRQ thread is a special thread (created by createSoftIRQThread) 
// because the scheduler know about it. The scheduler will schedule
// that thread if the softIRQ flags is non-zero. This thread
// would have priority since the scheduler wont even bother to look
// for a task in the task list if softIRQ needs to run
//
// TODO: Currently, the thread has hardcoded handlers. softIRQ0
//       will invoke net_process. But it should be possible for
//       drivers to register their softIRQ.
// TODO: softirq should get prioritized: would need a scheduler prioritization scheme.
/////////////////////////////////////////////////////////////////////////////////////////

softirqthread:
// we first copy the value to rax and atomically clear the softirq variable. if another
// IRQ comes in after that, it doesn't matter since we will yield and the scheduler will 
// give us time again.
clearIRQList:
    mov     $0,%rbx
    mov     $SOFTIRQLIST,%r15
    mov     (%r15),%rax
    lock    cmpxchg     %rbx,(%r15) //TODO: we need the lock, but do we need to loop?
    jnz     clearIRQList
    mov     %rax,%r15               // now r15 contains the original value of the softIRQ flags
    cmp     $0,%r15
    jne     checkIRQs
    call    yield
    jmp     clearIRQList
checkIRQs:
    bt      $SOFTIRQ_NET,%r15                 // SOFTIRQ_NET
    jnc     no_softirq0
    call    net_process
no_softirq0:
no_softirq63:
    jmp     clearIRQList
sit: .ASCIZ "soft irq\r\n"
softirqthread_end:


