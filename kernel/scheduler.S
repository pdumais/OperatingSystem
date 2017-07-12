#include "../memorymap.h"
#include "pit.h"
#include "macros.h"
#include "includes/kernel/intA0.h"
#include "tasks.h"

/*
        The scheduler's task list will contain the physical address
        of the PML4 table. All data will be saved on stack
        so the scheduler will just push/pop to change context after having
        restored the proper physical location of the page. but we must also
        store "rsp" in the task list

    Task list structure: 1024 entries of 16bytes each (16k) located at TASKLIST_BASE
    Each entry is structured like this:
        0..63:      RSP value
        64..127:    PML4 address for the task. 

*/
.global addTaskInList
.global createProcessStub
.global schedulerHandler
.global initScheduler
.global launchSoftIRQThread
.global launchKernelThread
.global getTicksSinceBoot
.global setSoftIRQ
.global schedule
.global killCurrentTask
.global cleanOneDeadTask
.global waitProcessDeath
.extern writeNumber
.extern allocateStackPage
.extern returnfromIRQ
.extern ackAPIC


// We should be passing a 128bit value to invpcid
// but we will only push 64bit because the upper
// 64bit will be ignored anyway when using operation "1"
#define INVPCID_REAL(PCIDREG) push    PCIDREG;\
    mov     $1,PCIDREG;\
    invpcid (%rsp),PCIDREG;\
    pop     PCIDREG

#define INVPCID_EMULATE(PCIDREG) push    %rdx;\
    mov     PCIDREG,%rdx;\
    call    emulate_invpcid;\
    pop     %rdx

// Use INVPCID_EMULATE if the CPU does not support invpcid
#define _INVPCID(PCIDREG) INVPCID_REAL(PCIDREG)

.code64

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: waitProcessDeath(rdi=processID)
// will yield the current thread until the requested process dies.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
waitProcessDeath:
    push        %rdi
    push        %r15
    push        %r14

    //TODO: this is not reliable since that task ID could get recycled
    // during a context switch and we would never notice it.

1:  call        yield  
    mov         %rdi,%r15
    call        findTask
    cmp         $0,%r14
    jz          1b

    pop         %r14
    pop         %r15
    pop         %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: setSoftIRQ(rdi=softirqNumber)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
setSoftIRQ:
    btsq    %rdi,SOFTIRQLIST 
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: initScheduler
// Intializes structures needed for scheduler
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
initScheduler:
    push    %rcx
    push    %rdi
    push    %rsi

    mov     $IPI_TLB_SHOOTDOWN_VECTOR,%rdi
    mov     $tlbShootDownHandler,%rsi
    call    registerIPIHandler

    movq    $0,SOFTIRQLIST
    mov     $TASKLIST_BASE,%rdi
    mov     $TASKLISTEND+1,%rcx
clearTaskList:
    movq    $0,(%rdi)
    add     $8,%rdi
    cmp     %rdi,%rcx
    jne     clearTaskList

    pop     %rsi
    pop     %rdi
    pop     %rcx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: tlbShootDownHandler(rdi=data,rsi=msgID,rdx=ApicID)
// Intializes structures needed for scheduler
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
tlbShootDownHandler:
    // No need to save registers in the IPI handlers
    _INVPCID(%rdi)
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: launchSoftIRQThread(rdi=entry-point, rsi=size, rdx=param to pass in rdx)
// 
// SoftIRQ is a special thread. It does not reside in the task list.
// Its task data resides in SOFTIRQTASKINFO (at the end of this file)
// The scheduler makes special things to look at that task before looking
// at the task list. For more information, look at the comments
// near the softirq handler code
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
launchSoftIRQThread:
    push    %rdi
    push    %rax
    call    createKernelThread
    mov     $SOFTIRQTASKINFO,%rax
    mov     %rdi,(%rax)
    movq    $STACK0TOP_VIRTUAL_ADDRESS-168,%rdi
    mov     %rdi,8(%rax)
    btsq    $TASK_ENABLE_BIT,(%rax)                  // enable the task now

    pop     %rax
    pop     %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: launchKernelThread(rdi=entry-point, rsi=size, rdx=param to pass in rdx)
// 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
launchKernelThread:
    push    %rdi
    push    %rsi
    call    createKernelThread
    call    addTaskInList
    pop     %rsi
    pop     %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: createKernelThread(rdi=entry-point, rsi=size, rdx=param to pass in rdx)
