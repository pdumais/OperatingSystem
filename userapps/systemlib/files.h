#include "types.h"

#define INTA0_FOPEN 0x40
#define INTA0_FREAD 0x41
#define INTA0_FWRITE 0x42
#define INTA0_FCLOSE 0x43
#define INTA0_FSEEK 0x44
#define INTA0_FGETSIZE 0x45

typedef void file_handle;

file_handle* fopen(char* name, uint64_t access_type);
uint64_t fread(file_handle* f, uint64_t count, char* destination);
uint64_t fwrite(file_handle* f, uint64_t count, char* destination);
void fclose(file_handle* f);
void fseek(file_handle* f, uint64_t count, bool absolute);
uint64_t fgetsize(file_handle* f);
