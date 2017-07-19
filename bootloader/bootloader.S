// the drive is raw, no MBR, no partitions, no filesystems.
// The kernel resides on sector 3

#include "../memorymap.h"

.include "../sizekernel.inc"

.CODE16
.ORG 0

.EQU HEAP,  0x0500    /* this is relative to 0x00000000*/
.EQU STACK_END,0x7C00
.EQU STACKSEG, 0
.EQU KERNEL_BASE_ADDR, KERNEL_BASE

start:
	.BYTE 0xEA  
	.WORD 5
	.WORD 0x07C0   /* this will jump and will set CS to 0x07C0*/

.org 5
main:	
	cli
	//Setup Stack
	pushw	$STACKSEG
	popw	%ss
	mov	    $STACK_END,%sp
    mov     %sp,%bp

    // create Application Processors boot address
    // This is a jump to the "apmain" label.
    push    %cs
    pop     %ds
    pushw   $0
    pop     %es
    mov     $SMP_TRAMPOLINE,%di
    mov     $JUMPCODETOAP,%si
    mov     $8,%cx
    rep     movsb

    mov     $0,%ax
    mov     %ax,%gs

    call    detect_memory

	// enable A20 line adressing
	call    a20wait
	mov     $0xAD,%al
	out     %al,$0x64
	call    a20wait
	mov     $0xD0,%al
	out     %al,$0x64
	call    a20wait2
	in      $0x60,%al
	push    %eax
	call    a20wait
	mov     $0xD1,%al
	out     %al,$0x64
	call    a20wait
	pop     %eax
	or      $2,%al
	out     %al,$0x60
	call    a20wait
	mov     $0xAE,%al
	out     %al,$0x64
	call    a20wait

	// enable unreal mode to have access to full 32bit addressing
	push	%cs     /* remember, cs is 07C0*/
	pop	    %ds
    mov     $GDTINFO,%eax
    lgdtl   (%eax)
    mov     %cr0,%eax
    or      $1,%al
    mov     %eax,%cr0   /* protected mode */
    mov     $0x08,%bx
    mov     %bx,%fs
    mov     %bx,%es
    and     $0xFE,%al  /* Back to real mode. es will still be a valid 32bit segment because cached value not cleared */
    mov     %eax,%cr0

    // load sectors 1 by one for kernel in temporary buffer and copy to real location
    movl    $0x1FA,%eax     /* get kernel size */
    movl    (%eax),%ecx
    shrl    $9,%ecx         /* divide by 512 and add 1 to get number of sectors we should read */
    incl    %ecx             /* dx now contains number of sectors for kernel */
    xorl    %ebx,%ebx

    mov     $KERNEL_BASE_ADDR,%edi
readNextSector:
    push    %edi
    movl    $DAP,%esi
    movl    %ebx,%eax
    addl    $3,%eax         /* offset sector by 3, to skip first 3 sectors */
    movl    %eax,8(%esi)    /* eax is the sector number*/
    mov     $0x42,%ah
    mov     $0x80,%dl
    int     $0x13
    
    // copy that sector to kernel memory
    pop     %edi
    mov     $HEAP,%esi
    push    %ecx
    mov     $512,%ecx
copykernel:
    mov     %gs:(%esi),%al
    mov     %al,%fs:(%edi)
    inc     %edi
    inc     %esi
    loop    copykernel
    pop     %ecx

    incl     %ebx
    cmpl     %ebx,%ecx       /* current sector is equal to number of sectors to read? */
    jne     readNextSector

	//swtich back to protected mode so we can jump to meg 1
	mov     %cr0,%eax
	or      $1,%al
    mov     %eax,%cr0

    // WARNING: WE ARE BACK TO PROTECTED MODE NOW. NO BIOS CALLS ARE ALLOWED ANYMORE
    // Jump far to update 'cs' and to reach new code: the kernel
    ljmpl   $0x10,$KERNEL_BASE_ADDR

a20wait:
    in      $0x64,%al
    test    $2,%al
    jnz     a20wait
    ret
a20wait2:
    in      $0x64,%al
    test    $1,%al
    jz      a20wait2
    ret

printchar:  /* param: al=char */
    pushl   %ebx
    pushl   %ecx
    mov     $0x09,%ah
    mov     $0x0004,%bx
    mov     $10,%cx
    int     $0x10
    popl    %ecx
    popl    %ebx
    ret

detect_memory:
    push    %es    
    mov     $(MEMMAP >> 4),%ax
    mov     %ax,%es
    mov     $0,%di
    mov     $0,%bx
1:  mov     $24,%cx
    mov     $0x534d4150,%edx
    mov     $0xe820,%ax
    int     $0x15
    add     $24,%di
    cmp     $0,%bx
    jne     1b
    pop     %es
    ret


// WARNING: do not use the stack in here
apmain:
    cli
    // Go in protected mode
    mov     %cs,%ax     /* remember, cs is 07C0*/
    mov     %ax,%ds
    mov     $GDTINFO,%eax
    lgdtl   (%eax)
    mov     %cr0,%eax
    or      $1,%al
    mov     %eax,%cr0   /* protected mode */
    ljmpl   $0x10,$KERNEL_BASE_ADDR


// =============================== END CODE ======================================================


.align 4
JUMPCODETOAP: 
    .BYTE 0x90
    .BYTE 0x90
    .BYTE 0x90
	.BYTE 0xEA  
	.WORD apmain
	.WORD 0x07C0
.align 4
DAP:
    .BYTE 0x10
    .BYTE 0x00
    .WORD 0x01
    .WORD HEAP  /*heap base location offset*/
    .WORD 0     /*segment*/
    sector: .LONG 0x02  /* sector 3 */
    .LONG 0x00   

.align 4
GDTINFO:
    // GDT INFO
    .WORD 0x20
    .LONG . + 0x7C04    /*that will be the address of the begining of GDT table*/

    // GDT
    .LONG 00
    .LONG 00

    // GDT entry 1. Data segment descriptor used during unreal mode
    .BYTE 0xFF  
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10010010
    .BYTE 0b11001111
    .BYTE 0x00

    // GDT entry 2. Code segment used during protected mode code execution
    .BYTE 0xFF
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10011010
    .BYTE 0b11001111
    .BYTE 0x00

    // GDT entry 3. 64bit Code segment used for jumping to 64bit mode.
    // This is just used to turn on 64bit mode. Segmentation will not be used anymore after 64bit code runs.
    // We will jump into that segment and it will enable 64bit. But limit and permissions are ignored,
    // the CPU will only check for bit D and L in this case because when we will jump in this, we will
    // already be in long mode, but in compatibility sub-mode. This means that while in long mode, segments are ignored.
    // but not entiorely. Long mode will check for D and L bits when jumping in another segment and will change
    // submodes accordingly. So in long mode, segments have a different purpose: to change sub-modes
    .BYTE 0xFF  
    .BYTE 0xFF
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0x00
    .BYTE 0b10011010
    .BYTE 0b10101111  // bit 6 (D) must be 0, and bit 5 (L, was reserved before) must be 1
    .BYTE 0x00


.ORG 0x01FA  /* Kernel size */
    .LONG KERNEL_SIZE
.ORG 0x01FE
    .BYTE 0x55
    .BYTE 0xAA

.org 0x200