// 
// This is for kernel threads. The code will not be copied in another
// area, it will be kept local. The 4k PTEs will stay empty and be used
// as heap for hte kernel thread
// This is multi-thread and multi-processor friendly for allocating memory
// and also for manipulating task list.
//
// Returns rdi = PML4 address for thread
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
createKernelThread:
    push    %rbx
    push    %rcx
    push    %rax
    push    %r9
    push    %r8
    mov     %rdi,%rbx // we need it to set initial RIP in stack of thread

    // Create 2 stack pages. The function returns (in rax)
    // the physical address to the begining of the block
    // The 2nd page is used for process meta-data.
    mov     $5,%rdi   
    call    allocateStackPage
    mov     %rax,%rdi           
    add     $0x4000,%rdi     // points to meta data page, after stack

    // Create a page structure somewhere in memory
    // the function will return the base address of the PML4 
    // and the address of the first 4k PTE.
    // the PTE entries are for the process's 4k pages that
    // start after kernel memory
    call    setupProcessPageStructure
    // At this point. 
    //      r8 = base address of the PML4
    //      r9 = adress of first PTE entry.
    //      The PTE entries are empty (not mapped)

    // map the stack page to physical address we got earlier in rax 
    // At this point rax=physical address of stack block that we got above.
    push    %rdi
    push    %rsi

    mov     %rax,%rsi
    mov     %r9,%rdi
    btsq    $MIRROR_BIT,%rdi        // set bit38 to add 256gig for identity mapping
    add     $STACK_ADDRESS_PAGE_TABLE_OFFSET-(3*8),%rdi
    mov     $4,%rcx
    call    mapMemory

    mov     %rax,%rsi
    add     $0x4000,%rsi
    mov     %r9,%rdi
    btsq    $MIRROR_BIT,%rdi        // set bit38 to add 256gig for identity mapping
    add     $META_ADDRESS_PAGE_TABLE_OFFSET,%rdi
    mov     $1,%rcx
    call    mapMemory


    // create firt heap page
    add     $0x4000,%rax        // now rax= meta page
    movq    $KERNEL_RESERVED_END,(PROCESS_HEAP_ADDRESS-META_VIRTUAL_ADDRESS)(%rax)
    mov     $1,%rdi
    call    allocateHeapPage
    push    %rax
    mov     %rax,%rsi
    mov     %r9,%rdi
    btsq    $MIRROR_BIT,%rdi
    add     $0,%rdi    // Useless but shows that the heap starts at 0x08000000 since 
    mov     $1,%rcx   // kernel threads are not relocated.
    call    mapMemory
    pop     %rdi
    btsq    $MIRROR_BIT,%rdi
    call    init_heap
    pop     %rsi
    pop     %rdi

    // So at this point, META page and first Stack page are mapped.

    // This is the stack data for the IRETQ that will occur after the timer iterrupt
    // when the task will be scheduled
    // rdi = end of stack (and begining of meta) that we calculated earlier
    btsq    $MIRROR_BIT,%rdi        // set bit38 to add 256gig for identity mapping
    movq    $0,-8(%rdi) /* ss */
    movq    $(STACK0TOP_VIRTUAL_ADDRESS),%rax
    movq    %rax,-16(%rdi)   /* rsp */
    movq    $0x200202,-24(%rdi)    /* rflags */ 
    movq    $0x18,-32(%rdi) /* CS */
    movq    %rbx,-40(%rdi) /* RIP */

    // This is the context data
    movq    $0xBBCCDD,-48(%rdi)    /* rax */
    movq    $0,-56(%rdi)    /* rdi */
    movq    $0,-64(%rdi)    /* rbx */
    movq    $0,-72(%rdi)    /* rcx */
    movq    %rdx,-80(%rdi)  /* rdx */
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
    movq    $-1,-168(%rdi)    /* VMCS */

    //  find first non-zero entry in task list
    //  Each tasklist entry is 16bytes long (128bit)
    //      The first 64bit of an entry is the PLM4 physical address of the thread 
    //          Since the PLM4 address is aligned on a 4k boundary, the lowest 12bits
    //          are free to use. bit0 being the enable flag
    //      The 2nd 64bit is the RSP
    mov     %r8,%rdi

    pop     %r8
    pop     %r9
    pop     %rax
    pop     %rcx
    pop     %rbx
    ret



