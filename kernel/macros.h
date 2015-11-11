#define GET_APIC_ID(reg) mov (APIC_BASE+0x20),reg; shr $24,reg
#define STACK_ADDRESS_PAGE_TABLE_OFFSET (((((STACK0TOP_VIRTUAL_ADDRESS-1-KERNEL_RESERVED_END))>>12)<<3))
#define STACK3_ADDRESS_PAGE_TABLE_OFFSET (((((STACK3TOP_VIRTUAL_ADDRESS-1-KERNEL_RESERVED_END))>>12)<<3))
#define META_ADDRESS_PAGE_TABLE_OFFSET (((((META_VIRTUAL_ADDRESS-KERNEL_RESERVED_END))>>12)<<3))
#define CODE_ADDRESS_PAGE_TABLE_OFFSET (((((THREAD_CODE_START-KERNEL_RESERVED_END))>>12)<<3))
#define STALL() 1337: hlt; jmp 1337b;
#define COUNT_ONES(regx,regy) push %rcx; \
    xor regy,regy; \
    1337:; \
    cmp $0,regx; \
    jz  1338f; \
    inc regy; \
    mov regx,%rcx; \
    dec %rcx; \
    and %rcx,regx; \
    jmp 1337b; \
    1338:; \
    pop  %rcx

#define PUSHAEXCEPTRAX  push    %rdi; \
    push    %rbx; \
    push    %rcx; \
    push    %rdx; \
    push    %rsi; \
    push    %rbp; \
    push    %r8; \
    push    %r9; \
    push    %r10; \
    push    %r11; \
    push    %r12; \
    push    %r13; \
    push    %r14; \
    push    %r15

#define PUSHA   push    %rax; \
    push    %rdi; \
    push    %rbx; \
    push    %rcx; \
    push    %rdx; \
    push    %rsi; \
    push    %rbp; \
    push    %r8; \
    push    %r9; \
    push    %r10; \
    push    %r11; \
    push    %r12; \
    push    %r13; \
    push    %r14; \
    push    %r15

#define POPA    pop %r15; \
    pop %r14; \
    pop %r13; \
    pop %r12; \
    pop %r11; \
    pop %r10; \
    pop %r9; \
    pop %r8; \
    pop %rbp; \
    pop %rsi; \
    pop %rdx; \
    pop %rcx; \
    pop %rbx; \
    pop %rdi; \
    pop %rax

#define PUTCHAR(x,y,char) push %rdi; mov  $(0xB8000+(x+(y*80))*2),%rdi; \
            movb    $(char),(%rdi); \
            movb    $0x04,1(%rdi); \
            pop     %rdi

#define WAIT(x) push %rcx; mov $x,%rcx; rep nop; pop %rcx;

#define DEBUG_SHOW_CPU()  push %rax; \
    GET_APIC_ID(%eax); \
    shl $1,%rax; \
    add $0xB8000, %rax; \
    incb (%rax); \
    cmpb $'Z',(%rax); \
    jb 444f; \
    movb $'A',(%rax); \
    444:pop %rax;

#define DEBUG_INCCHAR(location) \
    push %rax; \
    mov  location,%rax; \
    add $0xB8000,%rax; \
    incb (%rax); \
    cmpb $0x39,(%rax); \
    jb 444f; \
    movb $0x30,(%rax); \
    444: pop %rax

#define AP_STACK(id) \
    shl     $9,id ;\
    add     $AP_STACKS+512,id
