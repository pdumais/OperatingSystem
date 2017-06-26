.global vm_bootstrap

#define AP_START 0x1000
#define APIC_BASE_MSR                       0x1B
#define TEST_INDEX  0x2000

.CODE16
vm_bootstrap:
	.BYTE 0xEA  
	.WORD 10
	.WORD 0x0   
	.BYTE 0xEA  
	.WORD ap_entry_point-vm_bootstrap
	.WORD 0x0   
1:
    mov     $0,%ax
    mov     %ax,%ss
    mov     %ax,%ds
    mov     $0xF10,%sp

    mov     $0x2000,%si
    mov     %di,%ds:(%si)
    push    %di 
      
    // copy jump instruction to 0x1000
    mov     $AP_START,%di
    movb    $0xEA,(%di)
    movw    $(ap_entry_point-vm_bootstrap),1(%di)
    movw    $0x0,3(%di)

    // now send IPI with rip=0x1000 
    mov     $1,%edx
    mov     $AP_START,%edi
    vmcall

    pop     %di
    mov     $4,%ah

spin_test:
    mov     $0xB800,%bx
    mov     %bx,%es
    mov     $'A',%al

    mov     %di,%dx

    1:
    cmp     %di,%dx
    jne     2f
    mov     %ax,%es:(%di)
    inc     %al
    cmp     $'Z',%al
    jna     1b
    mov     $'A',%al
    jmp     1b

2:  hlt

ap_entry_point:
    mov     $0,%ax
    mov     %ax,%ss
    mov     %ax,%ds
    mov     $0xF10,%sp

    mov     $0x2000,%si
1:  mov     %ds:(%si),%ax
    mov     %ax,%di
    add     $2,%di
    lock cmpxchg %di,%ds:(%si)
    jnz     1b

    mov     $2,%ah
    jmp     spin_test
