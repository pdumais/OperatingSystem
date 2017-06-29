#include "flatfs.h"
#include "block_cache.h"
#include "printf.h"
#include "utils.h"

/*
This file system driver implements a flat fs following the "tar" file format.

*/

extern void memcpy64(char* source, char* destination, uint64_t size);
extern uint64_t block_get_size(uint32_t device);

typedef struct
{
    char name[100];
    char mode[8];
    char owner[8];
    char group[8];
    char size[12];
    char last_modification[12];
    char checksum[8];
    char link_indicator;
    char link_name[100];
    char pad[255];
} tar_header;


void flatfs_system_handle_destructor(system_handle* h);

bool strcompare(char* src, char* dst)
{
    if (*src == 0 || *dst == 0) return false;
    while(*src == *dst && *src != 0)
    {
        src++;
        dst++;
    }
    return (*src == 0 && *dst == 0);
}

    // converts from ascii octal to bin number
uint64_t ascii2number(char* c, uint8_t size)
{
    uint8_t byte;
    uint64_t ret = 0;
    uint64_t factor = 1;
    size-=2;
    char *buf = c+size;
    while (size)
    {
        byte = (*buf)-0x30;
        ret += ((uint64_t)byte)*factor; 
        buf--;
        factor <<= 3;
        size--;
    }
    return ret;
}

bool flatfs_fopen(system_handle* h, char* name, uint64_t access_type)
{
    unsigned char n1,n2,device;
    uint64_t n,i;
    file_handle* f = (file_handle*)h;
    h->destructor = &flatfs_system_handle_destructor;

    n1 = name[0];
    n2 = name[1];
    device = ((n1-0x30)<<4) | (n2-0x30);
    name += 4; // skip the xx:/ part of path
    
    tar_header header;
    int sector = 0;

    f->position = 0;
    f->start = -1;
    f->size = 0;
    f->device = device;

    uint64_t disk_size = block_get_size(device);
    uint64_t fsize;

    while ((sector) < disk_size)
    {
        block_cache_read(sector,device,&header,1);
        char* fname = &header.name[2]; // skip the "./" in the tar filename
        fsize = ascii2number(header.size,12);
        if (strcompare(fname,name))
        {
            f->start = sector+1;
            f->size = fsize;
            return true;
        }

        sector += (((fsize+511)&0x1FF)>>9)+1;
    }
    return false;
}

uint64_t flatfs_fread(system_handle* h, uint64_t count, char* destination)
{
    char buf[512];
    file_handle* f = (file_handle*)h;

    if ((f->position + count) > f->size) count = (f->size-f->position); 

    uint64_t first_sector = f->start + (f->position>>9);
    uint64_t start_index= f->position&0x1ff;
    uint64_t bytes_read = 0;

    if (start_index > 0)
    {
        block_cache_read(first_sector,f->device,buf,1);
        memcpy64((char*)&buf[start_index],destination,512-start_index);
        bytes_read = (512-start_index);
        destination += bytes_read;
        f->position += bytes_read;
        first_sector++;
    }

    if ((count-bytes_read)>=512)
    {
        uint64_t n = (count-bytes_read)>>9;
        block_cache_read(first_sector,f->device,destination,n);
        first_sector+=n;
        bytes_read += n<<9;
        destination += bytes_read;
        f->position += bytes_read;
    }

    if (count!=bytes_read)
    {
        if (((count-bytes_read)<512))
        {
            block_cache_read(first_sector,f->device,buf,1);
            memcpy64(buf,destination,count-bytes_read);
            bytes_read = count;
        }
        else
        {
            __asm("mov $0xDEADBEEF,%rax; int $3");
        }
    }

    return bytes_read;
}
uint64_t flatfs_fwrite(system_handle* h, uint64_t count, char* destination)
{
}

void flatfs_fclose(system_handle* h)
{
}

void flatfs_fseek(system_handle* h, uint64_t count, bool absolute)
{
}

uint64_t flatfs_fgetsize(system_handle* h)
{
    file_handle* f = (file_handle*)h;
    return f->size;
}
void flatfs_system_handle_destructor(system_handle* h)
{
    //TODO: should release any held locks
    //TODO: should cancel any pending block operations
}
