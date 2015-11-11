#include "../memorymap.h"

/*
        The scheduler's task list will contain the physical address
        of what the stack page should be. All data will be saved on stack
        so the scheduler will just push/pop to change context after having
        restored the proper physical location of the page. but we must also
        store "rsp" in the task list


    Task list structure: 1024 entries of 16bytes each (16k) located at TASKLIST_BASE
    Each entry is structured like this:
        0..63:      RSP value
        64..127:     Physical address of thread stack (only 21 bits necessary since we map 2mb pages)

*/

.global scheduler
.global initScheduler
.global launchThread

.extern writeNumber
.extern allocateStackPage

.code64
initScheduler:
    push    %rcx
    push    %rdi

    mov     $TASKLIST_BASE,%rdi
    mov     $TASKLISTEND+1,%rcx
    movq    $-16,(%rdi)             /* current task index = -1*/
    add     $16,%rdi
clearTaskList:
    movq    $0,(%rdi)
    add     $8,%rdi
    cmp     %rdi,%rcx
    jne     clearTaskList

    pop     %rdi
    pop     %rcx
    ret

// %rdi points to entry point of thread
// This is multi-thread and multi-processor friendly
launchThread:
//    pushf
    push    %rdi
    push    %rbx
    push    %rcx
    push    %rax
    mov     %rdi,%rbx

    // Create stack page
    call    allocateStackPage
    mov     %rax,%rdi           /* contains identiry map page adress of where stack will be physically */
    add     $STACKSIZE,%rdi     /* go to top of stack */

    
    // This is the stack data for the IRETQ that will occur after the timer iterrupt
    // when the task will be scheduled
    movq    $0,-8(%rdi) /* ss */
    movq    $STACK_VIRTUAL_ADDRESS,%rax
    addq    $STACKSIZE,%rax  
    movq    %rax,-16(%rdi)   /* rsp */
    movq    $0x200202,-24(%rdi)    /* rflags */ 
    movq    $0x18,-32(%rdi) /* CS */
    movq    %rbx,-40(%rdi) /* RIP */

    // This is the context data
    movq    $0xBBCCDD,-48(%rdi)    /* rax */
    movq    $0,-56(%rdi)    /* rbx */
    movq    $0,-64(%rdi)    /* rcx */
    movq    $0,-72(%rdi)    /* rdx */
    movq    $0,-80(%rdi)    /* rsi */
    movq    $0,-88(%rdi)    /* rdi */
    movq    $0,-96(%rdi)    /* rbp */
    movq    $0,-104(%rdi)    /* r8 */
    movq    $0,-112(%rdi)    /* r9 */
    movq    $0,-120(%rdi)    /* r10 */
    movq    $0,-128(%rdi)    /* r11 */
    movq    $0,-136(%rdi)    /* r12 */
    movq    $0,-144(%rdi)    /* r13 */
    movq    $0,-152(%rdi)    /* r14 */
    movq    $0,-160(%rdi)    /* r15 */
    mov     %rdi,%rbx
    sub     $STACKSIZE,%rbx

    //  find first non-zero entry in task list (skip 1st one)
    //  put rdi in it, and rsp=rdi+stacksize-160
    mov     $TASKLIST_BASE,%rdi
    mov     $TASKLISTEND,%rcx
    inc     %rcx
    add     $16,%rdi
searchTaskList:
    mov     (%rdi),%rax
    cmp     $0,%rax
    je      foundEmptyEntry
    add     $16,%rdi
    cmp     %rdi,%rcx 
    je      noneFound
    jmp     searchTaskList
foundEmptyEntry:
    lock    cmpxchg %rbx,(%rdi)   // this will only occur if (%rdi) is still equal to rax (which is zero)
    movq    $STACK_VIRTUAL_ADDRESS,%rbx
    addq    $STACKSIZE,%rbx
    sub     $160,%rbx
    mov     %rbx,8(%rdi)
noneFound:
    pop     %rax
    pop     %rcx
    pop     %rbx
    pop     %rdi
    ret