////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: mapMemory(rdi=pagetableentry, rsi=phys_address, rcx=numpage
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
mapMemory:
    or      $0b001000000111,%rsi
1:  mov     %rsi,(%rdi)
    add     $8,%rdi
    add     $0x1000, %rsi
    loop    1b     
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// createProcessStub(rdi=codeArea, rsi=entryPoint);    
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
createProcessStub:
    push        %rax
    push        %rsi
    push        %rdi
    push        %rcx

    mov         %rsi,%rax
    mov         $userProcessLaunchStub,%rsi
    mov         $userProcessLaunchStub_end-userProcessLaunchStub,%rcx
    push        %rdi
    rep movsb
    pop         %rdi
    add         $userProcessLaunchStubAddress-userProcessLaunchStub,%rdi
    mov         %eax,(%rdi)
    
    pop         %rcx
    pop         %rdi
    pop         %rsi 
    pop         %rax 
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// createProcessStub(rdi=codeArea, rsi=entryPoint);    
// This code will be copied at the begining of a program memory. 
// THIS SHOULD NEVER GROW ABOVE 512 BYTES!
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
userProcessLaunchStub:
    mov     %rsp,%rbp   
    sub     $8,%rsp

    //if stealing another console is requested, rbx = process to steal from
    push    %rbx        
    push    %rdi

    cmp     $0,%rbx
    je      1f
    
    mov     %rbx,%rdi
    mov     $INTA0_STEALTEXTCONSOLE,%rax
    int     $0xA0
    mov     %rax,-8(%rbp)
    jmp     2f

    // Create a console for that process
1:  mov     $INTA0_CREATETEXTCONSOLE,%rax
    int     $0xA0

2:
    // this is a "mov X,%rax". It is machine coded so that we can change the 
    // entry point address below.
    .byte   0x48,0xC7,0xC0    //mov %rax
userProcessLaunchStubAddress:
    .long   0x00  // Warning: cant use a 64bit address since this is a direct load reigster
    pop     %rdi

    call    %rax 


//TODO: we should do this in handle destructor. And invoke the destructor
    pop     %rbx
//    cmp     $0,%rbx
//    je      1f
//    mov     -8(%rbp),%rdi
//    mov     $INTA0_RESTORETEXTCONSOLE,%rax
//    int     $0xA0

1:  mov     $INTA0_KILL_CURRENT_TASK,%rax
    int     $0xA0

    // At this point, the dead bit is set. If we get preempted, 
    // we will never come back
    // We will just spin and wait to get preempted. Not very efficient though
    1:jmp 1b
userProcessLaunchStub_end:

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: addTaskInList(rdi=task_PML4_address)
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
addTaskInList:
    push    %rcx
    push    %r8
    push    %rax
    push    %rbx
    push    %rdx
    
1:  mov     $TASKLIST_BASE,%r8
    mov     $(TASKLISTEND+1),%rcx
2:  mov     (%r8),%rax
    cmp     $0,%rax
    je      4f
    add     $16,%r8
    cmp     %r8,%rcx
    je      addTaskError
    jmp     2b
4:
    // we are going to store the PML4 address (in rdi) in the task slot
    // we will OR it with the tasklist index. These 12 lower bits
    // will become the PCID for the TLB
    // When we move to a "multiple threads per process" model, the id 
    // should be the index of the process table. A new process table would 
    // exist, but the scheduler would use the task list.
    // It's not a good idea to find the task by using the index in cr3 since 
    // with the new model, the index would represent the process index 
    // and NOT the thread index."
#ifdef PCID
    mov     %r8,%rdx            
    sub     $TASKLIST_BASE,%rdx
    shr     $4,%rdx
    inc     %rdx
    cmp     $0xFFF,%rdx             // to make sure, should not be more than 12 bits
    ja      addTaskError
    or      %rdx,%rdi
#endif

    lock    cmpxchg %rdi,(%r8)
    jnz     1b                       // cmpxchg failed, start over again


#ifdef PCID
    // Now invalidate all pages for that PCID in case we are recyling it
    // rdx was set to the PCID
    _INVPCID(%rdx)

    push    %rsi
    mov     $IPI_TLB_SHOOTDOWN_VECTOR,%rdi
    mov     %rdx,%rsi
    mov     $0,%rdx
    call    sendIPI
    pop     %rsi

