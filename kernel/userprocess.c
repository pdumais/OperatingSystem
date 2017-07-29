#include "includes/kernel/types.h"
#include "printf.h"
#include "../memorymap.h"
#include "userprocess.h"
#include "macros.h"

#define MIRRORAREA(x) (((uint64_t)x)|(1LL<<MIRROR_BIT))
#define ISMIRROR_ADDRESS(x) (x>=(1LL<<MIRROR_BIT))


extern void* virt2phys(void* address, uint64_t pml4);
extern void* allocateStackPage(uint64_t pageCount);
extern void* allocateHeapPage(uint64_t pageCount);
extern uint64_t setupProcessPageStructureForUserProcess();
extern void destroyProcessPageStructure(uint64_t pml4_address);
extern bool mapMultiplePhysicalAddressToVirtualWithMask(uint64_t pml4, uint64_t virt, uint64_t phys, uint64_t pageCount, uint64_t mask);
extern void createProcessStub(char* codeArea, char* entryPoint);
extern void memcpy64(char* source, char* destination, uint64_t size);
extern void memclear64(char* destination, uint64_t size);
extern void addTaskInList(uint64_t* pml4Address);
extern uint64_t cleanOneDeadTask();
extern uint64_t calculateMemoryUsage();
extern void releasePhysicalPage(uint64_t physicalAddress);
extern void init_heap(uint64_t physicalAddressOfHeap);
extern uint64_t ram_end;


void setUserProcessCodeSection(struct UserProcessInfo* upi, char* buffer, uint64_t virtualAddress, uint64_t size);

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// This is for user threads. The code will be copied in another
// area. The 4k PTEs will stay empty and be used
// as heap for the user thread
// This is multi-thread and multi-processor friendly for allocating memory
// and also for manipulating task list.
//
// WARNING: This function should not be called to dispatch kernel code that was
//   built with the kernel. This will displace the code to another area in memory
//   so obviously any branching to a function out of the thread will be
//   miscalculated since the branches are relative. IE: creating a test thread
//   in this file and calling pf() inside of it.
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
void createUserProcess(struct UserProcessInfo* upi)
{

    // Create 5 stack pages. The function returns (in rax)
    // the physical address to the begining of the block
    // The last page is used for process meta-data.
    char* stack0Pages = (char*)allocateStackPage(5);
    char* metaPage = stack0Pages+(0x1000*4);
    char* stack3Pages = (char*)allocateStackPage(4); 
    upi->metaPage = (char*)MIRRORAREA(stack0Pages+0x4000);
    *((uint64_t*)&upi->metaPage[FILE_HANDLE_ADDRESS-META_VIRTUAL_ADDRESS]) = 0;

    // Create the paging structures and get the addess of the page tables
    upi->psi.pml4 = setupProcessPageStructureForUserProcess();
    upi->lastProgramAddress = 0;

    // Create the virtual mapping
    mapMultiplePhysicalAddressToVirtualWithMask(MIRRORAREA(upi->psi.pml4),STACK0TOP_VIRTUAL_ADDRESS-(0x1000*4),(uint64_t)stack0Pages,4,(1LL<<63)|(0<<2)|(1<<1));
    mapMultiplePhysicalAddressToVirtualWithMask(MIRRORAREA(upi->psi.pml4),META_VIRTUAL_ADDRESS,(uint64_t)metaPage,1,(1LL<<63)|(1<<2)|(1<<1));
    mapMultiplePhysicalAddressToVirtualWithMask(MIRRORAREA(upi->psi.pml4),STACK3TOP_VIRTUAL_ADDRESS-(0x1000*4),(uint64_t)stack3Pages,4,(1LL<<63)|(1<<2)|(1<<1));


    // Prepare stack
    uint64_t* stack = (uint64_t*)MIRRORAREA((stack0Pages+(0x1000*4)));
    // This portion is for the iretq of the scheduler handler
    stack[-1] = 0x33;                                   // ss : descriptor 0x30 with RPL=3
    stack[-2] = (uint64_t)STACK3TOP_VIRTUAL_ADDRESS;    // rsp
    stack[-3] = 0x200202;                               // rflags
    stack[-4] = RING3CSSELECTOR|3;                      // cs with RPL =3
    stack[-5] = THREAD_CODE_START;                      // rip

    // This portion is for context restore
    stack[-6] = 0;                                      // rax
    stack[-7] = upi->entryParameter;                    // rdi
    stack[-8] = upi->consoleSteal;                      // rbx
    stack[-9] = 0;                                      // rcx
    stack[-10] = 0;                                     // rdx
    stack[-11] = 0;                                     // rsi
    stack[-12] = 0;                                     // rbp
    stack[-13] = 0;                                     // r8
    stack[-14] = 0;                                     // r9
    stack[-15] = 0;                                     // r10
    stack[-16] = 0;                                     // r11
    stack[-17] = 0;                                     // r12
    stack[-18] = 0;                                     // r13
    stack[-19] = 0;                                     // r14
    stack[-20] = 0;                                     // r15
    stack[-21] = -1;                                    // VMCS

}

