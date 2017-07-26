#include "macros.h"
#include "vmx.h"
#include "../memorymap.h"


#define VMWRITE(A,B)  mov B,%rbx; mov A,%rax; vmwrite %rbx,%rax
#define VMREAD(A)  mov A,%rbx; vmread %rbx,%rax

.global init_hypervisor
.global create_vm
.global new_vcpu_thread

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
// Function: add_vmcs_to_vminfo(%rdi=vminfo, %rsi=vmcs)
//
// Note: This function is multi-processor safe
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
add_vmcs_to_vminfo:
    push        %rdi
    push        %rsi
    push        %rax
    push        %rcx

    mov         $(VMINFO_VMCS_COUNT+1),%rcx
    btr         $MIRROR_BIT,%rsi                // to physical address

    add         $VMINFO_VMCS,%rdi
1:  mov         (%rdi),%rax
    add         $8,%rdi
    dec         %rcx
    jz          vmcs_list_full    
    cmp         $0,%rax
    jne         1b
    lock cmpxchg %rsi,-8(%rdi)
    jnz         1b              //cmpxchg failed if z=0

    pop         %rcx
    pop         %rax
    pop         %rsi
    pop         %rdi
    ret

vmcs_list_full:
    int         $3


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
    push        %rdi

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

    // Clear the VMCS list
    mov         $VMINFO_VMCS_COUNT,%rcx
    mov         %rax,%rdi
    add         $VMINFO_VMCS,%rdi
    push        %rax
    xor         %rax,%rax
    rep         stosq
    pop         %rax

1:  pop         %rdi
    pop         %rcx
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

    push        %rdi
    call        ept_init_static_pages
    pop         %rdx

    // Allocate 2 4k page in the guest so we can copy code in it.
    mov         $0,%rdi
    mov         $2,%rsi
    call        ept_allocate_pages

    mov         $vm_bootstrap,%rsi
    mov         %rax,%rdi
    mov         $((4096*2)/8),%rcx
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
    btrq        $MIRROR_BIT,%rdi            // to phys address
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
//  Function: create_vm(%rdi=number_of_vcpu,rsi=param_for_thread)
//  
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
create_vm:
    push        %rdi
    push        %rsi
    push        %rdx
    push        %rcx
    push        %rbx

    // Save the vCPU count in rcx so we can loop later
    mov         %rdi,%rcx
    // Create the VMINFO structure
    call        reserve_vminfo
    cmp         $0,%rax
    jnz         1f
    int         $3
1:  mov         %rsi,VMINFO_META(%rax)
    push        %rax
    
    // Create the EPT and store the pointer in the VMINFO structure
    mov         $1,%rdi
    mov         %rax,%rsi
    call        create_ept
    pop         %rdx          //rdx=param for thread in launchKernelThread
                              // in this case, we pass in the vminfo
    mov         $new_vcpu_thread,%rdi
    mov         $(new_vcpu_thread_end-new_vcpu_thread),%rsi
    push        %rcx
create_vcpus:
    call        launchKernelThread
    loop        create_vcpus
    pop         %rcx

    // Now wait until all VMCS are created and added in the list before 
    // we start the first vCPU
    // index_of_last_cpu = (cpu_count-1)*8
    dec         %rcx
    shl         $3,%rcx     
    add         $VMINFO_VMCS,%rdx
    add         %rdx,%rcx

1:  cmpq        $0,(%rcx)
    je          1b

    // At this point, we know all vCPUs are created
    mov         (%rdx),%rdx
    bts         $MIRROR_BIT,%rdx
    movl        $0,4(%rdx)

    pop         %rbx
    pop         %rcx
    pop         %rdx
    pop         %rsi
    pop         %rdi
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//  Function: new_vcpu_thread
//  This is a thread that will be launched for a VCPU
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
new_vcpu_thread:
    // test and set the bit to indicate the console was created
    lock bts    $1,VMINFO_FLAGS(%rdx)
    jc          no_need_for_new_console

    push    %rdx
    call    createTextConsole
    pop     %rdi
    push    %rdi
    call    ept_map_video_buffer
    pop     %rdx

