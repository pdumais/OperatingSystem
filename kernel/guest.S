.global vm_bootstrap

.CODE16
vm_bootstrap:
    mov     $0,%ax
    mov     %ax,%ss
    mov     $0x100,%sp
    push    %di
        
    mov     %edi,%eax
    mov     $0x1000,%di
    mov     %eax,%es:(%di)
    mov     %es:(%di),%edx

    mov     $0xB800,%ax
    mov     %ax,%es
    mov     $'A',%al

    pop     %di
    1:
    mov     %al,%es:(%di)
    inc     %al
    cmp     $'Z',%al
    jna     1b
    mov     $'A',%al
    jmp     1b