void createProcessHeap(struct UserProcessInfo* upi)
{
    char* pages = (char*)allocateHeapPage(1);
    uint64_t virtualAddress = (upi->lastProgramAddress+0xFFF)&~0xFFF;
    mapMultiplePhysicalAddressToVirtualWithMask(MIRRORAREA(upi->psi.pml4),virtualAddress,(uint64_t)pages,1, (1LL<<63)|(1<<2)|(1<<1));
    init_heap(MIRRORAREA(pages));
    *((uint64_t*)&upi->metaPage[PROCESS_HEAP_ADDRESS-META_VIRTUAL_ADDRESS]) = virtualAddress;
}

void addUserProcessSection(struct UserProcessInfo* upi, char* buffer, uint64_t virtualAddress, uint64_t size, bool readonly, bool execute, bool initZero)
{
    if (virtualAddress == USER_PROCESS_CODE_START)
    {
        setUserProcessCodeSection(upi, buffer, virtualAddress, size);
        return;
    }

    //pf("add section: %x  rw=%x  nx=%x\r\n",virtualAddress,!readonly, !execute);
    uint64_t mask = (1<<2);
    if (!readonly) mask |= (1LL<<1);
    if (!execute) mask |= (1LL<<63);
    uint64_t pageCount = (size+0xFFF)>>12; 
    char* pages = (char*)allocateHeapPage(pageCount);
    mapMultiplePhysicalAddressToVirtualWithMask(MIRRORAREA(upi->psi.pml4),virtualAddress,(uint64_t)pages,pageCount, mask);
    pages = (char*)MIRRORAREA(pages);
    if (initZero)
    {
        memclear64(pages,pageCount*0x1000);
    }
    else
    {
        memcpy64(buffer,pages,size);
    }
    if ((virtualAddress+(pageCount*0x1000))>upi->lastProgramAddress) upi->lastProgramAddress = virtualAddress+(pageCount*0x1000);
}




void setUserProcessCodeSection(struct UserProcessInfo* upi, char* buffer, uint64_t virtualAddress, uint64_t size)
{
    uint64_t mask = (1LL<<2); // execute, readonly, U/S
    uint64_t pageCount = ((size+USER_PROCESS_STUB_SIZE)+0xFFF)>>12; 
    char* pages = (char*)allocateHeapPage(pageCount);
    mapMultiplePhysicalAddressToVirtualWithMask(MIRRORAREA(upi->psi.pml4),virtualAddress-USER_PROCESS_STUB_SIZE,(uint64_t)pages,pageCount,mask);
    pages = (char*)MIRRORAREA(pages);
    memcpy64(buffer,(char*)&pages[USER_PROCESS_STUB_SIZE],size);
    createProcessStub(pages, upi->entryPoint);       

    if ((virtualAddress+(pageCount*0x1000))>upi->lastProgramAddress) upi->lastProgramAddress = virtualAddress+(pageCount*0x1000);
}

void launchUserProcess(struct UserProcessInfo* upi)
{
    // Add in task list
    addTaskInList(upi->psi.pml4);
}


void destroyProcessMemory(uint64_t pml4)
{
    uint64_t    virtualAddress;

    // This is a slow operation but we have no other choice
    // than to translate page-per-page since we have no
    // guarantee that all page tables are sequential in 
    // physical memory (at least, we don't want to assume so)
    //
    // We could release those pages in destroyProcessPageStructure
    // since it will already iterate through all tables and detect
    // pages, but I prefer to leave the control here. We can
    // validate that the page is not a mirror page etc..

    for (virtualAddress = THREAD_CODE_START; virtualAddress < ram_end; virtualAddress += 0x1000)
    {
        uint64_t phys = (uint64_t)virt2phys(virtualAddress,pml4);
        uint64_t pageFlags = phys&0x8000000000000FFF;
        phys = phys & ~0x8000000000000FFF;
        if (ISMIRROR_ADDRESS(phys)) break;                          // stop when getting mirror address
        if ((pageFlags&0b111000000000) == 0) continue;              // AVL bits cleared = page is free
        releasePhysicalPage(phys);
    }

    destroyProcessPageStructure(pml4);
}

void manageDeadProcesses()
{
    uint64_t memdiff;
    uint64_t pml4 = cleanOneDeadTask();
    if (pml4 != 0)
    {
        // At this point, the task is guaranteed to not run on any
        // other CPU, and will never run again.
        // The task is guaranteed no to write out to its console output
        // since it is not running and writing is done synchronously.
        // But there is a window of potential crash in the keyboard handler
        // that is described in the removeConsole function. Other than that
        // race condition, it is perfectly safe to destroy all the memory 
        // associated to that process
        memdiff = calculateMemoryUsage();               // Terribly bad for performance, for debug only
//        pf("Reaping dead task [%x] ...",pml4);
        destroyProcessMemory(pml4);
        memdiff -= calculateMemoryUsage();  
//        pf(" %x pages freed\r\n",memdiff);
    }
}



