#include "../memorymap.h"
#include "macros.h"
#include "config.h"

.global setupIDT
.global loadIDT
.global registerInterruptGate
.global registerTrapGate

.extern debug_writenumber_dangerous
.extern ackAPIC
.extern getPTEntry

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Interrupt Stack Frame 
//      -- Stack Bottom -- 
//      Error Code (for some exceptions)
//      RIP
//      CS
//      EFLAGS
//      RSP
//      SS
//      -- Stack Top --
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// setupIDT: Initial preparation of the IDT
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
setupIDT:
    push    %rax
    push    %rbx
    push    %rcx
    push    %rdi

    // 32 first slots are reserved for exception handlers
    mov     $IDTSPACE,%rdi
    mov     $32,%rcx
    mov     $InterruptHandlers,%rax
1:  //prepareExceptions
    mov     (%rax),%rbx
    movw    %bx,(%rdi)
    movw    $0x18,2(%rdi)           // ring0 selector since code resides in ring0
    mov     %rbx,4(%rdi)
    movw    $0xEF00,4(%rdi)         // Ef: Trap gate with DPL=3. Those dont clear IF when entring handler
    movl    $0,12(%rdi)
    add     $16,%rdi
    add     $8,%rax
    loop    1b

    pop     %rdi
    pop     %rcx
    pop     %rbx
    pop     %rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: registerInterruptGate(rax=vector,rbx=handler_adress)
//  Int gates will disable interrupts on entry
////////////////////////////////////////////////////////////////////////////////////
/////////////////G///////////////////////////////////////////////////////////////////
registerInterruptGate:
    push    %rdi
    shl     $4,%rax                   // rax = rax*16
    leaq    IDTSPACE(%rax),%rdi    // rdi=IDTSPACE+rax*1
    movw    %bx,(%rdi)
    movw    $0x18,2(%rdi)
    mov     %rbx,4(%rdi)
    movw    $0xEE00,4(%rdi)         //DPL=3
    movl    $0,12(%rdi)
    pop     %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: registerTrapGate(rax=vector,rbx=handler_adress)
//  Trap gatew won't change change the IF flag upon entry
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
registerTrapGate:
    push    %rdi
    shl     $4,%rax                   // rax = rax*16
    leaq    IDTSPACE(%rax),%rdi    // rdi=IDTSPACE+rax*1
    movw    %bx,(%rdi)
    movw    $0x18,2(%rdi)
    mov     %rbx,4(%rdi)
    movw    $0xEF00,4(%rdi)         //DPL=3
    movl    $0,12(%rdi)
    pop     %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: loadIDT
// 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
loadIDT:
    push    %rax
    mov     $IDTRegister,%rax
    lidt   (%rax)
    pop     %rax
    ret
