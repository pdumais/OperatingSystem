#pragma once

#include "includes/kernel/types.h"
#include "includes/kernel/systemhandle.h"

#define ACCESS_TYPE_READ 1
#define ACCESS_TYPE_WRITE 2

// File path example: 01:/file.text


// A system_handle is the base of all handles in the system. It provides a functor
// for a destructor function. On process death, all system_handle's destructor 
// function will be invoked to make sure that resources are cleaned appropriately.
//
// A file_handle contains a system_handle. When a process dies, the system_handle's 
// destructor gets called. The kernel finds all opened file handles in a linked-list.
// Each file_handle has a previous and next field because it is a linked-list.

typedef struct
{
    bool (*fopen)(system_handle* h, char* name, uint64_t access_type);
    uint64_t (*fread)(system_handle* h, uint64_t count, char* destination);
    uint64_t (*fwrite)(system_handle* h, uint64_t count, char* destination);
    void (*fclose)(system_handle* h);
    void (*fseek)(system_handle* h, uint64_t count, bool absolute);
    uint64_t (*fgetsize)(system_handle* h);
} file_operations;

struct _file_handle
{
    system_handle handle;
    struct _file_handle* next;
    struct _file_handle* previous;
    file_operations operations;

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
uint64_t fgetsize(file_handle* f);
