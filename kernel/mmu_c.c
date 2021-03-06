#include "includes/kernel/types.h"
#include "../memorymap.h"
#include "macros.h"
#include "printf.h"
#include "utils.h"

#define MIRRORAREA(x) (((uint64_t)x)|(1LL<<MIRROR_BIT))
#define ISMIRROR_ADDRESS(x) (x>=(1LL<<MIRROR_BIT))

typedef uint64_t PML4E;
typedef uint64_t PDPTE;
typedef uint64_t PDE;
typedef uint64_t PTE;

typedef struct
{
    uint64_t address;
    uint64_t size;
    uint32_t type;
    uint32_t ext;
} mem_map_entry;

extern void* kernelAllocPages(uint64_t pageCount);
extern uint64_t allocateStackPage(uint64_t pageCount);

uint64_t ram_end;

bool is_page_reserved(uint64_t address)
{
    mem_map_entry* m = (mem_map_entry*)MEMMAP;
    while (m->size != 0)
    {
        if (m->type == 1)
        {
            if (address >= m->address && address < (m->address+m->size)) return false;
        }
        m++;
    }
    return true;
}

void show_memory_map()
{
    mem_map_entry* m = (mem_map_entry*)MEMMAP;
    pf("Usable memory: \r\n");
    while (m->size != 0)
    {
        if (m->type == 1)
        {
            pf("\tRegion: %X-%X\r\n",m->address,(m->address+m->size));
        }
        m++;
    }
}

void set_ram_end()
{
    ram_end = 0;
    mem_map_entry* m = (mem_map_entry*)MEMMAP;
    while (m->size != 0)
    {
        uint64_t end = m->address + m->size;
        if (m->type == 1)
        {
            if (ram_end < end) ram_end = end;
        }
        m++;
    }
}


void setup_kernel_page_structure()
{
    uint64_t i,n;
    uint64_t num_gig = (ram_end /(1024*1024*1024));

    PML4E* pml4 = (PML4E*)PML4TABLE;
    PDPTE* pdpt = (PDPTE*)PDPTTABLE;

    // Create pml4 entry for first 512gig
    pml4[0] = PDPTTABLE | 0b111LL;
    // Create pml4 entry for mirror space (bit 47, so starting at gig 256) 
    pml4[128] = PDPTTABLE | 0b111LL;

    // For each gig that we support, create a PD 
    i = 0;
    uint64_t firstPDAddress = PDTABLE;
    n = num_gig;
    while (n)
    {
        PDE* pd = (PDE*)firstPDAddress;
        pdpt[i] = (uint64_t)pd | 0b111LL;

        firstPDAddress += 0x1000;
        i++;
        n--;
    }

    
    // create the 2mb mappings of kernel area 
    // Kernel wants the entire section including page tables
    // these are defined until KERNERL_RESERVED_END
    n = (KERNEL_END/(2*1024*1024));
    uint64_t pdptindex = 0;
    uint64_t pdindex = 0;
    uint64_t virtual_address = 0;
    while (n)
    {
        PDE* pd = (PDE*)((uint64_t)(pdpt[pdptindex]) & (~0b111111111111LL));
        
        pd[pdindex] = virtual_address | 0b0010010000111LL;
        pdindex++;
        virtual_address += 0x200000LL;
        if (pdindex >= 512)
        {
            pdindex = 0;
            pdptindex++;
        }
        n--;
    }


    // Now do the 4k mapping of the rest of memory.
    // Those tables will be used as identity memory and also
    // as a structure to mark pages free or not
    // We technically don't need these entries since kernel code
    // wont run anymore after processes are started so this pml4 will not
    // be used. We could simply use a bitmap to track free pages.
    // but these tables are referenced by all processes for the mirror space
    virtual_address = KERNEL_END;
    uint64_t pageTableAddress = PAGETABLES;
    uint64_t pageTableEnd = PAGETABLES + (((ram_end-KERNEL_END)/4096)*8);
    while (pdptindex < num_gig)
    {
        PDE* pd = (PDE*)((uint64_t)(pdpt[pdptindex]) & (~0b111111111111LL));
        pd[pdindex] = (PDE)(pageTableAddress | 0b111LL);
        PTE* pt = (PTE*)pageTableAddress;
        for (i=0;i<512;i++)
        {

            // Using the data returned by BIOS e820, we will mark
            // all non-free addresses with AVL=0b100 so that the page allocation
            // algorithm does not use that physical memory to map on a virtual address
            // for a process
            uint64_t avl = 0;
            if (is_page_reserved(virtual_address))
            {   
                avl = (0b100 << 9);
            }

            if (virtual_address < pageTableEnd)
            {   
                avl = (0b100 << 9);
            }
            pt[i] = (PTE*)(virtual_address | 0b111LL | avl);
            virtual_address += 0x1000LL;
        }
        pageTableAddress += 0x1000LL;
        
        // we also need to check if the page that will contain the pagetable
        // is reserved. Otherwise we will end up creating a page table
        // in reserved memory. We should avoid that, but for now, let's 
        // just crash :)
        if (is_page_reserved(pageTableAddress))
        {
            C_BREAKPOINT_VAR(0xDEADBEEF,0x66778899,pageTableAddress,0);
        }
        pdindex++;
        if (pdindex >= 512)
        {
            pdindex = 0;
            pdptindex++;
        }
    }


}


