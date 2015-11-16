#include "vfs.h"
#include "flatfs.h"
#include "../memorymap.h"

extern void* currentProcessVirt2phys(void* address);
void add_file_handle_to_list(file_handle* f);
void remove_file_handle_from_list(file_handle* f);

file_handle* fopen(char* name, uint64_t access_type)
{
    file_handle* f = (file_handle*)malloc(sizeof(file_handle));    

    f->operations.fopen = &flatfs_fopen;
    f->operations.fread = &flatfs_fread;
    f->operations.fwrite = &flatfs_fwrite;
    f->operations.fseek = &flatfs_fseek;
    f->operations.fclose = &flatfs_fclose;
    f->operations.fgetsize = &flatfs_fgetsize;

    if (!f->operations.fopen(f,name,access_type))
    {
        free((void*)f);
        return 0;
    }

    // add in list
    add_file_handle_to_list(f);
    return f;
}

void add_file_handle_to_list(file_handle* f)
{
    //TODO: this should lock so that it would be multi-thread safe
    file_handle* firstHandle = *((file_handle**)FILE_HANDLE_ADDRESS);    

    f->next = 0;
    if (firstHandle == 0)
    {
        *((file_handle**)FILE_HANDLE_ADDRESS) = (file_handle*)currentProcessVirt2phys((void*)f);
        f->previous = 0;
    }
    else
    {
        while (firstHandle->next != 0) firstHandle = firstHandle->next;
        firstHandle->next = (file_handle*)currentProcessVirt2phys((void*)f);
        f->previous = firstHandle;
    }
}

void remove_file_handle_from_list(file_handle* f)
{
    file_handle *previous = f->previous;
    file_handle *next = f->next;

    //TODO: this should lock so that it would be multi-thread safe
    if (previous == 0)
    {
        *((file_handle**)FILE_HANDLE_ADDRESS) = next;
        if (next != 0) next->previous = 0;
    }
    else
    {
        previous->next = next;
        if (next!=0) next->previous = previous;
    }

}

uint64_t fread(file_handle* f, uint64_t count, char* destination)
{
    return f->operations.fread((system_handle*)f,count,destination);
}

uint64_t fwrite(file_handle* f, uint64_t count, char* destination)
{
    return f->operations.fwrite((system_handle*)f,count,destination);
}

void fclose(file_handle* f)
{
    f->operations.fclose((system_handle*)f);
    remove_file_handle_from_list(f);
    free((void*)f);
}

void fseek(file_handle* f, uint64_t count, bool absolute)
{
    f->operations.fseek((system_handle*)f,count,absolute);
}

uint64_t fgetsize(file_handle* f)
{
    return f->operations.fgetsize(f);
}

// This function will be invoked by the kernel when killing a task
// This function should not lock because we want to avoid deadlocks when
// force-killing the task. No other threads should be messing with
// the current process's file handles anyway since the process is dying.
void destroyFileHandles()
{
    file_handle* f = *((file_handle**)FILE_HANDLE_ADDRESS);    
    while (f != 0)
    {
        system_handle* h = (system_handle*)f;
        h->destructor(h);
        f = f->next;
        pf("destroying improperly closed file handle\r\n");
    }   
    *((file_handle**)FILE_HANDLE_ADDRESS) = 0;    
}



