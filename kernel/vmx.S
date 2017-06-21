#include "macros.h"
#include "vmx.h"
#include "../memorymap.h"

#define VMWRITE(A,B)  mov B,%rbx; mov A,%rax; vmwrite %rbx,%rax
#define VMREAD(A)  mov A,%rbx; vmread %rbx,%rax

.global init_hypervisor
.global create_vm


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
get_revision_id:
    push    %rcx
    mov     $0x480,%rcx
    rdmsr
    pop     %rcx
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: reserve_vminfo()
// Returns rax=vminfo pointer
//
// Note: This function is multi-processor safe
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
reserve_vminfo:
    push        %rcx

    mov         $VMINFOS,%rax
    mov         $((VMINFOSEND-VMINFOS)/VMINFO_SIZE),%rcx

find_vm_info:
    lock bts    $0,(%rax)
    jnc         vminfo_found
    add         $VMINFO_SIZE,%rax
    loop        find_vm_info

vminfo_full:
    mov         $0,%rax
    jmp         1f

vminfo_found:
    xor         %rcx,%rcx
    mov         %rcx,VMINFO_MEMORY_LOCK(%rax)
1:  pop         %rcx
    ret


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: setup_vm_bootstrap(rdi=vminfo)
// This will copy the first 4096 bytes of the "guest" source file
// A future improvement would be to load a file called "vmbios.bin" from the 
// disk and copy it.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
setup_vm_bootstrap:
    push        %rcx
    push        %rsi
    push        %rdx

    mov         %rdi,%rdx
    mov         $0,%rdi
    mov         $1,%rsi
    call        ept_allocate_pages

    mov         $vm_bootstrap,%rsi
    mov         %rax,%rdi
    mov         $(4096/8),%rcx
    rep         movsq

    pop         %rdx
    pop         %rsi
    pop         %rcx
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: init_hypervisor()
// Needs to be done on all cores.
// If running this under KVM, nested virtualization must be
// enabled in the kernel
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
init_hypervisor:
    pushf
    push    %rdi
    push    %rcx

    cli

    mov     $0x3A,%rcx
    rdmsr
    bt      $3,%rax
    je      vmx_enabled_in_msr
    bt      $0,%rax
    jne     msr_unlocked
    mov     $0xE0000001,%rax
    int     $3

msr_unlocked:
    bts     $2,%rax
    bts     $0,%rax
    wrmsr

vmx_enabled_in_msr:

    // Enable VMX by setting cr4.VMXE
    mov     %cr4,%rax
    or      $0b10000000000000,%rax
    mov     %rax,%cr4

    // Reserve a physical page for the VMCS
    mov         $1,%rdi
    call        kernelAllocPages 
    mov         %rax,%rdi
    push        %rdi
    mov         $0,%rax
    mov         $(4096/8),%rcx
    rep         stosq
    pop         %rdi

    // Get revision ID and set it in the VMCS
    call        get_revision_id
    movl        %eax,(%rdi)

    // Enter VMX root-operations
    btrq        $38,%rdi            // to phys address
    push        %rdi
    vmxon       (%rsp)
    pop         %rdi

    jbe         vmxon_fail

    pop         %rcx
    pop         %rdi
    popf                            // restore interrupt flag
    ret

vmxon_fail:
    pushf
    pop     %rdx
    int $3

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//  Function: create_vm(rdi=metadata)
//  
//  This function will never return. It will launch a VM and execute code from there.
//  Upon vmexit, a handler will be called and will execute in the same task context
//  that created the VM
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
create_vm:
    push        %rdi
    //This function will never return, so no need to maintain a stack

    // we disable interrupts because we don't wanna be preempted by setting
    // up the VMCS
    cli

    call        reserve_vminfo
    cmp         $0,%rax
    jnz         1f
    int         $3
