#include <time.h>
#include <stdio.h>

typedef void (*atairqcallback)(unsigned char, unsigned long, unsigned long);

extern int block_cache_write(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks);
extern void clearCacheEntries(unsigned int count);
extern void block_cache_init(char* buf);
extern int block_cache_read(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks);


unsigned char isbusy;
atairqcallback irqhandler;

int main(int argc, char* argv)
{
    char* buf = malloc(512*128);
    char* buf2 = malloc(512*512);

    isbusy=0;

    block_cache_init(buf);

    block_cache_write(0, 0, buf2, 130);
//    irqhandler(0,0,1);
//    isbusy = 0;
//    block_cache_read(0, 0, buf2, 1);

}


// stubs
void yield()
{
}

void init_ata(atairqcallback* handler)
{
    irqhandler = handler;
}

int ata_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{
    isbusy = 1;
    irqhandler(dev,sector,count);
    isbusy = 0;
}

int ata_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{
    isbusy = 1;
}

unsigned char ata_isBusy(unsigned char dev)
{
    return isbusy;
}

void memcpy64(char* source, char* dest, unsigned long size)
{
}

unsigned long getTicksSinceBoot()
{
    time_t t;
    time(&t);
    return t;
}

void pf(char * format,...)
{
    printf("%s",format);
}

