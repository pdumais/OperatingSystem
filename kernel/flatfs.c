#include "flatfs.h"


void flatfs_system_handle_destructor(system_handle* h);

bool flatfs_fopen(system_handle* h, char* name, uint64_t access_type)
{
    unsigned char n1,n2,device;
    uint64_t n,i;
    file_handle* f = (file_handle*)h;
    h->destructor = &flatfs_system_handle_destructor;

    char index[5120];
    n1 = name[0];
    n2 = name[1];
    device = ((n1-0x30)<<4) || (n2-0x30);
    name += 4; // skip the xx:/ part of path
    
    block_cache_read(0,device,index,10);
  
    f->position = 0;
    f->start = -1;
    f->size = 0;
    f->device = device;
    char* lbuf = (uint64_t*)&index[0];
    for (i = 0; i < 100; i++)       // index can't contain more than 100 entries    
    {
        for (n=0;n<32;n++)
        {
            if (lbuf[n]==0x20 && name[n]==0)
            {
                f->start = ((*((uint64_t*)&lbuf[32]))>>9);
                f->size = *((uint64_t*)&lbuf[40]);
                break;
            }
            if (lbuf[n] != name[n]) break;

        }

        lbuf += (32+8+8);         // each index entries are 32+8+8 long
    }

    if (f->start == -1 || f->size ==-1) return false;

    return true;
}

uint64_t flatfs_fread(system_handle* h, uint64_t count, char* destination)
{
    char buf[512];
    file_handle* f = (file_handle*)h;

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

//pf("woof %x  %x  %x\r\n",first_sector,count,bytes_read);
//while (1);
    if (count!=bytes_read)
    {
        if (((count-bytes_read)<512))
        {
            block_cache_read(first_sector,f->device,buf,1);
            memcpy64(buf,destination,count-bytes_read);
        }
        else
        {
            __asm("mov $0xDEADBEEF,%rax; int $3");
        }
    }
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
