#include "../memorymap.h"
.global mutexLock
.global mutexUnlock
.global spinLock
.global spinUnlock
.global spinLock_softirq
.global spinUnlock_softirq
.global disableCurrentThread
.global enableThread
.global rwlockWriteLock
.global rwlockWriteUnlock
.global rwlockReadLock
.global rwlockReadUnlock
.extern writeNumber
.extern yield

// mutex with ticketing system.
// rdi= address to mutex
// mutex is a 64bit value representing:
// 63:32: next ticket
// 31:0 : current ticket
mutexLock:    
    push        %rax
    mov         $1,%rax
    lock xadd   %eax,(%rdi)
1:  cmp         %eax,4(%rdi)
    je          2f
    call        yield
    jmp         1b
2:  pop         %rax
    ret
    
mutexUnlock:       // rdi = mutext address
    lock incq   4(%rdi) 
    ret


// spinlock with ticketing systemnfo .
// rdi= address to mutex
// spinlock is a 64bit value representing:
// 63:32: next ticket
// 31:0 : current ticket
spinLock:
    push        %rax
    mov         $1,%rax
    lock xadd   %eax,(%rdi)
    // eax has been loaded with the value that was in (%rdi), now wait until
    // the second part becomes equal to that
1:  cmp         %eax,4(%rdi)
    jne         1b
    pop         %rax
    ret

spinUnlock:       // rdi = spinlock address
    lock incq   4(%rdi)
    ret


// spinlock with ticketing system.                                                    
// rdi= address to mutex                                                              
// spinlock is a 128bit value representing:                                            
// 127:64: flags status 
// 63:32 : next ticket                                                                 
// 31:0  : current ticket                                                              
// WARNING: This will disable interrupts. So this should not be used between
//          threads since they wont be preempted and won't give a chance
//          to concurrents to release the lock. but then again, it the
//          interrupts are cleared, then this thread will be guaranteed to
//          be able to release the lock without being preempted
spinLock_softirq:                                                                             
    push        %rax   
    pushfq
    pop         %rax
    mov         %rax,8(%rdi)
    cli                                                               
    mov         $1,%rax                                                               
    lock xadd   %eax,(%rdi)                                                           
1:  cmp         %eax,4(%rdi)                                                          
    jne         1b                                                                    
    pop         %rax                                                                  
    ret                                                                               
                                                                                      
spinUnlock_softirq:       // rdi = spinlock address                                           
    lock incq   4(%rdi)                                                               
    bt          $9,8(%rdi)
    jnc         1f
    sti
1:  ret                                      



////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// rwlockWriteLock(%rdi=lock)
// lock format:
//   63:32: readCount
//   31:00: writeLock
// It would be possible to waste time when this function waits for readers to be 0
// another reader count constantly inc/dec 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
rwlockWriteLock:
    // lock the write lock or wait if it is already.
1:  lock btsq   $0,(%rdi)
    jc          1b

    // Wait for readers to finish
1:  cmpl        $0,4(%rdi)
    jnz         1b      
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// rwlockWriteUnlock(%rdi=lock)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
rwlockWriteUnlock:
    btrq        $0,(%rdi)
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// rwlockReadLock(%rdi=lock)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
rwlockReadLock:
1:  lock incl   4(%rdi)
    
    bt          $0,(%rdi)
    jnc         3f    

    lock decl   4(%rdi)

    bt          $0,(%rdi)
    jnc         1b
    call        yield
    jmp         1b
    
3:  ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// rwlockReadUnlock(%rdi=lock)
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
rwlockReadUnlock:
    lock decl   4(%rdi)
    ret

