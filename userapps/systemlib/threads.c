#include "threads.h"

#define INTA0_GET_APIC_ID 0

uint64_t getCurrentCPU()
{
    uint64_t cpu;

    __asm("int $0xA0" : "=a"(cpu) : "a"(INTA0_GET_APIC_ID));

    return cpu;
}