#endif


    movq    $STACK0TOP_VIRTUAL_ADDRESS-168,%rbx
    mov     %rbx,8(%r8)
    mov     (%r8),%rbx
    btsq    $TASK_ENABLE_BIT,%rbx
    mov     %rbx,(%r8)     // set the Enabled flag
3:  pop     %rdx
    pop     %rbx
    pop     %rax
    pop     %r8
    pop     %rcx
    ret

addTaskError:
    int     $3

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: getTicksSinceBoot
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
getTicksSinceBoot:
    push    %rdx

    rdtsc
    shl     $32,%rax
    shrd    $32,%rdx,%rax

    pop     %rdx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: lockTaskList
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
lockTaskList:
1:  cmpq        $0, TASKLISTLOCK
    jnz         1b
    lock btsq   $1, TASKLISTLOCK
    jc          1b
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: unlockTaskList
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
unlockTaskList:
    movq        $0, TASKLISTLOCK
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: checkSoftIRQTask
// return rax= task entry or 0 if no need to run the task
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
checkSoftIRQTask:
    cmpq        $0,SOFTIRQLIST
    jz          noNeedForSoftIRQTask
   
    // check if the task is already running, and set the running bit
    mov         $SOFTIRQTASKINFO,%rax
//orb $(1<<5),7(%rax) //pathack
    lock btsq   $TASK_RUNNING_BIT,(%rax)
    jc          noNeedForSoftIRQTask        // it was already running
    ret   
noNeedForSoftIRQTask:
    mov         $0,%rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// killCurrentTask
// Will set the "dead" bit in the task list for the current task
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
killCurrentTask:
    mov         %cr3,%r15       //findtask will clean the address
    call        findTask
    cmp         $0,%r14
    jnz         cantKillTask

    // We lock the the memory because another thread might be
    // trying to write to this area. But we dont care
    // if the flag was already set or not.
    lock btsq    $TASK_DEAD_BIT,(%r15)
    ret
cantKillTask:
    // What can we do? Crash it with a breakpoint for now
    int         $3

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// cleanOneDeadTask()
// RETURN: rax = PML4 address of that task 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
cleanOneDeadTask:
    push        %rdi

    mov         $TASKLIST_BASE,%rdi
1:  movq        (%rdi),%rax

    btq         $TASK_DEAD_BIT,%rax
    jnc         3f
    btq         $TASK_RUNNING_BIT,%rax
    jnc         foundOneDeadTask
3:  add         $16,%rdi
    cmp         $(TASKLISTEND+1),%rdi
    jb          1b

    xor         %rax,%rax
    pop         %rdi
    ret

foundOneDeadTask:
    movq        $0,(%rdi)    
    CLEANCR3ADDRESS(%rax)
    pop         %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: findTask(r15=PML4 address of requested Task)
// return: r15=address of current task in tasklist or -1 if not found
//         r14: 0: if address is valid
//              1: if task not found
//              2: if task is a CPU park
//              Errors with bit 7 set mean that the address is valid but the state
//              of the task is not:
//              128: if curent task is dead
// TODO: not very efficient to go through the whole list
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
findTask:
    push        %rdi
    push        %rcx
    push        %rax

    CLEANCR3ADDRESS(%r15)
    // Check if this is for a parked CPU
    mov         $2,%r14
    cmp         $PML4TABLE,%r15         // shortcut: kernel page table is not in the list
    jz          invalidTask

    // Now check if it's in the task list
    mov         $2,%r14
    mov         $TASKLIST_BASE,%rdi
    mov         $TASKLISTEND+1,%rcx
1:  movq        (%rdi),%rax
    CLEANCR3ADDRESS(%rax)
    cmp         %rax,%r15
    jz          foundCurrentTask
    add         $16,%rdi
    cmp         %rdi,%rcx
    jne         1b

    // At this point, no task was found. Check the softIRQ task then
    mov         $1,%r14
    movq        $SOFTIRQTASKINFO,%rdi
    movq        (%rdi),%rax
    CLEANCR3ADDRESS(%rax)
    cmp         %rax,%r15
    jz          foundCurrentTask

invalidTask: 
    // r14 should have been set correctly to non-zero before going here
    // We return an invalid task if
    //  - The current task could not be found
    //  - The current task is dead
    //  - The current task the root task (cpu parked, or after boot)
    pop         %rax
    pop         %rcx
    pop         %rdi
    ret

