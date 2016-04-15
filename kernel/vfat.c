#include "vfat.h"
#include "block_cache.h"

//TODO: this driver is not implemented yet

extern void memcpy64(char* source, char* destination, uint64_t size);

void vfat_system_handle_destructor(system_handle* h);

bool vfat_fopen(system_handle* h, char* name, uint64_t access_type)
{
    return false;
}

uint64_t vfat_fread(system_handle* h, uint64_t count, char* destination)
{
    return 0;
}
uint64_t vfat_fwrite(system_handle* h, uint64_t count, char* destination)
{
    return 0;
}

void vfat_fclose(system_handle* h)
{
}

void vfat_fseek(system_handle* h, uint64_t count, bool absolute)
{
}

uint64_t vfat_fgetsize(system_handle* h)
{
    file_handle* f = (file_handle*)h;
    return f->size;
}
void vfat_system_handle_destructor(system_handle* h)
{
    //TODO: should release any held locks
    //TODO: should cancel any pending block operations
}
