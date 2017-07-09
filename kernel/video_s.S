#include "../memorymap.h"
#include "macros.h"
.global video_handler
.global video_refresh_enable


video_handler:
    push    %rax

    // only do this on CPU0, we dont want to call it on all cores
    GET_APIC_ID(%rax)
    cmp     $0,%al
    jne     ignore_video_handler

    cmpq    $0,video_refresh_enable
    jz      ignore_video_handler


    push    %rcx

    mov     refresh_counter,%rcx
    cmp     $0,%rcx
    jne     1f

    push    %rsi
    push    %rdi
    movq    $SCREEN_REFRESH_RATE_COUNTER,refresh_counter

    //TODO: should lock this buffer because another CPU may have
    //      changed it and unmapped it while we are executin movsb
    mov     current_video_back_buffer,%rsi

    // TODO: should check dirty flag. But looking through the page tables
    //       then clearing the flag and invalidating the TLB (because of 
    //       dirty flag clear) is a lot of operations... is it worth it?

    mov     $0xB8000,%rdi
    mov     $(4096/8),%rcx
    rep     movsq

2:  pop     %rdi
    pop     %rsi
1:  pop     %rcx
    decb    refresh_counter
ignore_video_handler:
    pop     %rax
    ret

refresh_counter: .quad SCREEN_REFRESH_RATE_COUNTER
video_refresh_enable: .quad 1