foundCurrentTask:
    mov         %rdi,%r15         
    // Check if the task is dead
    mov         $128,%r14
    btq         $TASK_DEAD_BIT,(%rdi)
    jc          invalidTask
    mov         $0,%r14
    pop         %rax
    pop         %rcx
    pop         %rdi
    ret


TASKLISTLOCK: .quad 0x00

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: selectNextTask
// Return: 
//          rax=address in task list (not index). bit 63 is set if no task were found
// TODO: should not loop through entire list if there are no more records
// TODO: having 2 tasks in the list would have the effect of tasks bouncing
//       arround on multiple CPUs. should try to run them on the same one.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
selectNextTask:
    push        %rcx
    push        %rdx
    push        %rbx
    
    // Lock the task list so no other CPU can mess with it.
    call        lockTaskList

    // 1) get current pointer
    mov         TASKLISTINDEX,%rbx                      // get current entry

    // 2) find next that is not running and is ready, and not dead
    xor         %rdx,%rdx
    mov         $(((TASKLISTEND+1)-TASKLIST_BASE)>>4)+1,%rcx// record count
1:  add         $16,%rbx                                // next Entry
    cmp         $((TASKLISTEND+1)-TASKLIST_BASE),%rbx    // if we are at the end, wrap around
    cmoveq      %rdx,%rbx
    dec         %rcx                                    // did we loop through list completely?
    jz          noTaskFound
    mov         TASKLIST_BASE(%rbx),%rax
    btq         $TASK_ENABLE_BIT,%rax                                 // is it enabled?
    jnc         1b
    btq         $TASK_RUNNING_BIT,%rax                                 // is it already running?
    jc          1b
    btq         $TASK_DEAD_BIT,%rax                                 // is it Dead?
    jc          1b

    // At this point, rbx is the index to the task ready to run
    // 3) update pointer
    btsq        $TASK_RUNNING_BIT,%rax                                 // set this task as running
    mov         %rax,TASKLIST_BASE(%rbx)
  
    lea         TASKLIST_BASE(%rbx),%rax
    mov         %rbx,TASKLISTINDEX  

1:  call        unlockTaskList
    pop         %rbx        
    pop         %rdx
    pop         %rcx
    ret
noTaskFound:
    btsq        $63,%rax
    jmp         1b

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: schedulerHandler
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
schedulerHandler:
    push        %rax

    // we first check if we should run a softIRQ. If not
    // then proceed with checking the current task count and
    // selecting the next task to run
    call        checkSoftIRQTask
    cmp         $0,%rax
    jnz         2f

    // SoftIRQ does not need to run so check task counter
    // decrease task quantum and only switch when count is down to zero
    mov         %cr3,%rax
    CLEANCR3ADDRESS(%rax)
    cmp         $PML4TABLE,%rax     // if current task is a parked task, dont check 
                                    // counter. it means
    jz          1f                  // that this is the initial switch or the cpu is parked.
    mov         $TIME_SLICE_COUNT,%rax 
    decq        (%rax)
    jz          1f                  // if the count drops to 0, switch. Otherwise, exit handler.
    pop         %rax
    call        ackAPIC
    iretq

1:  call        selectNextTask
    // At this point, rax=address of next task
2:  PUSHAEXCEPTRAX                  // rax was pushed at handler entrance

    // Flush current vmcs in case this thread is running a VM
    mov         %rsp,%rdi
    sub         $8,%rdi
    vmptrst     (%rdi)
    mov         (%rdi),%r15
    push        %r15                // now VMCS is pushed in context
    cmpq        $-1,(%rdi)
    je          1f
    vmclear     (%rdi)
