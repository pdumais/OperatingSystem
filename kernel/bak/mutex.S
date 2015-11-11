.global mutexLock
.global mutexUnlock
.extern writeNumber


/*
note that this is not very performant. Instead of just yielding, the thread trying to
lock the mutex should put its ID in a waiting queue and yield.
The scheduler should not run threads that are in a waiting queue
The thread that unlocks the mutex, should yield its time slice to a thread
that is in the mutex's waiting queue
*/

mutexLock:          /* rdi = mutext address*/
    push    %rbx
    push    %rax
    mov     $1,%rbx
    jmp     2f
1:  int     $0x40               /* Yield */
2:  mov     (%rdi),%rax
    cmp     $0,%rax
    jnz     1b 
    lock    cmpxchg %rbx,(%rdi) /*at this point, rax=0. if (%rdi)==rax, then move rbx in (%rdi)*/
    jnz     1b
    pop     %rax
    pop     %rbx
    ret
    
mutexUnlock:       /* rdi = mutext address*/
    push    %rax
    mov     $0,%rax
    mov     %rax,(%rdi)
//    int     $0x40     /* Yield. abort current execution to give a chance to another thread to lock the mutex */
    pop     %rax
    ret

