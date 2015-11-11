
#define BLOCK_SIZE 512

#define CACHE_WRITE_PENDING 1
#define CACHE_READ_PENDING 2
#define CACHE_BLOCK_VALID 4
#define CACHE_IN_USE 8
#define CACHE_FILL_PENDING 16

struct block_cache_entry
{
    unsigned long block;
    char *data;
    unsigned char device;
    volatile unsigned char flags;
    unsigned long lastAccess;
} __attribute((packed))__;


void block_cache_init();
int block_cache_read(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks);
int block_cache_write(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks);
