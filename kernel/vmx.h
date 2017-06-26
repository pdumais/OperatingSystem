#pragma once
#ifndef __ASSEMBLER__
#include "includes/kernel/types.h"
#endif

#define IA32_VMX_BASIC 0x480
#define IA32_VMX_PINBASED_CTLS 0x481
#define IA32_VMX_PROCBASED_CTLS 0x482
#define IA32_VMX_PROCBASED_CTLS2    0x48b
#define IA32_VMX_EXIT_CTLS 0x483
#define IA32_VMX_ENTRY_CTLS 0x484
#define MSR_FS_BASE 0xc0000100
#define MSR_GS_BASE 0xc0000101
#define MSR_IA32_SYSENTER_CS             0x00000174
#define MSR_IA32_SYSENTER_ESP            0x00000176
#define MSR_IA32_SYSENTER_EIP            0x00000175
#define MSR_IA32_VMX_CR0_FIXED0         0x00000486
#define MSR_IA32_VMX_CR0_FIXED1         0x00000487
#define MSR_IA32_VMX_CR4_FIXED0         0x00000488
#define MSR_IA32_VMX_CR4_FIXED1         0x00000489

#define VMCS_VIRTUAL_PROCESSOR_ID             0x00000000
#define VMCS_GUEST_ES_SELECTOR                0x00000800
#define VMCS_GUEST_CS_SELECTOR                0x00000802
#define VMCS_GUEST_SS_SELECTOR                0x00000804
#define VMCS_GUEST_DS_SELECTOR                0x00000806
#define VMCS_GUEST_FS_SELECTOR                0x00000808
#define VMCS_GUEST_GS_SELECTOR                0x0000080a
#define VMCS_GUEST_LDTR_SELECTOR              0x0000080c
#define VMCS_GUEST_TR_SELECTOR                0x0000080e
#define VMCS_HOST_ES_SELECTOR                 0x00000c00
#define VMCS_HOST_CS_SELECTOR                 0x00000c02
#define VMCS_HOST_SS_SELECTOR                 0x00000c04
#define VMCS_HOST_DS_SELECTOR                 0x00000c06
#define VMCS_HOST_FS_SELECTOR                 0x00000c08
#define VMCS_HOST_GS_SELECTOR                 0x00000c0a
#define VMCS_HOST_TR_SELECTOR                 0x00000c0c
#define VMCS_IO_BITMAP_A                      0x00002000
#define VMCS_IO_BITMAP_A_HIGH                 0x00002001
#define VMCS_IO_BITMAP_B                      0x00002002
#define VMCS_IO_BITMAP_B_HIGH                 0x00002003
#define VMCS_MSR_BITMAP                       0x00002004
#define VMCS_MSR_BITMAP_HIGH                  0x00002005
#define VMCS_VM_EXIT_MSR_STORE_ADDR           0x00002006
#define VMCS_VM_EXIT_MSR_STORE_ADDR_HIGH      0x00002007
#define VMCS_VM_EXIT_MSR_LOAD_ADDR            0x00002008
#define VMCS_VM_EXIT_MSR_LOAD_ADDR_HIGH       0x00002009
#define VMCS_VM_ENTRY_MSR_LOAD_ADDR           0x0000200a
#define VMCS_VM_ENTRY_MSR_LOAD_ADDR_HIGH      0x0000200b
#define VMCS_TSC_OFFSET                       0x00002010
#define VMCS_TSC_OFFSET_HIGH                  0x00002011
#define VMCS_VIRTUAL_APIC_PAGE_ADDR           0x00002012
#define VMCS_VIRTUAL_APIC_PAGE_ADDR_HIGH      0x00002013
#define VMCS_APIC_ACCESS_ADDR                 0x00002014
#define VMCS_APIC_ACCESS_ADDR_HIGH            0x00002015
#define VMCS_EPT_POINTER                      0x0000201a
#define VMCS_EPT_POINTER_HIGH                 0x0000201b
#define VMCS_GUEST_PHYSICAL_ADDRESS           0x00002400
#define VMCS_GUEST_PHYSICAL_ADDRESS_HIGH      0x00002401
#define VMCS_VMCS_LINK_POINTER                0x00002800
#define VMCS_VMCS_LINK_POINTER_HIGH           0x00002801
#define VMCS_GUEST_IA32_DEBUGCTL              0x00002802
#define VMCS_GUEST_IA32_DEBUGCTL_HIGH         0x00002803
#define VMCS_GUEST_IA32_PAT                   0x00002804
#define VMCS_GUEST_IA32_PAT_HIGH              0x00002805
#define VMCS_GUEST_IA32_EFER                  0x00002806
#define VMCS_GUEST_IA32_EFER_HIGH             0x00002807
#define VMCS_GUEST_IA32_PERF_GLOBAL_CTRL      0x00002808
#define VMCS_GUEST_IA32_PERF_GLOBAL_CTRL_HIGH 0x00002809
#define VMCS_GUEST_PDPTR0                     0x0000280a
#define VMCS_GUEST_PDPTR0_HIGH                0x0000280b
#define VMCS_GUEST_PDPTR1                     0x0000280c
#define VMCS_GUEST_PDPTR1_HIGH                0x0000280d
#define VMCS_GUEST_PDPTR2                     0x0000280e
#define VMCS_GUEST_PDPTR2_HIGH                0x0000280f
#define VMCS_GUEST_PDPTR3                     0x00002810
#define VMCS_GUEST_PDPTR3_HIGH                0x00002811
#define VMCS_HOST_IA32_PAT                    0x00002c00
#define VMCS_HOST_IA32_PAT_HIGH               0x00002c01
#define VMCS_HOST_IA32_EFER                   0x00002c02
#define VMCS_HOST_IA32_EFER_HIGH              0x00002c03
#define VMCS_HOST_IA32_PERF_GLOBAL_CTRL       0x00002c04
#define VMCS_HOST_IA32_PERF_GLOBAL_CTRL_HIGH  0x00002c05
#define VMCS_PIN_BASED_VM_EXEC_CONTROL        0x00004000
#define VMCS_CPU_BASED_VM_EXEC_CONTROL        0x00004002
#define VMCS_EXCEPTION_BITMAP                 0x00004004
#define VMCS_PAGE_FAULT_ERROR_CODE_MASK       0x00004006
#define VMCS_PAGE_FAULT_ERROR_CODE_MATCH      0x00004008
#define VMCS_CR3_TARGET_COUNT                 0x0000400a
#define VMCS_VM_EXIT_CONTROLS                 0x0000400c
#define VMCS_VM_EXIT_MSR_STORE_COUNT          0x0000400e
#define VMCS_VM_EXIT_MSR_LOAD_COUNT           0x00004010
#define VMCS_VM_ENTRY_CONTROLS                0x00004012
#define VMCS_VM_ENTRY_MSR_LOAD_COUNT          0x00004014
#define VMCS_VM_ENTRY_INTR_INFO_FIELD         0x00004016
#define VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE    0x00004018
#define VMCS_VM_ENTRY_INSTRUCTION_LEN         0x0000401a
#define VMCS_TPR_THRESHOLD                    0x0000401c
#define VMCS_SECONDARY_VM_EXEC_CONTROL        0x0000401e
#define VMCS_PLE_GAP                          0x00004020
#define VMCS_PLE_WINDOW                       0x00004022
#define VMCS_VM_INSTRUCTION_ERROR             0x00004400
#define VMCS_VM_EXIT_REASON                   0x00004402
#define VMCS_VM_EXIT_INTR_INFO                0x00004404
#define VMCS_VM_EXIT_INTR_ERROR_CODE          0x00004406
#define VMCS_IDT_VECTORING_INFO_FIELD         0x00004408
#define VMCS_IDT_VECTORING_ERROR_CODE         0x0000440a
#define VMCS_VM_EXIT_INSTRUCTION_LEN          0x0000440c
#define VMCS_VMX_INSTRUCTION_INFO             0x0000440e
#define VMCS_GUEST_ES_LIMIT                   0x00004800
#define VMCS_GUEST_CS_LIMIT                   0x00004802
#define VMCS_GUEST_SS_LIMIT                   0x00004804
#define VMCS_GUEST_DS_LIMIT                   0x00004806
#define VMCS_GUEST_FS_LIMIT                   0x00004808
#define VMCS_GUEST_GS_LIMIT                   0x0000480a
#define VMCS_GUEST_LDTR_LIMIT                 0x0000480c
#define VMCS_GUEST_TR_LIMIT                   0x0000480e
#define VMCS_GUEST_GDTR_LIMIT                 0x00004810
#define VMCS_GUEST_IDTR_LIMIT                 0x00004812
#define VMCS_GUEST_ES_AR_BYTES                0x00004814
#define VMCS_GUEST_CS_AR_BYTES                0x00004816
#define VMCS_GUEST_SS_AR_BYTES                0x00004818
#define VMCS_GUEST_DS_AR_BYTES                0x0000481a
#define VMCS_GUEST_FS_AR_BYTES                0x0000481c
#define VMCS_GUEST_GS_AR_BYTES                0x0000481e
#define VMCS_GUEST_LDTR_AR_BYTES              0x00004820
#define VMCS_GUEST_TR_AR_BYTES                0x00004822
#define VMCS_GUEST_INTERRUPTIBILITY_INFO      0x00004824
#define VMCS_GUEST_ACTIVITY_STATE             0X00004826
#define VMCS_GUEST_SYSENTER_CS                0x0000482A
#define VMCS_HOST_IA32_SYSENTER_CS            0x00004c00
#define VMCS_CR0_GUEST_HOST_MASK              0x00006000
#define VMCS_CR4_GUEST_HOST_MASK              0x00006002
#define VMCS_CR0_READ_SHADOW                  0x00006004
#define VMCS_CR4_READ_SHADOW                  0x00006006
#define VMCS_CR3_TARGET_VALUE0                0x00006008
#define VMCS_CR3_TARGET_VALUE1                0x0000600a
#define VMCS_CR3_TARGET_VALUE2                0x0000600c
#define VMCS_CR3_TARGET_VALUE3                0x0000600e
#define VMCS_EXIT_QUALIFICATION               0x00006400
#define VMCS_GUEST_LINEAR_ADDRESS             0x0000640a
#define VMCS_GUEST_CR0                        0x00006800
#define VMCS_GUEST_CR3                        0x00006802
#define VMCS_GUEST_CR4                        0x00006804
#define VMCS_GUEST_ES_BASE                    0x00006806
#define VMCS_GUEST_CS_BASE                    0x00006808
#define VMCS_GUEST_SS_BASE                    0x0000680a
#define VMCS_GUEST_DS_BASE                    0x0000680c
#define VMCS_GUEST_FS_BASE                    0x0000680e
#define VMCS_GUEST_GS_BASE                    0x00006810
#define VMCS_GUEST_LDTR_BASE                  0x00006812
#define VMCS_GUEST_TR_BASE                    0x00006814
#define VMCS_GUEST_GDTR_BASE                  0x00006816
#define VMCS_GUEST_IDTR_BASE                  0x00006818
#define VMCS_GUEST_DR7                        0x0000681a
#define VMCS_GUEST_RSP                        0x0000681c
#define VMCS_GUEST_RIP                        0x0000681e
#define VMCS_GUEST_RFLAGS                     0x00006820
#define VMCS_GUEST_PENDING_DBG_EXCEPTIONS     0x00006822
#define VMCS_GUEST_SYSENTER_ESP               0x00006824
#define VMCS_GUEST_SYSENTER_EIP               0x00006826
#define VMCS_HOST_CR0                         0x00006c00
#define VMCS_HOST_CR3                         0x00006c02
#define VMCS_HOST_CR4                         0x00006c04
#define VMCS_HOST_FS_BASE                     0x00006c06
#define VMCS_HOST_GS_BASE                     0x00006c08
#define VMCS_HOST_TR_BASE                     0x00006c0a
#define VMCS_HOST_GDTR_BASE                   0x00006c0c
#define VMCS_HOST_IDTR_BASE                   0x00006c0e
#define VMCS_HOST_IA32_SYSENTER_ESP           0x00006c10
#define VMCS_HOST_IA32_SYSENTER_EIP           0x00006c12
#define VMCS_HOST_RSP                         0x00006c14
#define VMCS_HOST_RIP                         0x00006c16