.align 4
IDTRegister:
    .WORD 0x1000
    .QUAD IDTSPACE


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// These are the exception handlers table that will be referenced in IDT.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 4
InterruptHandlers:
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad int3Handler
    .quad defaultHandler
    .quad defaultHandler
    .quad int6Handler
    .quad deviceNotAvailableHandler
    .quad defaultHandlerWithError
    .quad defaultHandler
    .quad defaultHandlerWithError
    .quad defaultHandlerWithError
    .quad defaultHandlerWithError
    .quad exceptionDHandler
    .quad exceptionEHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandlerWithError
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandler
    .quad defaultHandlerWithError
    .quad defaultHandler


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Default exception handler that does not expect an error code
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 16
defaultHandler:
    // push a fake error code since crashDump will expect one on the stack
    pushq       $0  
    push        %rax
    mov         $0xFF,%rax
    jmp         crashDump

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Default exception handler that expects an error code
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 16
defaultHandlerWithError:
    push        %rax
    mov         $0xFF,%rax
    jmp         crashDump


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Exception 0x0D: General Protection Failure
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 16
exceptionDHandler:
    push    %rax
    mov     $0x0D,%rax
    jmp     crashDump


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Exception 0x0E: Page Fault
//  In here, we will try to recover from the fault by allocating a page
//  for the offending virtual address if it is for stack. Otherwise, let
//  thread crash.
//
//  The page fault handler is not the most efficient one. It blindly allocates
//  a physical page to unallocated virtual addresses. The only thing it checks
//  is if the address resides in the page guard between heap and stack.
//  This should be modified to detect if a heap operation overflowed
//  on the stack or if a stack operation overflowed in the heap.
//  We should also make sure that a page fault in the heap for an
//  address that is not part of a "isuse" block does not get allocated.
//  Problems with this algorithm:
//      - Touching memory below the guard page while trying to access memory
//        (stack overflow) will allocate a page in heap. And we wont track it.
//        ie: char test[(1024*1024*20)+4097]; test[0]=1;
//      - Touching memory above the guard page during heap operation will
//        allocate a page in stack. ie: char* c = malloc(X); c[10000]=1
//        where c+10000 would be in the stack.
//      - Pages allocated in stack will not be freed until the process dies.
//        so if a large buffer was created initially and not touched for
//        the rest of program execution, a lot of pages will be allocated for
//        nothing.
//      - Touching heap memory outside of a buffer created with malloc will
//        create a page and it won't be freed until the process dies.
//
//  For these reasons we should do some garbage collecting of some sort.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 16
exceptionEHandler:
    push        %rbp
    lea         8(%rsp),%rbp         
    push        %rax
    push        %rdi
    push        %rbx

    // Check the error code pushed on the stack.
    // If the page was already present, then this is a permission error
    // Dont check if it was a user access because maybe the buffer was passed
    // to a system call. 
    mov         (%rbp),%rax
    bt          $0,%rax
    jc          failPF

    // Make sure that the address is within stack range or heap range
    // because these are the only two areas where we dynamically 
    // allocate pages
    mov         %cr2,%rax
    mov         $THREAD_CODE_START,%rbx
    cmp         %rbx,%rax
    jb          failPF   
    mov         $STACK3TOP_VIRTUAL_ADDRESS,%rbx
    cmp         %rbx,%rax
    jae         failPF   
    mov         $HEAP_TOP,%rbx
    cmp         %rbx,%rax
    jb          1f
    mov         $STACK3BOTTOM_VIRTUAL_ADDRESS,%rbx
    cmp         %rbx,%rax
    jae         1f
    jmp         failPF

    // Get the PTE for that address
1:  mov         %rax,%rdi
    call        getPTEntry
    mov         (%rax),%rbx         // rax = PTE address for faulty address
    mov         %rax,%rbx

    mov         $1,%rdi
    call        allocateStackPage
    
    // make the mapping in the page table
    or          $0b001000000111,%rax
    mov         %rax,(%rbx)     

    pop         %rbx
    pop         %rdi
    pop         %rax
    pop         %rbp
    add         $8,%rsp     // overwrite error
    iretq

failPF:
    pop         %rbx
    pop         %rdi
    pop         %rax
    pop         %rbp
    push        %rax
    mov         $0x0E,%rax
    jmp         crashDump



////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Exception 0x03: Debug Exception
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 16
int3Handler:
    pushq       $0  // push a fake error code
    push    %rax
    mov     $3,%rax
    jmp     crashDump

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Exception 0x06: Invalid Opcode
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 16
int6Handler:
    pushq       $0  // push a fake error code
    push    %rax
    mov     $6,%rax
    jmp     crashDump


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Exception 0x07: Device Not Available
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
.align 16
deviceNotAvailableHandler:
    push    %rdx
    push    %rax
    push    %rbx
    push    %rcx
    push    %rdi

    // clear the TS flag
    mov     %cr0,%rax
    and     $0xFFFFFFFFFFFFFFF7,%rax
    mov     %rax,%cr0

    // get previous save ares
    mov     lastMediaSaveArea,%rbx

    // get physical address of current SAVE_AREA    
    mov     $AVX_SAVE_AREA,%rdi
    call    getPageDescriptorAddress
    mov     (%rax),%rcx
    and     $0xFFFFFFFFFFFFF000,%rcx

    // is it the same context again?
    cmp     %rbx,%rcx
    je      sameContext
    
    mov     $0b110,%rax //implicitely used by xsave/xrestor
    mov     $0,%rdx //implicitely used by xsave/xrestor

    cmp     $0,%rbx
    jz      noLastContext // this is the first time we run, so don't save last context
    
    xsave   (%rbx)   // save into the last save area, which is the thread that executed the last media instruction