no_need_for_new_console:

    // rdx is the metadata that is passed in the new thread. it can
    // be used for anything.

    mov     %rdx,%rdi
    call    create_vcpu
    int     $3      //create_vcpu should never return
new_vcpu_thread_end:

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: create_ept(rdi=gig_count, rsi=vminfo_structure)
// Creates an EPT structure for 'gig_count' gigabytes of RAM
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
create_ept:
    call        ept_setup_guest_memory //returns PML4 in rax
    mov         %rax,VMINFO_PML4(%rsi)

    mov         %rsi,%rdi
    call        setup_vm_bootstrap
    ret
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//  Function: create_vcpu(rdi=vminfo)
//
//  This function will never return. It will launch a VM and execute code from there.
//  Upon vmexit, a handler will be called and will execute in the same task context
//  that created the VM
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
create_vcpu:
    mov         VMINFO_META(%rdi),%rax
    push        %rax                // metadata passed to create_vm
    mov         %rdi,%r14

    //This function will never return, so no need to maintain a stack

    // we disable interrupts because we don't wanna be preempted by setting
    // up the VMCS
    cli

    // Reserve a physical page for the VMCS and clear it.
    mov         $1,%rdi
    call        kernelAllocPages
    mov         %rax,%rdi

    // clear vmcs
    push        %rdi
    mov         $0,%rax
    mov         $(4096/8),%rcx
    rep         stosq
    pop         %rdi

    // Get revision ID and set it in the VMCS
    call        get_revision_id
    movl        %eax,(%rdi)
    btrq        $MIRROR_BIT,%rdi            // to phys address
    mov         $PROCESS_VMCS,%rax
    mov         %rdi,(%rax)   // save the VMCS
    vmclear     (%rax)
    jbe         vm_create_failed
    vmptrld     (%rax)
    jbe         vm_create_failed


    mov         VMINFO_PML4(%r14),%rdi
    btrq        $MIRROR_BIT,%rdi            // to phys address
    call        init_vm_vmcs

    pop         %rdi                    //metadata to be handed to VM

    // We push the vminfo address on the stack so it is available on vmexits
    push        %r14
    VMWRITE($VMCS_HOST_RSP,%rsp)

    // We are done using the VMCS so we release it because
    // another CPU might write to it when we add it in the 
    // VM's vcpu list
    mov         $PROCESS_VMCS,%rax
    vmclear     (%rax)

    // add VMCS in vmcs list of vminfo
    push        %rdi
    mov         (%rax),%rsi         // VMCS
    bts         $MIRROR_BIT,%rsi
    movl        $0xDEADBEEF,4(%rsi) // set as pending-start
    btr         $MIRROR_BIT,%rsi
    mov         %r14,%rdi           //vminfo
    call        add_vmcs_to_vminfo
    pop         %rdi

    // Now we are ready to launch the VCPU. But we check if the 
    // state is 0 first. If not, then it is not ready to start
    // all CPU will wait before starting until all vCPUs are created
    // in the VM.
    sti
check_vm_state:
    mov         $PROCESS_VMCS,%rax
    mov         (%rax),%rbx
    bts         $MIRROR_BIT,%rbx
    cmpl        $0,4(%rbx)
    je          launchvm
    call        yield
    jmp         check_vm_state
launchvm:
    vmptrld     (%rax)
    vmlaunch
    // if we get here, it means vmlaunch failed
    mov         $0x6666666666,%r15

