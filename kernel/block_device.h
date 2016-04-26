
#define BLOCK_DEVICE_TYPE_ATA 1
#define BLOCK_DEVICE_TYPE_VIRTIO 2

typedef struct
{
    int (*read)(unsigned int,unsigned long, char*, unsigned long);
    int (*write)(unsigned int,unsigned long, char*, unsigned long);
    unsigned char (*isBusy)(unsigned char);

    unsigned int hw_device_number;
    unsigned char type;
} block_device;

typedef void (*blockirqcallback)(unsigned char, unsigned long, unsigned long);
typedef void (*blockreadycallback)(unsigned char);

void init_block(blockirqcallback irq_callback, blockreadycallback ready_callback);
int block_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
int block_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