noLastContext:
    mov     %rcx,lastMediaSaveArea // indicate that this thread is the last one that executed a media instruction
    xrstor  (%rcx)

sameContext:
    pop     %rdi
    pop     %rcx
    pop     %rbx
    pop     %rax
    pop     %rdx
    iretq
lastMediaSaveArea: .QUAD 0


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Crash Dump: This is where handlers end up when needing to stop execution 
//             and details about the event.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
#define REGPOS(col,row) (7+(col*23)+(80*(row+2)))
#define BACKCOLOR (0x74)
#define EXNAMEPOS ((17+80+80))
crashDump:
    push    %rbp
    mov     %rsp,%rbp
    push    %rax
    push    %rdi
    push    %rsi
    push    %rcx
    mov     $0xB8000,%rdi
    mov     $crashDumpString,%rsi
    mov     $crashDumpString_end-crashDumpString,%rcx
    mov     $BACKCOLOR,%ah
1:  mov     (%rsi),%al
    mov     %ax,(%rdi)
    add     $2,%rdi
    inc     %rsi
    loop    1b  


    // Display Exception Name
    mov     -8(%rbp),%rsi        // exception number
    shl     $5,%rsi
    add     $exceptionNames,%rsi
    mov     $32,%rcx
    mov     $((EXNAMEPOS*2)+0xB8000),%rdi
1:  mov     (%rsi),%al
    mov     %al,(%rdi)
    add     $2,%rdi
    inc     %rsi
    loop    1b

    mov     -8(%rbp),%rdi        // exception number
    mov     $(12+(80*2)),%rsi
    mov     $2,%rcx
    call    printReg    

    mov     8(%rbp),%rdi        // rax
    mov     $REGPOS(0,1),%rsi
    mov     $16,%rcx
    call    printReg    

    mov     %rbx,%rdi
    mov     $REGPOS(1,1),%rsi
    mov     $16,%rcx
    call    printReg    

    mov     -32(%rbp),%rdi
    mov     $REGPOS(2,1),%rsi
    mov     $16,%rcx
    call    printReg    
  
    mov     %rdx,%rdi           // rdx
    mov     $REGPOS(0,2),%rsi
    mov     $16,%rcx
    call    printReg

    mov     -16(%rbp),%rdi
    mov     $REGPOS(1,2),%rsi
    mov     $16,%rcx
    call    printReg

    mov     -24(%rbp),%rdi
    mov     $REGPOS(2,2),%rsi
    mov     $16,%rcx
    call    printReg

    mov     (%rbp),%rdi         // rbp
    mov     $REGPOS(0,3),%rsi
    mov     $16,%rcx
    call    printReg

    mov     48(%rbp),%rdi       // rsp
    mov     $REGPOS(1,3),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r8,%rdi
    mov     $REGPOS(2,3),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r9,%rdi    
    mov     $REGPOS(0,4),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r10,%rdi 
    mov     $REGPOS(1,4),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r11,%rdi
    mov     $REGPOS(2,4),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r12,%rdi        
    mov     $REGPOS(0,5),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r13,%rdi
    mov     $REGPOS(1,5),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r14,%rdi
    mov     $REGPOS(2,5),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %r15,%rdi
    mov     $REGPOS(0,6),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %cr2,%rax
    mov     %rax,%rdi
    mov     $REGPOS(1,6),%rsi
    mov     $16,%rcx
    call    printReg

    mov     %cr3,%rax
    mov     %rax,%rdi
    mov     $REGPOS(2,6),%rsi
    mov     $16,%rcx
    call    printReg

    mov     24(%rbp),%rax           // rip
    mov     %rax,%rdi
    mov     $REGPOS(0,7),%rsi
    mov     $16,%rcx
    call    printReg

    mov     16(%rbp),%rdi           // Error Number
    mov     $REGPOS(1,7),%rsi
    mov     $16,%rcx
    call    printReg    

    //Don't dump stack in case of page fault
    cmpq    $0x0E,-8(%rbp)
    je      noStackPrint

    mov     48(%rbp),%rsi       // rsp
    mov     $4+(2*11*80)+0xB8000,%rdi
    
    //TODO: should verify stack limit
    mov     $4, %rdx
    mov     $16,%rcx
