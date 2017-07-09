#include "includes/kernel/types.h"
#include "vmx.h"
#include "macros.h"
#include "../memorymap.h"
#include "console.h"

extern uint64_t* kernelAllocPages(unsigned int pageCount);
extern void spinLock(uint64_t*);
extern void spinUnlock(uint64_t*);
extern uint64_t get_apic_address();
extern Screen* getDirectVideo();

//TODO: when deleting a VM, we should free all those pages.

uint64_t ept_setup_guest_memory(uint64_t size_gig)
{
    uint64_t i,n;
    uint64_t pde_index, pdpte_index, pte_index;
    uint64_t* pml4;
    uint64_t* dummy_page;

    // Allocate the page for the PML4 table
    pml4 = kernelAllocPages(1);
    dummy_page = kernelAllocPages(1); 
    for (i=0;i<512;i++) dummy_page[i]=0;
   
    // Only use one pml4e since it can address 512 gig
    uint64_t* pdpt = kernelAllocPages(1);
    uint64_t pml4e = UNMIRROR(pdpt) | (0b010000000111);
    pml4[0] = pml4e;
 
    // We need one PDPT for each gig.
    for (pdpte_index=0;pdpte_index<size_gig;pdpte_index++)
    {
        uint64_t* pd = kernelAllocPages(1);
        uint64_t pdpte = UNMIRROR(pd) | (0b010000000111); 
        pdpt[pdpte_index] = pdpte;

        // then we need 1 PD for each 2mb inside the gig
        for (pde_index=0;pde_index<512;pde_index++)
        {
            uint64_t* pt = kernelAllocPages(1);
            uint64_t pde = UNMIRROR(pt) | (0b010100000111);
            pd[pde_index] = pde;

            for (pte_index=0;pte_index<512;pte_index++)
            {
                // Initially, all ram will point to a zero'd out RO page.
                // It will give the impression that all ram is available 
                // but will trigger a vmexit when trying to write in it so we can
                // lazily assign new pages
                uint64_t pte = UNMIRROR(dummy_page) | (0b010001000101); 
                pt[pte_index] = pte;
            }
        }
    }


    uint64_t phys_pml4 = (uint64_t)pml4;
    return phys_pml4;    
}

uint64_t* ept_get_pte(uint64_t* pml4, uint64_t vm_start_address)
{
    uint64_t pml4_index = vm_start_address >> 39;
    uint64_t pdpt_index = vm_start_address >> 30;
    uint64_t pd_index = vm_start_address >> 21;
    uint64_t pt_index = vm_start_address >> 12;

    uint64_t* pdpt = MIRROR(pml4[pml4_index] & (~0xFFF));
    uint64_t* pd = MIRROR(pdpt[pdpt_index] & (~0xFFF));
    uint64_t* pt = MIRROR(pd[pd_index] & (~0xFFF));
    
    return (uint64_t*)&pt[pt_index];

}


void ept_map_pages(uint64_t vm_start_address, uint64_t map_address, uint64_t page_count, vminfo* vm)
{
    uint64_t i;

    spinLock(vm->memory_lock);

    //TODO: should check it not already mapped
    for (i=0;i<page_count;i++)
    {
        uint64_t* pte = ept_get_pte(vm->pml4, vm_start_address);

        *pte = map_address | 0b010001000111;

        vm_start_address += 4096;
        map_address += 4096;
    }

    spinUnlock(vm->memory_lock);
}

void ept_map_video_buffer(vminfo* vm)
{
    Screen* s = getDirectVideo();
//__asm__("int $3": : "a"(s->backBuffer));
    
    ept_map_pages(0xB8000, UNMIRROR(s->backBuffer), 1, vm);
}

void ept_init_static_pages(vminfo* vm)
{
    
//    uint64_t apic_base = get_apic_address();
//    ept_map_pages(VAPIC_GUEST_ADDRESS,apic_base, 1, vm);
}

uint64_t* ept_allocate_pages(uint64_t vm_start_address, uint64_t page_count, vminfo* vm)
{
    uint64_t i;


    uint64_t* addr = kernelAllocPages(page_count);
    uint64_t realaddr = UNMIRROR(addr);
    

    ept_map_pages(vm_start_address, realaddr, page_count, vm);

    return  addr;
}

