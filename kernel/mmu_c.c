#include "includes/kernel/types.h"
#include "../memorymap.h"
#include "macros.h"
#include "printf.h"

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

void setup_kernel_page_structure(uint64_t num_gig)
{
    uint64_t i,n;
    
    if (num_gig >= 512) return 0;

    PML4E* pml4 = (PML4E*)PML4TABLE;
    PDPTE* pdpt = (PDPTE*)PDPTTABLE;

    // Create pml4 entry for first 512gig
    pml4[0] = PDPTTABLE | 0b111LL;
    // Create pml4 entry for mirror space (bit 47, so starting at gig 256) 
    pml4[256] = PDPTTABLE | 0b111LL;

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
    n = (KERNEL_RESERVED_END/(2*1024*1024));
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
    virtual_address = KERNEL_RESERVED_END;
    uint64_t pageTableAddress = PAGETABLES;
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
            pt[i] = (PTE*)(virtual_address | 0b111LL | avl);
            virtual_address += 0x1000LL;
        }
        pageTableAddress += 0x1000LL;
        pdindex++;
        if (pdindex >= 512)
        {
            pdindex = 0;
            pdptindex++;
        }
    }

}


///////////////////////////////////////////////////////////////////////////////////////
// NOTE:    This mapping function is only used during page structure creation
//          It should not be used for assigning pages to live processes since
//          it doesn't have any locking mechanism. If such a function is needed,
//          look at mapPhysOnVirtualAddressSpace in mmu.S
//
///////////////////////////////////////////////////////////////////////////////////////
void add_process_mapping(uint64_t virt, uint64_t phys, uint64_t pageCount, uint64_t pagetableAddress)
{

    pagetableAddress += ((virt - KERNEL_RESERVED_END)/4096)*8;
    while (pageCount)
    {
        PTE* pte = (PTE*)pagetableAddress;
        *pte = phys | 0b111;
        pagetableAddress+=8;
        phys+=0x1000;    
        pageCount--;
    }
}

uint64_t setup_process_page_structure(uint64_t num_gig, uint64_t* pml4Address, uint64_t* pagetableAddress)
{
    uint64_t n,i;
    if (num_gig >= 512)
    {
        *pml4Address = 0;
        *pagetableAddress = 0;
        return;
    }

    PML4E* pml4 = (PML4E*)kernelAllocPages(1);    
    PDPTE* pdpt = (PDPTE*)kernelAllocPages(1);    
    *pml4Address = (uint64_t*)UNMIRROR(pml4);
     
    pml4[0] = UNMIRROR(pdpt) | 0b111;
    pml4[128] = PDPTTABLE | 0b111;      // mirror space
    
    // create PD entries
    for (n=0;n<num_gig;n++)
    {
        // Add an entry in PDPT
        uint64_t a = kernelAllocPages(1);
        pdpt[n] = UNMIRROR(a) | 0b111;
    }

    // Create 2meg entries for kernel mapping
    n = (KERNEL_RESERVED_END/(2*1024*1024));
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

    // Now do the 4k mapping of the rest of memory.
    address = KERNEL_RESERVED_END;
    uint64_t pageCount = ((num_gig*1024*1024*1024) - (KERNEL_RESERVED_END))/(2*1024*1024);
    uint64_t pageTableAddress = kernelAllocPages(pageCount);
    *pagetableAddress = UNMIRROR(pageTableAddress);

    while (pdptindex < num_gig)
    {
        PDE* pd = (PDE*)MIRROR((uint64_t)(pdpt[pdptindex]) & (~0b111111111111LL));
        pd[pdindex] = (PDE)UNMIRROR(pageTableAddress | 0b111LL);
        PTE* pt = (PTE*)pageTableAddress;
        for (i=0;i<512;i++)
        {
            pt[i] = 0;
            address += 0x1000LL;
        }
        pageTableAddress += 0x1000LL;
        pdindex++;
        if (pdindex >= 512)
        {
            pdindex = 0;
            pdptindex++;
        }
    }
    pageTableAddress = MIRROR(*pagetableAddress);
    
    // mmio. TODO: we should only map specific ranges
    add_process_mapping(0xFEC00000, 0xFEC00000, ((0xFF000000-0xFEC00000)/4096), pageTableAddress);

    return;
}