uint64_t setup_process_page_structure()
{
    uint64_t n,i;
    uint64_t num_gig = ram_end>>30;

    PML4E* pml4 = (PML4E*)kernelAllocPages(1);    
    PDPTE* pdpt = (PDPTE*)kernelAllocPages(1);    
     
    pml4[0] = UNMIRROR(pdpt) | 0b111;
    pml4[128] = PDPTTABLE | 0b111;      // mirror space
    
    pdpt[0] = UNMIRROR(kernelAllocPages(1)) | 0b111;

    // Create 2meg entries for kernel mapping
    n = (KERNEL_END/(2*1024*1024));
    uint64_t pdindex = 0;
    uint64_t pdptindex = 0;
    uint64_t address = 0;
    for (i=0;i<n;i++)
    {
        PDE* pd = (PDE*)MIRROR((uint64_t)(pdpt[pdptindex]) & (~0b111111111111LL));
        pd[pdindex] = address | 0b0010010000111LL;

        address += (2*1024*1024);
        pdindex++;
        if (pdindex >=512)
        {
            pdindex=0;
            pdptindex++;
        }
    } 

    return (uint64_t)UNMIRROR(pml4);
}


bool mapPhysicalAddressToVirtual(PML4E* pml4, uint64_t virt, uint64_t phys, uint64_t mask)
{
    uint64_t pml4index = (virt>>(12+9+9+9))&0x1FF;
    uint64_t pdptindex = (virt>>(12+9+9))&0x1FF;
    uint64_t pdindex = (virt>>(12+9))&0x1FF;
    uint64_t ptindex = (virt>>(12))&0x1FF;

    PML4E pml4e = pml4[pml4index];
    if (pml4e == 0) 
    {
        pml4e = UNMIRROR(kernelAllocPages(1));   
        pml4[pml4index] = pml4e | 0b111;
    }
    PDPTE* pdpt = MIRRORAREA(((uint64_t)pml4e & 0x00FFFFFFFFFFF000LL));
    PDPTE pdpte = pdpt[pdptindex];
    if (pdpte == 0)
    {
        pdpte = UNMIRROR(kernelAllocPages(1));   
        pdpt[pdptindex] = pdpte | 0b111;
    }
    PDE* pd = MIRRORAREA(((uint64_t)pdpte & 0x00FFFFFFFFFFF000LL));
    PDE pde = pd[pdindex];
    if (pde == 0)
    {
        pde = UNMIRROR(kernelAllocPages(1));   
        pd[pdindex] = pde | 0b111;
    }
    PTE* pt = MIRRORAREA(((uint64_t)pde & 0x00FFFFFFFFFFF000LL));
    PTE* pte = MIRRORAREA((uint64_t)&pt[ptindex]);
    
    phys = phys | mask | (1LL<<9) | (1LL<<0);
    *pte = phys; 
    return true;
}

bool mapMultiplePhysicalAddressToVirtualWithMask(PML4E* pml4, uint64_t virt, uint64_t phys, uint64_t pageCount, uint64_t mask)
{
    while (pageCount)
    {
        mapPhysicalAddressToVirtual(pml4, virt, phys, mask);
        virt+=0x1000;
        phys+=0x1000;
        pageCount--;
    }
}

bool mapMultiplePhysicalAddressToVirtual(PML4E* pml4, uint64_t virt, uint64_t phys, uint64_t pageCount)
{
    mapMultiplePhysicalAddressToVirtualWithMask(pml4,virt,phys,pageCount,0b110);
}