1:  mov         %rax,%r14

    // Reserve a physical page for the VMCS and clear it.
    mov         $1,%rdi
    call        kernelAllocPages
    mov         %rax,%rdi
    mov         %rax,VMINFO_VMCS(%r14)
    push        %rdi
    mov         $0,%rax
    mov         $(4096/8),%rcx
    rep         stosq
    pop         %rdi

    // Get revision ID and set it in the VMCS
    call        get_revision_id
    movl        %eax,(%rdi)
    btrq        $38,%rdi            // to phys address
    mov         $PROCESS_VMCS,%rax
    mov         %rdi,(%rax)   // save the VMCS
    vmclear     (%rax)
    jbe         vm_create_failed
    vmptrld     (%rax)
    jbe         vm_create_failed
    
    mov         $1,%rdi             // 1 gig
    call        ept_setup_guest_memory //returns PML4 in rax
    mov         %rax,%rdi
    mov         %rax,VMINFO_PML4(%r14)
    btrq        $38,%rdi            // to phys address
    call        init_vm_vmcs

    mov         %r14,%rdi
    call        setup_vm_bootstrap

    pop         %rdi                //metadata to be handed to VM

    // We push the vminfo address on the stack so it is available on vmexits
    push        %r14
    VMWRITE($VMCS_HOST_RSP,%rsp)

    // We don't need to re-enable interrupts because they will still trigger
    // a VMExit and we will re-enable them so we can process them at that time.
    vmlaunch

    // if we get here, it means vmlaunch failed
