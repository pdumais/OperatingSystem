#include "files.h"

file_handle* fopen(char* name, uint64_t access_type)
{
    file_handle* ret;
    __asm("int $0xA0" : "=a"(ret) : "D"(name),"S"(access_type),"a"(INTA0_FOPEN));
    return ret;
}

uint64_t fread(file_handle* f, uint64_t count, char* destination)
{
    uint64_t ret;
    __asm("int $0xA0" : "=a"(ret) : "D"(f),"S"(count),"d"(destination),"a"(INTA0_FREAD));
    return ret;
}

uint64_t fwrite(file_handle* f, uint64_t count, char* source)
{
    uint64_t ret;
    __asm("int $0xA0" : "=a"(ret) : "D"(f),"S"(count),"d"(source),"a"(INTA0_FWRITE));
    return ret;
}

void fclose(file_handle* f)
{
    __asm("int $0xA0" : : "D"(f),"a"(INTA0_FCLOSE));
}

void fseek(file_handle* f, uint64_t count, bool absolute)
{
    __asm("int $0xA0" : : "D"(f),"S"(count),"d"(absolute),"a"(INTA0_FSEEK));
}

uint64_t fgetsize(file_handle* f)
{
    uint64_t ret;
    __asm("int $0xA0" : "=a"(ret) : "D"(f),"a"(INTA0_FGETSIZE));
    return ret;
}
