#include "memory.h"

#define INTA0_MALLOC 0x30
#define INTA0_FREE 0x31

void* malloc(uint64_t size)
{
    uint64_t ret;
    __asm("int $0xA0" : "=a"(ret) : "D"(size), "a"(INTA0_MALLOC));
    return (void*)ret;
}

void free(void* buffer)
{
    __asm("int $0xA0" : : "D"(buffer), "a"(INTA0_FREE));
}