1:

    // This is bad for performance. Instead of looping through the list, the current thread
    // should remember its slot and and we could fetch it back right away
    mov         %cr3,%r15
    call        findTask;           // Find the current running task (since r15 is loaded with cr3)
    // At this point, r15=address of current task in table or -1 if not found

    btq         $63,%rax  // rax comes from selecteNextTask or checkSoftIRQTask
    jc          noTaskSwitch

    ////////////////////////////////////////////////
    // This is the "schedule" code
    // At this point we do have a task that
    // we wanna switch to. But if the current 
    // task is invalid (cpu parked or task killed)
    // then we should not save context
    ////////////////////////////////////////////////
    mov         %rax,%rdi
    cmp         $0,%r14                     // current task does not exist (cpu parked?)
    jnz         justLoadTaskContext         // so don't save context

    ////////////////////////////////////////////////
    // Save and load Context
    ////////////////////////////////////////////////
    mov         %rsp,8(%r15)                // rsp is now saved, so dont use it anymore
    mov         (%rdi),%r8                  // PML4
    mov         8(%rdi),%rsp                // rsp value when task switch occured
    STRIPCONTROLFROMCR3(%r8)
    mov         %r8,%cr3
    lock btrq   $TASK_RUNNING_BIT,(%r15)    // Now we write back the old task entry
                                            //  with the "not-running" flag.
                                            //  we waited until we updated cr3
                                            //  so that we can guarantee that
                                            //  it is set to not-running when its
                                            //  page tables are not used anymore.
    jmp     contextSwitchDone


    ////////////////////////////////////////////////
    // Just load next context and do not save any
    ////////////////////////////////////////////////
justLoadTaskContext:
    // By restoring PLM4 and RSP, we can now use that thread's stack to pop out all registers saved.
    mov         (%rdi),%r8                  // PML4 
    mov         8(%rdi),%rsp                // rsp value when task switch occured
    STRIPCONTROLFROMCR3(%r8)
    mov         %r8,%cr3

    // The the current task address is valid (could be a dead task)
    // update its "running" flag. see findTask for values of r14,r15
    btq         $7,%r14
    jnc         contextSwitchDone
    lock btrq   $TASK_RUNNING_BIT,(%r15)                   // Now we clear the running flag

contextSwitchDone:
    // set task switched flag to trigger exception on AVX usage
    mov     %cr0,%r8
    or      $0b1000,%r8
    mov     %r8,%cr0
    mov     $TIME_SLICE_COUNT,%rax
    movq    $TASK_QUANTUM_COUNTER,(%rax)

    // Restore process's VMCS
    mov         %rsp,%rdi
    cmp         $-1,(%rdi)
    je          1f
    vmptrld     (%rdi)
    jbe         cant_restore_vmcs
1:  add         $8,%rsp         // remove VMCS
    POPA
    call        ackAPIC
    iretq


noTaskSwitch:
    // At this point, we found no next task to execute. Only possible
    // explanation is that there are less tasks than the number of CPUs.
    // We must now check if the current task is valid. Maybe we are parked
    // in which case we should continue to be parked or that the current
    // task was killed so we should not execute it anymore.
    //
    // If the current task is a valid task, then we should just conitnue to
    // run it.
    //
    // r15 = address of current task info
    //       r14 is non-zero if the current task was terminated or
    //       if it was not found in the list (parked cpu)
    // TODO: if current task is softIRQ, and no task switch is scheduled 
    //       (hence why are here), the softIRQ will be rescheduled for nothing.
    //       this will waste CPU power, but would have no impact on performance
    mov         $TIME_SLICE_COUNT,%rax
    movq        $TASK_QUANTUM_COUNTER,(%rax)
    cmp         $0,%r14                
    jnz         parkCPU 

    mov         %rsp,%rdi
    cmp         $-1,(%rdi)
    je          1f
    vmptrld     (%rdi)
    jbe         cant_restore_vmcs
1:  add         $8,%rsp     
    POPA
    call        ackAPIC
    iretq
cant_restore_vmcs:
    int         $3

parkCPU:
    // Set back the kernel page tables, 
    mov         $PML4TABLE,%rax
    STRIPCONTROLFROMCR3(%rax)
    mov         %rax,%cr3
    btq         $7,%r14
    jnc         1f
    lock btrq   $TASK_RUNNING_BIT,(%r15)                   // Now we write back the old task entry
1:
    // set back the cpu's stack
    GET_APIC_ID(%eax)
    AP_STACK(%eax)
    mov         %rax,%rsp
    
    // re-enable interrupts and set some registers for debug and wait 
    mov         $0x11111111,%rax
    mov         $0x22222222,%rax
    mov         $0x33333333,%rax
    call        ackAPIC
    sti
    STALL()


TASKLISTINDEX: .quad -16

// SoftIRQTackInfo: This is a task entry just like the ones in the task list.
// quad: PML4 address
// quad: RSP0 value
SOFTIRQTASKINFO: .quad 0x00,0x00




