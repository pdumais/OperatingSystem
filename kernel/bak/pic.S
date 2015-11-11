.global enableIRQ
.global initPIC
.global initPIT

.extern writeNumber
#include "pit.h"

.equ MasterPICCMD,0x20
.equ MasterPICData,0x21
.equ SlavePICCMD,0xA0
.equ SlavePICData,0xA1
    
initPIC:
    //Init
    mov     $0x11,%al
    out     %al,$MasterPICCMD
    call    waitDevice
    mov     $0x11,%al
    out     %al,$SlavePICCMD
    call    waitDevice

    // define vectors
    mov     $0x20,%al
    outb    %al,$MasterPICData
    call    waitDevice
    mov     $0x28,%al
    outb    %al,$SlavePICData
    call    waitDevice

    //Init
    mov     $0x04,%al
    outb    %al,$MasterPICData
    call    waitDevice
    mov     $0x02,%al
    outb    %al,$SlavePICData
    call    waitDevice

    mov     $0x01,%al
    outb    %al,$MasterPICData
    call    waitDevice
    mov     $0x01,%al
    outb    %al,$SlavePICData
    call    waitDevice



    // Mask all interrupts
    mov     $0xFB,%al               // leave IRQ enabled
    out     %al,$MasterPICData
    call    waitDevice
    mov     $0xFF,%al
    out     %al,$SlavePICData
    call    waitDevice
    ret

// enableIRQ: enables or disables an IRQ
// %rdi[0..7] = irq number
// %rdi[8] = enabled (1), disabled (0)
enableIRQ:
    push    %rdx
    push    %rcx

    mov     %rdi,%rax
    and     $0x1FF,%rax
    mov     $MasterPICData,%dx
    cmp     $8,%al
    jb      masterPic
    mov     $SlavePICData,%dx
    sub     $8,%al
masterPic:
    mov     %al,%cl
    mov     $1,%ch
    shl     %cl,%ch
    in      %dx,%al
    cmp     $1,%ah
    jne     setDisabled
    not     %ch
    and     %ch,%al
    jmp     maskpic
setDisabled:
    or      %ch,%al
maskpic:
    out     %al,%dx
    call    waitDevice

    pop     %rcx
    pop     %rdx
    ret

initPIT:
    mov 0b00110100,%al
    out %al,$0x43
    mov $TIMER_RELOAD_VALUE,%eax
    out %al,$0x40
    mov %ah,%al
    out %al,$0x40
    ret
    
waitDevice:
    outb    %al,$0x80   /*useless port*/
    ret

