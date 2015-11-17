#include "../memorymap.h"
#include "macros.h"
#include "config.h"
#include "includes/kernel/intA0.h"

.global initIntA0

initIntA0:
    push        %rax    
    push        %rbx
    
    mov         $intA0,%rbx
    mov         $0xA0,%rax
    call        registerTrapGate
    
    pop         %rbx
    pop         %rax
    ret


intA0:
    push        %r15
    push        %r14
    push        %r13
    push        %r12
    push        %r11
    push        %r10
    push        %r9
    push        %r8
    push        %rbp
    push        %rdi
    push        %rsi
    push        %rdx
    push        %rcx
    push        %rbx

    cmp         $INTA0_GET_APIC_ID,%rax
    je          intA0_get_apic_id
    cmp         $INTA0_KILL_CURRENT_TASK,%rax
    je          intA0_kill_current_task
    cmp         $INTA0_WAITPROCESS_DEATH,%rax
    je          intA0_wait_process_death
    cmp         $INTA0_PRINTF,%rax
    je          intA0_printf
    cmp         $INTA0_CREATETEXTCONSOLE,%rax
    je          intA0_createTextConsole
    cmp         $INTA0_STEALTEXTCONSOLE,%rax
    je          intA0_stealTextConsole
    cmp         $INTA0_RESTORETEXTCONSOLE,%rax
    je          intA0_restoreTextConsole
    cmp         $INTA0_POLL_IN,%rax
    je          intA0_poll_in
    cmp         $INTA0_MALLOC,%rax
    je          intA0_malloc
    cmp         $INTA0_FREE,%rax
    je          intA0_free
    cmp         $INTA0_VIRT2PHYS,%rax
    je          intA0_virt2phys
    cmp         $INTA0_LOADPROCESS,%rax
    je          intA0_load_process
    cmp         $INTA0_GET_DATETIME,%rax
    je          intA0_get_datetime
    cmp         $INTA0_FOPEN,%rax
    je          intA0_fopen
    cmp         $INTA0_FREAD,%rax
    je          intA0_fread
    cmp         $INTA0_FWRITE,%rax
    je          intA0_fwrite
    cmp         $INTA0_FCLOSE,%rax
    je          intA0_fclose
    cmp         $INTA0_FSEEK,%rax
    je          intA0_fseek
    cmp         $INTA0_FGETSIZE,%rax
    je          intA0_fgetsize
    cmp         $INTA0_CREATE_SOCKET,%rax
    je          intA0_create_socket
    cmp         $INTA0_CLOSE_SOCKET,%rax
    je          intA0_close_socket
    cmp         $INTA0_CONNECT,%rax
    je          intA0_connect
    jmp         intA0_exit

intA0_get_apic_id:
    GET_APIC_ID(%eax)
    jmp         intA0_exit

intA0_kill_current_task:
    mov         %cr3,%rax
    mov         %rax,%rdi
    call        destroy_sockets
    call        destroyFileHandles
    call        removeConsole
    call        killCurrentTask
    jmp         intA0_exit

intA0_printf:
    call        safeWriteString
    jmp         intA0_exit

intA0_createTextConsole:
    call        createTextConsole
    jmp         intA0_exit

intA0_stealTextConsole:
    call        stealTextConsole
    jmp         intA0_exit

intA0_restoreTextConsole:
    call        restoreTextConsole
    jmp         intA0_exit

intA0_poll_in:
    call        pollChar
    jmp         intA0_exit

intA0_malloc:
    call        malloc
    jmp         intA0_exit

intA0_free:
    call        free
    jmp         intA0_exit

intA0_virt2phys:
    call        currentProcessVirt2phys
    jmp         intA0_exit

intA0_load_process:
    call        loadProcess
    jmp         intA0_exit

intA0_wait_process_death:
    call        waitProcessDeath
    jmp         intA0_exit

intA0_get_datetime:
    call        getDateTime
    jmp         intA0_exit

intA0_fopen:
    call        fopen
    jmp         intA0_exit
intA0_fread:
    call        fread
    jmp         intA0_exit
intA0_fwrite:
    call        fwrite
    jmp         intA0_exit
intA0_fclose:
    call        fclose
    jmp         intA0_exit
intA0_fseek:
    call        fseek
    jmp         intA0_exit
intA0_fgetsize:
    call        fgetsize
    jmp         intA0_exit

intA0_create_socket:
    call        create_socket
    jmp         intA0_exit
intA0_close_socket:
    call        close_socket
    jmp         intA0_exit
intA0_connect:
    call        connect
    jmp         intA0_exit


intA0_exit:
    pop         %rbx
    pop         %rcx
    pop         %rdx
    pop         %rsi
    pop         %rdi
    pop         %rbp
    pop         %r8
    pop         %r9
    pop         %r10
    pop         %r11
    pop         %r12
    pop         %r13
    pop         %r14
    pop         %r15
    iretq
