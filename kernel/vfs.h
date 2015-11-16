#pragma once

#include "../types.h"
#include "systemhandle.h"

// File path example: 01:/file.text


typedef struct
{
    bool (*fopen)(system_handle* h, char* name, uint64_t access_type);
    uint64_t (*fread)(system_handle* h, uint64_t count, char* destination);
    uint64_t (*fwrite)(system_handle* h, uint64_t count, char* destination);
    void (*fclose)(system_handle* h);
    void (*fseek)(system_handle* h, uint64_t count, bool absolute);
} file_operations;

struct _file_handle
{
    system_handle handle;
    struct _file_handle* next;
    file_operations* operations;

    uint64_t start; //sector number
    uint64_t position; //relative byte offset in file
    uint64_t size;
    uint64_t device;
};

typedef struct _file_handle file_handle;

file_handle* fopen(char* name, uint64_t access_type);
uint64_t fread(file_handle* f, uint64_t count, char* destination);
uint64_t fwrite(file_handle* f, uint64_t count, char* destination);
void fclose(file_handle* f);
void fseek(file_handle* f, uint64_t count, bool absolute);