lineLoop:
    mov     (%rsi),%al
    add     $1,%rsi
    mov     %al,%ah
    shr     $4,%ah
    and     $0x0F,%al
    add     $0x3030,%ax
    cmp     $0x39,%al
    jna     1f
    add     $7,%al
1:  cmp     $0x39,%ah
    jna     1f
    add     $7,%ah
1:  mov     %ah,(%rdi)
    mov     %al,2(%rdi)
    movb    $0x20,4(%rdi)
    add     $(2*3),%rdi
    loop    lineLoop 
    add     $(2*(80-(3*16))),%rdi
    mov     $16,%rcx
    dec     %rdx
    jnz     lineLoop
    

noStackPrint:
    STALL()

// rdi=val
// rsi=address to write to
// rcx=digits
printReg:
    push    %rax
    shl     $1,%rsi
    add     $0xB8000-2,%rsi
    shl     $1,%rcx
    add     %rcx,%rsi     
    shr     $1,%rcx
1:  mov     %rdi,%rax
    and     $0x0F,%rax
    add     $0x30,%al
    cmp     $0x39,%al
    jna     2f
    add     $7,%al
2:  mov     %al,(%rsi)
    shr     $4,%rdi
    sub     $2,%rsi
    loop    1b
    pop     %rax
    ret

crashDumpString:
.ASCII "################################################################################"
.ASCII "#                                                                              #"
.ASCII "# EXCEPTION 00 -                                                               #"
.ASCII "# RAX: 0000000000000000  RBX: 0000000000000000  RCX: 0000000000000000          #"
.ASCII "# RDX: 0000000000000000  RDI: 0000000000000000  RSI: 0000000000000000          #"
.ASCII "# RBP: 0000000000000000  RSP: 0000000000000000  R8:  0000000000000000          #"
.ASCII "# R9:  0000000000000000  R10: 0000000000000000  R11: 0000000000000000          #"
.ASCII "# R12: 0000000000000000  R13: 0000000000000000  R14: 0000000000000000          #"
.ASCII "# R15: 0000000000000000  CR2: 0000000000000000  CR3: 0000000000000000          #"
.ASCII "# RIP: 0000000000000000  ERR: 0000000000000000                                 #"
.ASCII "# STACK:                                                                       #"
.ASCII "#                                                                              #"
.ASCII "#                                                                              #"
.ASCII "#                                                                              #"
.ASCII "#                                                                              #"
.ASCII "#                                                                              #"
.ASCII "################################################################################"
crashDumpString_end:
exceptionNames:
.ASCII "Divide By Zero                  "
.ASCII "Debug                           "
.ASCII "NMI                             "
.ASCII "BreakPoint                      "
.ASCII "Overflow                        "
.ASCII "                                "
.ASCII "Invalid Opcode                  "
.ASCII "Device Not Available            "
.ASCII "Double Fault                    "
.ASCII "                                "
.ASCII "Invalid TSS                     "
.ASCII "Segment Not Present             "
.ASCII "Stack Segment Fault             "
.ASCII "General Protection Fault        "
.ASCII "Page Fault                      "
.ASCII "                                "
.ASCII "x87 FPU                         "
.ASCII "Alignment Check                 "
.ASCII "Machine Check                   "
.ASCII "SIMD                            "
.ASCII "Virtualization                  "
.ASCII "                                "
.ASCII "                                "
.ASCII "                                "
.ASCII "                                "
.ASCII "                                "
.ASCII "                                "
.ASCII "                                "
.ASCII "                                "
.ASCII "                                "
.ASCII "Security                        "
.ASCII "                                "

