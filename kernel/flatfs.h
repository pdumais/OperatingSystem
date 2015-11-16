#include "../types.h"
#include "vfs.h"

// This is a dumb flat FS implementation that I use while I dont
// support any other FS

bool flatfs_fopen(system_handle* h, char* name, uint64_t access_type);
uint64_t flatfs_fread(system_handle* h, uint64_t count, char* destination);
uint64_t flatfs_fwrite(system_handle* h, uint64_t count, char* destination);
void flatfs_fclose(system_handle* h);
void flatfs_fseek(system_handle* h, uint64_t count, bool absolute);

