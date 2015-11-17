#include "types.h"
#include "kernel/intA0.h"

typedef void file_handle;

file_handle* fopen(char* name, uint64_t access_type);
uint64_t fread(file_handle* f, uint64_t count, char* destination);
uint64_t fwrite(file_handle* f, uint64_t count, char* destination);
void fclose(file_handle* f);
void fseek(file_handle* f, uint64_t count, bool absolute);
uint64_t fgetsize(file_handle* f);