#define EXIT_REASON_EXCEPTION_NMI       0
#define EXIT_REASON_EXTERNAL_INTERRUPT  1
#define EXIT_REASON_TRIPLE_FAULT        2
#define EXIT_REASON_PENDING_INTERRUPT   7
#define EXIT_REASON_NMI_WINDOW          8
#define EXIT_REASON_TASK_SWITCH         9
#define EXIT_REASON_CPUID               10
#define EXIT_REASON_HLT                 12
#define EXIT_REASON_INVD                13
#define EXIT_REASON_INVLPG              14
#define EXIT_REASON_RDPMC               15
#define EXIT_REASON_RDTSC               16
#define EXIT_REASON_VMCALL              18
#define EXIT_REASON_VMCLEAR             19
#define EXIT_REASON_VMLAUNCH            20
#define EXIT_REASON_VMPTRLD             21
#define EXIT_REASON_VMPTRST             22
#define EXIT_REASON_VMREAD              23
#define EXIT_REASON_VMRESUME            24
#define EXIT_REASON_VMWRITE             25
#define EXIT_REASON_VMOFF               26
#define EXIT_REASON_VMON                27
#define EXIT_REASON_CR_ACCESS           28
#define EXIT_REASON_DR_ACCESS           29
#define EXIT_REASON_IO_INSTRUCTION      30
#define EXIT_REASON_MSR_READ            31
#define EXIT_REASON_MSR_WRITE           32
#define EXIT_REASON_INVALID_STATE       33
#define EXIT_REASON_MWAIT_INSTRUCTION   36
#define EXIT_REASON_MONITOR_INSTRUCTION 39
#define EXIT_REASON_PAUSE_INSTRUCTION   40
#define EXIT_REASON_MCE_DURING_VMENTRY  41
#define EXIT_REASON_TPR_BELOW_THRESHOLD 43
#define EXIT_REASON_APIC_ACCESS         44
#define EXIT_REASON_EPT_VIOLATION       48
#define EXIT_REASON_EPT_MISCONFIG       49
#define EXIT_REASON_WBINVD              54
#define EXIT_REASON_XSETBV              55
#define EXIT_REASON_INVPCID             58






#define VMINFO_VMCS_COUNT   32
#ifdef __ASSEMBLER__
#define VMINFO_FLAGS        0
#define VMINFO_PML4         8
#define VMINFO_MEMORY_LOCK  16
#define VMINFO_VAPIC_PAGE   24
#define VMINFO_META         32
#define VMINFO_VMCS         40

#define VMINFO_SIZE         (40+(VMINFO_VMCS_COUNT*8))
#else

typedef struct
{
    uint64_t flags;
    uint64_t* pml4;
    uint64_t  memory_lock;
    uint64_t* vapic_page;
    uint64_t meta;
    uint64_t* vmcs[VMINFO_VMCS_COUNT];
} vminfo;

#define VMINFO_SIZE         (sizeof(vminfo))

#endif


