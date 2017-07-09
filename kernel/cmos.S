.global getDateTime

#define READ_CMOS_BCD(reg) mov $reg,%al; out %al,$0x70; in $0x71,%al; \
    mov %al,%ah; \
    shr $4,%al; \
    and $0x0F0F,%ax; \
    add $0x3030,%ax

     


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: getDateTime(rdi=buffer_address)
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
getDateTime:
    push        %rax
    push        %rdi

    push        %rdi
    mov         $cmoslock,%rdi
    call        spinLock
    pop         %rdi

    // TODO: should adjust for time zone

    //TODO: should be carefull about CMOS updates
    READ_CMOS_BCD(7)
    mov         %ax,(%rdi)
    movb        $'/',2(%rdi)
    add         $3,%rdi

    READ_CMOS_BCD(8)
    mov         %ax,(%rdi)
    movb        $'/',2(%rdi)
    add         $3,%rdi

    //TODO: This is a Y2.1K bug!
    movw        $0x3032,(%rdi)
    add         $2,%rdi

    READ_CMOS_BCD(9)
    mov         %ax,(%rdi)
    movb        $' ',2(%rdi)
    add         $3,%rdi

    READ_CMOS_BCD(4)
    mov         %ax,(%rdi)
    movb        $':',2(%rdi)
    add         $3,%rdi

    READ_CMOS_BCD(2)
    mov         %ax,(%rdi)
    movb        $':',2(%rdi)
    add         $3,%rdi

    READ_CMOS_BCD(0)
    mov         %ax,(%rdi)
    movb        $' ',2(%rdi)
    add         $3,%rdi

    movb        $0,(%rdi)

    mov         $cmoslock,%rdi
    call        spinUnlock

    pop         %rdi
    pop         %rax
    ret

cmoslock: .quad 0