vm_create_failed:
    pushf
    pop         %rax
    mov         $VMCS_VM_INSTRUCTION_ERROR,%rdx
    vmread      %rdx,%rdx
    mov         $0x242242,%rax
    int $3
    //This function will never return. No need to clear the stack

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function init_vm_vmcs(rdi=PML4 of guest-physical memory)
// Will init the currently loaded VMCS (loaded with vmptrld) with initial data
// to be ready for a vmlaunch
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
init_vm_vmcs:
    push        %r15
    mov         %rdi,%r15
    sub         $8,%rsp
    mov         %rsp,%rbp

    mov         %cr3,%rax
    VMWRITE($VMCS_HOST_CR3,%rax)

    mov $MSR_IA32_VMX_CR0_FIXED0,%rcx
    rdmsr
    mov         %cr0,%rdx
    or          %rax,%rdx
    VMWRITE($VMCS_HOST_CR0,%rdx)

    mov $MSR_IA32_VMX_CR4_FIXED0,%rcx
    rdmsr
    mov         %cr4,%rdx
    or          %rax,%rdx
    VMWRITE($VMCS_HOST_CR4,%rdx)

    VMWRITE($VMCS_HOST_RIP,$vm_exit_handler)

    VMWRITE($VMCS_HOST_GDTR_BASE,$GDT)
    VMWRITE($VMCS_HOST_IDTR_BASE,$IDTSPACE)
    VMWRITE($VMCS_HOST_TR_BASE,$TSS)

    str         %rax
    and         $0xF8,%al
    VMWRITE($VMCS_HOST_TR_SELECTOR,%rax)
    VMWRITE($VMCS_HOST_CS_SELECTOR,%cs)
    VMWRITE($VMCS_HOST_DS_SELECTOR,$0)
    VMWRITE($VMCS_HOST_ES_SELECTOR,$0)
    VMWRITE($VMCS_HOST_FS_SELECTOR,%fs)
    VMWRITE($VMCS_HOST_GS_SELECTOR,%gs)
    VMWRITE($VMCS_HOST_SS_SELECTOR,$0)


    VMWRITE($VMCS_HOST_IA32_SYSENTER_CS,$0)
    VMWRITE($VMCS_HOST_FS_BASE,$0)
    VMWRITE($VMCS_HOST_GS_BASE,$0)
    VMWRITE($VMCS_HOST_IA32_SYSENTER_ESP,$0)
    VMWRITE($VMCS_HOST_IA32_SYSENTER_EIP,$0)

    mov         $IA32_VMX_ENTRY_CTLS,%rcx
    mov         $0,%rdi
    mov         $VMCS_VM_ENTRY_CONTROLS,%rdx
    call        vmx_set_control

    mov         $IA32_VMX_PINBASED_CTLS,%rcx
    mov         $0b00101001,%rdi
    mov         $VMCS_PIN_BASED_VM_EXEC_CONTROL,%rdx
    call        vmx_set_control

    mov         $IA32_VMX_PROCBASED_CTLS,%rcx
    mov         $(1<<31 | 1<<7),%rdi
    mov         $VMCS_CPU_BASED_VM_EXEC_CONTROL,%rdx
    call        vmx_set_control

    mov         $IA32_VMX_PROCBASED_CTLS2,%rcx
    mov         $(1<<1|1<<5|1<<7),%rdi
    mov         $VMCS_SECONDARY_VM_EXEC_CONTROL,%rdx
    call        vmx_set_control

    mov         $IA32_VMX_EXIT_CTLS,%rcx
    mov         $(1<<9),%rdi
    mov         $VMCS_VM_EXIT_CONTROLS,%rdx
    call        vmx_set_control
    VMWRITE($VMCS_VMCS_LINK_POINTER,$0xffffffffffffffff)
    VMWRITE($VMCS_EXCEPTION_BITMAP,$0xFFFFFFFF)

    mov $MSR_IA32_VMX_CR0_FIXED0,%rcx
    rdmsr
    shl         $32,%rdx
    or          %rdx,%rax
    btr         $31,%rax    // clear paging if if fixed because of unrestricted mode
    btr         $0,%rax     // clear PE even if paging because of unrestricted mode
    VMWRITE($VMCS_GUEST_CR0,%rax)
    mov $MSR_IA32_VMX_CR4_FIXED0,%rcx
    rdmsr
    shl         $32,%rdx
    or          %rdx,%rax
    VMWRITE($VMCS_GUEST_CR4,%rax)
    VMWRITE($VMCS_GUEST_CR3,$0)
    VMWRITE($VMCS_GUEST_GDTR_BASE,$0)
    VMWRITE($VMCS_GUEST_GDTR_LIMIT,$0)
    VMWRITE($VMCS_GUEST_IDTR_BASE,$0)
    VMWRITE($VMCS_GUEST_IDTR_LIMIT,$0)
    VMWRITE($VMCS_GUEST_CS_AR_BYTES,$(3 | (1<<4) | (1<<7)))
    VMWRITE($VMCS_GUEST_CS_BASE,$0)
    VMWRITE($VMCS_GUEST_CS_LIMIT,$0xFFFF)
    VMWRITE($VMCS_GUEST_CS_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_DS_AR_BYTES,$(3 | (1<<4) | (1<<7)))
    VMWRITE($VMCS_GUEST_DS_BASE,$0)
    VMWRITE($VMCS_GUEST_DS_LIMIT,$0xFFFF)
    VMWRITE($VMCS_GUEST_DS_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_ES_AR_BYTES,$(3 | (1<<4) | (1<<7))) //3=RW/Accessed
    VMWRITE($VMCS_GUEST_ES_BASE,$0)
    VMWRITE($VMCS_GUEST_ES_LIMIT,$0xFFFF)
    VMWRITE($VMCS_GUEST_ES_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_FS_AR_BYTES,$(3 | (1<<4) | (1<<7)))
    VMWRITE($VMCS_GUEST_FS_BASE,$0)
    VMWRITE($VMCS_GUEST_FS_LIMIT,$0xFFFF)
    VMWRITE($VMCS_GUEST_FS_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_GS_AR_BYTES,$(3 | (1<<4) | (1<<7)))
    VMWRITE($VMCS_GUEST_GS_BASE,$0)
    VMWRITE($VMCS_GUEST_GS_LIMIT,$0xFFFF)
    VMWRITE($VMCS_GUEST_GS_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_SS_AR_BYTES,$(3 | (1<<4) | (1<<7)))
    VMWRITE($VMCS_GUEST_SS_BASE,$0)
    VMWRITE($VMCS_GUEST_SS_LIMIT,$0xFFFF)
    VMWRITE($VMCS_GUEST_SS_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_LDTR_AR_BYTES,$(2 | (1<<7)))
    VMWRITE($VMCS_GUEST_LDTR_BASE,$0)
    VMWRITE($VMCS_GUEST_LDTR_LIMIT,$0)
    VMWRITE($VMCS_GUEST_LDTR_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_TR_AR_BYTES,$(3 | (1<<7)))
    VMWRITE($VMCS_GUEST_TR_LIMIT,$0)
    VMWRITE($VMCS_GUEST_TR_BASE,$0)
    VMWRITE($VMCS_GUEST_TR_SELECTOR,$0)
    VMWRITE($VMCS_GUEST_DR7,$0)
    VMWRITE($VMCS_GUEST_RSP,$0)
    VMWRITE($VMCS_GUEST_RIP,$0)
    VMWRITE($VMCS_GUEST_RFLAGS,$(2))
    VMWRITE($VMCS_GUEST_SYSENTER_ESP,$0)
    VMWRITE($VMCS_GUEST_SYSENTER_EIP,$0)
    VMWRITE($VMCS_GUEST_SYSENTER_CS,$0)

    // r15 contains the 4k-aligned base address of the guest'PML4
    or          $(0 | 3 << 3 | 1<< 6),%r15      // uncacheable, page-walk=3, dFlag.
    VMWRITE($VMCS_EPT_POINTER,%r15)

    add         $8,%rsp
    pop         %r15
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: vmx_set_control(rcx=MSR, rdi=wanted_value, rdx=vmcs field)
// Resturns rax: the value written
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
vmx_set_control:
    push        %rdx
    push        %rcx
    mov         $IA32_VMX_BASIC,%rcx
    rdmsr
    bt          $55,%rax
    jnc         vmx_basic_supported
    // We do not support bit 55 being set
    mov         $0xDEADBEEF,%r8
    int         $3
