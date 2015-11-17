#include "memory.h"
#include "kernel/intA0.h"


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