/*
The following registers will be automatically pushed on stack when interrupt triggers:
    RIP,CS (8bytes),rflags,rsp,ss (can be null)
These one needs to be done manually:
    rax,rbx,rcx,rdx
    rsi,rdi,rbp
    r8,r9,r10,r11,r12,r13,r14,r15

WARNING: accessing the task list in here is not multi-processor safe
*/
scheduler:
    push    %rax
    push    %rbx
    push    %rcx
    push    %rdx
    push    %rsi
    push    %rdi
    push    %rbp
    push    %r8
    push    %r9
    push    %r10
    push    %r11
    push    %r12
    push    %r13
    push    %r14
    push    %r15

    // before this function was invoked (jumped into), the carry flag was set
    // to indicate if this is IRQ0 invocation or not.
    setc    %al        /*save carry flag in r15 so we can look at it later*/
    movzx   %al,%r15

    mov     $TASKLIST_BASE,%rdi
    mov     $TASKLISTEND,%rdx
    inc     %rdx
    mov     (%rdi),%rbx             /* rbx is the "current" pointer */
    add     $16,%rdi                /* skip 1st entry, it is the "current" pointer */
    add     %rbx,%rdi
    mov     %rdi,%rbx               /* rbx=current entry. To know if we did a full loop*/
searchTaskList2:
    add     $16,%rdi                /*go to next*/
    cmp     %rdi,%rdx               /*if we are at the end, wrap around*/
    jne     nowrap
    mov     $TASKLIST_BASE,%rdi     /* wrap around */
    add     $16,%rdi                /* skip 1st entry, it is the "current" pointer */
nowrap:
    cmp     %rbx,%rdi               /* are we back to starting point? */
    je      noEntryFound
    mov     (%rdi),%rax             /* if this entry not null, use it */
    cmp     $0,%rax
    je      searchTaskList2
    mov     (%rdi),%r8              /* physical address of stack */
    mov     8(%rdi),%r9             /* rsp value when task switch occured */
    cmp     $0,%r9
    je      searchTaskList2         // if stored RSP value is null, it is probably
                                    // because there was a context switch while creating
                                    // that task. It is not finished being created so skip it.

    /* At this point:
        r8 = physical address of stack for new thread
        r9 = rsp value for new thread
        rdi = current taslk list entry pointer
    */
    //get phys addr of current stack
    mov     $STACK_VIRTUAL_ADDRESS,%rbx  /*restore the stack physical address and remap it.*/
    shr     $18,%rbx                    /* divide by 2meg, then mul by 8bytes per entry = >>21, <<3 = >> 18 */
    add     $PDTABLE,%rbx               /* rbx contains the PD entry address for stack page */
    mov     (%rbx),%rax
    and     $0xFFFFFFFFFFE00000,%rax        /* rax = phys addr of current stack */
    cmp     $0,%rax
    je      taskNotFound
    
    // Find task list entry using that physical address
    mov     $TASKLIST_BASE,%r10
    mov     $TASKLISTEND,%r11
    inc     %r11
searchTask:
    add     $16,%r10
    cmp     %r10,%r11
    je      taskNotFound
    cmp     (%r10),%rax
    jne     searchTask
    
    //store rsp in  that location 
    mov     %rsp,8(%r10)

taskNotFound:                       /* old task was not found. Just ignore it and dont save context */
    // load new context
    and     $0xFFFFFFFFFFE00000,%r8        /* 2meg pages, so clear the rest */
    or      $0b001010000111,%r8           /* mark it as a Stack page */
    mov     %r8,(%rbx)
    mov     $STACK_VIRTUAL_ADDRESS,%rbx
    invlpg  (%rbx)
    mov     %r9,%rsp                /* restore rsp as it was before the last context switch */

    // Store current task index in the 1st entry of task list
    mov     %rdi,%rbx
    sub     $TASKLIST_BASE,%rbx
    sub     $16,%rbx
    mov     $TASKLIST_BASE,%rdi
    mov     %rbx,(%rdi)

noEntryFound:
    // reload the carry flag from r15 before we pop back r15
    bt      $0,%r15

    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %r11
    pop     %r10
    pop     %r9
    pop     %r8
    pop     %rbp
    pop     %rdi
    pop     %rsi
    pop     %rdx
    pop     %rcx
    pop     %rbx

    // The carry flag, fetch back from r15 earlier, was set if this function was
    // called from IRQ0.
    jnc     notIRQ0
    mov     $0x20,%al
    outb    %al,$0x20
notIRQ0:
    pop     %rax
    iretq