vmx_basic_supported:
    // Get allowed 0-settings (a 0 means we are allowed to set to 0)
    // bit 63:32 -> allowed 1-settings (if 0, then not allowed to set to 1
    pop         %rcx
    rdmsr
    shr         $32,%rcx
    and         %rdx,%rdi  //rdx contains the required 0 (if edx[x]==0 -> reserved 0
    or          %rdi,%rax  //rax contains the required 1 (if eax[x]==1 -> reserved 1
    pop         %rdx
    push        %rax
    VMWRITE(%rdx,%rax)
    pop         %rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: vm_exit_handler()
// 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
vm_exit_handler:
    push        %rbp

    // Before doing the vmlaunch, we pushed the vminfo pointer on the stack,
    // on vmexit, our stack pointer is restored so we can retrieve the vminfo 
    // address. We'll store it in rbp for later use.
    mov         8(%rsp),%rbp
    push        %rax
    push        %rbx

    VMREAD($VMCS_VM_EXIT_REASON)
    cmp         $EXIT_REASON_EXTERNAL_INTERRUPT,%rax
    je          handle_external_interrupt
    cmp         $0,%rax
    je          handle_vm_exception
    cmp         $0x0C,%rax
    je          handle_vm_halt
    cmp         $EXIT_REASON_EPT_VIOLATION,%rax
    je          handle_ept_violation

////// UNHANDLED VM EXIT
    mov         $0x111111112222220,%r15
    int         $3

////// EXTERNAL INTERRUPT
handle_external_interrupt:
    //VMREAD($VMCS_IDT_VECTORING_INFO_FIELD)
    sti
    jmp         resume_from_vmexit

////// EPT VIOLATION
handle_ept_violation:
    VMREAD($VMCS_EXIT_QUALIFICATION)
    bt          $1,%rax
    jc          handle_ept_violation_write
    mov         $0x911111112222222,%r15
    int         $3

// If we get here, it is because the VM tried to write in a RO page mapped in EPT.
// This is because we need to lazily assign memory to the VM so we will create
// a new page.
handle_ept_violation_write:
    VMREAD($VMCS_GUEST_PHYSICAL_ADDRESS)

    push        %rdi
    push        %rsi
    push        %rdx
    mov         %rax,%rdi
    mov         $1,%rsi
    mov         %rbp,%rdx 
    call        ept_allocate_pages
    pop         %rdx
    pop         %rsi
    pop         %rdi
    jmp         resume_from_vmexit

////// VM EXCEPTION
handle_vm_exception:
    mov         $0x111111112222221,%r15
    VMREAD($VMCS_VM_EXIT_INTR_INFO)     // Chapter 24.9.2
    mov         %rax,%r8
    VMREAD($VMCS_VM_EXIT_INTR_ERROR_CODE)
    int $3

////// VM HALT
handle_vm_halt:
    //TODO: we should yield this thread and wake up only when 
    //      an event is available, then we should inject it.
    int $3

resume_from_vmexit:
    pop         %rbx
    pop         %rax
    pop         %rbp
    cli
    vmresume
    mov         $0xE000001,%r15
    jc          1f
    vmlaunch
    mov         $0xE000002,%r15
1:  VMREAD($VMCS_VM_INSTRUCTION_ERROR)
    pushf
    pop         %rbx
    int         $3

