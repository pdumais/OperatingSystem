#include "threads.h"

#define INTA0_GET_APIC_ID 0
#define INTA0_VIRT2PHYS   1

uint64_t getCurrentCPU()
{
    uint64_t cpu;

    __asm("int $0xA0" : "=a"(cpu) : "a"(INTA0_GET_APIC_ID));

    return cpu;
}

uint64_t virt2phys(uint64_t addr)
{
    uint64_t ret;

    __asm("int $0xA0" : "=a"(ret) : "D"(addr),"a"(INTA0_VIRT2PHYS));

    return ret;
}