vm_create_failed:
    sti
    pushf
    pop         %rax
    mov         $VMCS_VM_INSTRUCTION_ERROR,%rdx
    vmread      %rdx,%rdx
    mov         $0x242242,%rcx
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
    mov         $(1<<0|1<<1|1<<5|1<<7|1<<20),%rdi
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
    VMWRITE($VMCS_GUEST_ACTIVITY_STATE,$0)  //TODO
    //VMWRITE($VMCS_GUEST_ACTIVITY_STATE,$3)  //wait-for-sipi


    // VPID will be the same for all VCPUs. They each will be using a different EPT
    VMWRITE($VMCS_VIRTUAL_PROCESSOR_ID,$242)

    call        get_apic_address
    VMWRITE($VMCS_APIC_ACCESS_ADDR,%rax)
    mov         %rsi,%rax
    btr         $MIRROR_BIT,%rax
    VMWRITE($VMCS_VIRTUAL_APIC_PAGE_ADDR,%rax)

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
    and         %rdx,%rdi  //rdx contains the required 0 (if edx[x]==0 -> reserved 0
    or          %rdi,%rax  //rax contains the required 1 (if eax[x]==1 -> reserved 1
    pop         %rdx
    push        %rax
    VMWRITE(%rdx,%rax)
    pop         %rax
    ret

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Function: adjust_guest_rip_after_fault
// 
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
adjust_guest_rip_after_fault:
    push        %r15
    push        %rax
    push        %rbx

    VMREAD($VMCS_VM_EXIT_INSTRUCTION_LEN)
    mov         %rax,%r15
    VMREAD($VMCS_GUEST_RIP)
    add         %r15,%rax
    VMWRITE($VMCS_GUEST_RIP,%rax)

    pop         %rbx
    pop         %rax
    pop         %r15
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
    cmp         $EXIT_REASON_APIC_ACCESS,%rax
    je          handle_apic_access
    cmp         $EXIT_REASON_VMCALL,%rax
    je          handle_vmcall

////// UNHANDLED VM EXIT
    mov         $0x111111112222220,%r15
    int         $3


////// VMCALL
handle_vmcall:
    // Since vmcall is a fault, we must increase guest's RIP
    call        adjust_guest_rip_after_fault

    cmp         $1,%rdx
    je          handle_vmcall_sipi

    int         $3

handle_vmcall_sipi:
    // we need interrupts cleared because we temporarily execute VMPTRLD
    // and a context switch would reset back our own VMCS
    cli
    // The VMM sets all AP's VMCS field "abort" to 0xDEADBEEF when they 
    // are pending start, so we must clear them now to indicate they are ready
    // Note that this field is allowed to be written to directly without
    // VMWRITE
    push        %rcx
    mov         $PROCESS_VMCS,%rcx
    vmclear     (%rcx)

    add         $VMINFO_VMCS,%rbp
    mov         $VMINFO_VMCS_COUNT,%rcx
1:  mov         (%rbp),%rcx   //VMCS
    cmp         $0,%rcx       // is it a null entry?
    je          2f            //  if yes then we reached end of list
    bts         $MIRROR_BIT,%rcx
    cmpl        $0xDEADBEEF,4(%rcx)    // waiting for startup?
    jne         3f          // if not then no processing is needed

    // high 32bits of rdi should be cleared 
    // but need to confirm this
    vmptrld     (%rbp)
    VMWRITE($VMCS_GUEST_RIP,%rdi)   
    vmclear     (%rbp)
    movl        $0,4(%rcx)
3:  add         $8,%rbp
    loop        1b
    
2:  mov         $PROCESS_VMCS,%rcx
    vmptrld     (%rcx)

    jbe         handle_vmcall_sipi_error
    pop         %rcx
    jmp         resume_from_vmexit
handle_vmcall_sipi_error:
    int         $3



////// APIC ACCESS
handle_apic_access:
    VMREAD($VMCS_VIRTUAL_APIC_PAGE_ADDR)
    mov         %rax,%rbp
    bts         $MIRROR_BIT,%rbp 
    VMREAD($VMCS_EXIT_QUALIFICATION)
    int         $3

////// EXTERNAL INTERRUPT
handle_external_interrupt:
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

    // We clear interrupts because we dont want
    // a context switch to occur after vmresume if it fails.
    cli

    vmresume
    jc          1f
    vmlaunch
1:  VMREAD($VMCS_VM_INSTRUCTION_ERROR)
    pushf
    pop         %rbx
    mov         $0x111111112222223,%r15
    int         $3

