#include "includes/kernel/types.h"
#include "vfs.h"

bool vfat_fopen(system_handle* h, char* name, uint64_t access_type);
uint64_t vfat_fread(system_handle* h, uint64_t count, char* destination);
uint64_t vfat_fwrite(system_handle* h, uint64_t count, char* destination);
void vfat_fclose(system_handle* h);
void vfat_fseek(system_handle* h, uint64_t count, bool absolute);
uint64_t vfat_fgetsize(system_handle* h);
