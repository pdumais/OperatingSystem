#include "../memorymap.h"
#include "display.h"

extern void* allocateStackPage(unsigned long pageCount);
extern void* allocateHeapPage(unsigned long pageCount);
extern void setupProcessPageStructure_c();

#define IDENTITY(x) ((void*)((unsigned long)x|0x4000000000))

void* launchUserThread(void* entryPoint, unsigned long size, unsigned long data)
{
    unsigned long i;
    unsigned long tmp;
    unsigned long numPages = (size+(0xFFF))&0xFFF;
    unsigned long pml4Address;
    unsigned long pageTables;
    void* codeStart = allocateHeapPage(numPages);
    void* stackPages = allocateStackPage(5);                    // 4 for stack and 1 for Meta page
    char* codeDestination = (char*)IDENTITY(codeStart);
    unsigned long* pageTablesBuffer;
    unsigned long* stackTop = (unsigned long*)IDENTITY(stackPages);
    stackTop += (0x1000*4/8);

    // Copy code to newly assigned area
    for (i = 0; i < size; i++) codeDestination[i] = ((char*)entryPoint)[i];
    
    // Create the page tables
    setupProcessPageStructure_c(&pml4Address, &pageTables); 
    pageTablesBuffer = IDENTITY(pageTables);

    //TODO: create user stack

    // create virtual memory mapping for stack
    tmp  = (unsigned long)stackPages;
    for (i = 0; i < 4 ;i++)
    {
        unsigned int n = ((STACK0TOP_VIRTUAL_ADDRESS-KERNEL_RESERVED_END)>>12)-4+i;
//pf("meow %x   %x\r\n",n, tmp);
        pageTablesBuffer[n] = tmp|0b001000000111;
        tmp+=0x1000;
    }
    pageTablesBuffer[(META_VIRTUAL_ADDRESS-KERNEL_RESERVED_END)>>12] = tmp|0b001000000111;


//pf("CODE: %x  %x   %x   \r\n", pageTables, stackPages);

    // create virtual memory mapping for code at 0x08000000
    tmp = (unsigned long)codeStart;
    for (i = 0; i<numPages; i++)
    {
        pageTablesBuffer[i] = (unsigned long)codeStart|0b0011000000111;
        codeStart+=0x1000;
    }

    //TODO: create stack data
    stackTop[-1] = 0;               // ss    
    stackTop[-2] = 0;               // Ring3 stack TODO: set this as ring 3 when ready
        

    //TODO add in task list

    return codeStart;
}
