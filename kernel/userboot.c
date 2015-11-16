#include "../types.h"
#include "block_cache.h"
#include "userprocess.h"
#include "vfs.h"

// This MUST be a multiple of 4096
//TODO: we should determine the count dynamically. This will be easy when using FAT32
#define ELFBUFFER (4*4096)

extern void* kernelAllocPages(uint64_t count);
extern void kernelReleasePages(uint64_t addr, uint64_t count);
extern void showMem();
extern void strcpy(char* src, char* dst);

uint64_t deviceContainingScript;
uint64_t appParam = 0;

struct __attribute__((__packed__)) ElfHeader
{
    uint32_t magic;
    uint8_t  bits;
    uint8_t  endian;
    uint8_t  version1;
    uint8_t  abi;
    uint64_t reserved1;
    uint16_t relocatable;
    uint16_t instructionSet;
    uint32_t version2;
    uint64_t programPosition;
    uint64_t programHeaderPosition;
    uint64_t sectionHeaderPosition;
    uint32_t flags;
    uint16_t headerSize;
    uint16_t programHeaderEntrySize;
    uint16_t programHeaderEntryCount;
    uint16_t sectionHeaderEntrySize;
    uint16_t sectionHeaderEntryCount;
    uint16_t sectionHeaderNamesIndex;
};
struct __attribute__((__packed__)) SectionHeader64
{
    uint32_t    nameIndex;
    uint32_t    type;
    uint64_t    flags;
    uint64_t    virtualAddress;
    uint64_t    offsetInFile;
    uint64_t    size;
    uint32_t    link;
    uint32_t    info;
    uint64_t    align;
    uint64_t    entrySize;
};


uint8_t isValidSection(struct SectionHeader64* sh)
{
    // We only support progbits (data,rodata,text) and no bits (bss)
    if (sh->type != 1 && sh->type != 8) return 0;

    // only sections that needs allocation (alloc flag)
    if ((sh->flags & 0x02) == 0) return 0;

    // And, for now, we only allow sections aligned on a 4k boundary
    if ((sh->virtualAddress&0xFFF) > 0)
    {
        // But the text section will map on a non-4k-boundary. 
        // That's the only exception
        if (sh->virtualAddress!=USER_PROCESS_CODE_START)
        {
            return 0;
        }
    }

    return 1;
}




uint64_t loadProcess(char* name, bool createNewConsole)
{
    uint64_t i;
    uint64_t n;
    uint64_t size;
    char* elfBuffer;

    file_handle* f = fopen(name,ACCESS_TYPE_READ);
    if (f == 0)
    {
        pf("File not found\r\n");
        return 0;
    }

    size = fgetsize(f);
    if (size > ELFBUFFER)
    {
        pf("Can't load application. Too big\r\n");
        fclose(f);
        return 0;
    }
    elfBuffer = (char*)malloc(size);

    fread(f,size,elfBuffer);
    struct ElfHeader *eh = (struct ElfHeader*)elfBuffer;

    struct UserProcessInfo upi;
    upi.entryPoint  = (char*)eh->programPosition;
    upi.entryParameter = appParam;
    if (createNewConsole)
    {
        upi.consoleSteal = 0; 
    }
    else
    {
        // the new process will takeover the current console
        __asm("mov %%cr3,%0" : "=r"(upi.consoleSteal));
    }
    createUserProcess(&upi);    
    char* sectionHeaders = elfBuffer[eh->sectionHeaderPosition];
    for (n = 0; n < eh->sectionHeaderEntryCount; n++)
    {
        struct SectionHeader64* sh = (struct SectionHeader64*)&elfBuffer[eh->sectionHeaderPosition+(n*sizeof(struct SectionHeader64))];
        if (isValidSection(sh) == 0) continue;

        bool readOnly = !(sh->flags&1);
        bool executable = (sh->flags&4);
        bool initZero = (sh->type==8);
    
        addUserProcessSection(&upi,&elfBuffer[sh->offsetInFile], sh->virtualAddress, sh->size, readOnly, executable, initZero);
    }

    createProcessHeap(&upi);
    launchUserProcess(&upi);
    appParam++;
    fclose(f);
    free((void*)elfBuffer);
    return upi.psi.pml4;
}

//TODO: right now, we load apps from raw disk, but we should support a file system
void loadUserApplications(uint64_t device)
{
    unsigned int i,n;
    uint64_t param=0;
    char bootscript[512]; 
    char fname[512];
    
    if (device >9)
    {
        pf("Bad device name\r\n");
        return;
    }

    fname[0]='0';
    fname[1]=0x30+device;  
    fname[2]=':';
    fname[3]='/';
    strcpy("bootscript",(char*)&fname[4]);
    file_handle* f = fopen(fname,ACCESS_TYPE_READ);
    if (f == 0)
    {
        pf("ERROR: bootscript file [%s] not found\r\n",fname);
        return;
    }

    block_cache_read(1,device,bootscript,1);    // read bootscript
    fread(f,512,bootscript);
    fclose(f);

    n = 0;
    for (i = 0; i< 512; i++)
    {
        if (bootscript[i] == 0) break;
        if (bootscript[i] == 0x0A)
        {
            bootscript[i]=0;
            strcpy((char*)&bootscript[n],(char*)&fname[4]);
            fname[4+i-n]=0;
            pf("loading [%s] from bootscript\r\n",fname);
            loadProcess(fname,true);
            n = i+1;
        }
    }
}


