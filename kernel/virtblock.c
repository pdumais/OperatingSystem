#include "includes/kernel/types.h"
#include "virtio.h"
#include "macros.h"
#include "utils.h"
#include "printf.h"

#define VIRTIO_BLK_F_SIZE_MAX 1
#define VIRTIO_BLK_F_SEG_MAX 2
#define VIRTIO_BLK_F_GEOMETRY 4
#define VIRTIO_BLK_F_RO 5
#define VIRTIO_BLK_F_BLK_SIZE 6
#define VIRTIO_BLK_F_FLUSH 9
#define VIRTIO_BLK_F_TOPOLOGY 10
#define VIRTIO_BLK_F_CONFIG_WCE 11
#define VIRTIO_BLK_T_IN           0 
#define VIRTIO_BLK_T_OUT          1 
#define VIRTIO_BLK_T_FLUSH        4
#define VIRTIO_BLK_S_OK        0 
#define VIRTIO_BLK_S_IOERR     1 
#define VIRTIO_BLK_S_UNSUPP    2

typedef void (*virtioirqcallback)(unsigned char, unsigned long, unsigned long);
typedef void (*virtioreadycallback)(unsigned char);

extern void pci_enableBusMastering(unsigned int dev);
extern void registerIRQ(void* handler, unsigned long irq);
extern void setSoftIRQ(unsigned long);
extern unsigned short pci_getIOAPICIRQ(unsigned int dev);
extern char* kernelAllocPages(unsigned int pageCount);
extern void memclear64(char* destination, uint64_t size);
extern void memcpy64(char* source, char* dest, unsigned long size);


struct operation
{
    unsigned char dev;
    volatile unsigned char pending;
    unsigned long block;
    unsigned long count;
};

typedef struct
{
    struct virtio_device_info virtio;
    u64 block_count; // we assume 512bytes sectors
    //struct operation pending_request;
    //unsigned long long devices_lock;
} virtio_block_device;

typedef struct 
{ 
    u32 type; 
    u32 reserved; 
    u64 sector; 
} block_request_header;

static virtio_block_device block_devices[32] = {0};
static virtioirqcallback irq_callback;
static virtioreadycallback ready_callback;

static void process_block_device_interrupt(virtio_block_device* dev, u32 dev_index)
{

    virt_queue* vq = &dev->virtio.queues[0];

    while (vq->last_used_index != vq->used->index)
    {
        u16 index = vq->last_used_index % vq->queue_size;
        u16 buffer_index1 = vq->used->rings[index].index;
        u16 buffer_index2 = vq->buffers[buffer_index1].next;
        u16 buffer_index3 = vq->buffers[buffer_index2].next;
        char *status = MIRROR((u64)(vq->buffers[buffer_index3].address));
        block_request_header* h = MIRROR((u64)(vq->buffers[buffer_index1].address));
        if (*status != VIRTIO_BLK_S_OK)
        {
            C_BREAKPOINT_VAR(0xDEADBEEF,0x11111111,0x22222222,0x33333333)
        }

        irq_callback(dev_index,h->sector,1);
        ready_callback(dev_index);
        
        vq->last_used_index++;
    }
//C_BREAKPOINT_VAR(vq->last_used_index,vq->used->index,index,3)

}

static void handler()
{

    u8 deviceIndex;
    u8 v;
    // for each device that has data ready, ack the ISR
    for (deviceIndex = 0; deviceIndex < 32; deviceIndex++)
    {
        struct virtio_device_info* dev = &block_devices[deviceIndex];
        if (dev->iobase==0) continue;

        INPORTB(v,dev->iobase+0x13);
        if (v&1 == 1) process_block_device_interrupt(&block_devices[deviceIndex], deviceIndex);
    }
}

static void negotiate(u32* features)
{
    DISABLE_FEATURE(*features,VIRTIO_BLK_F_RO);
    DISABLE_FEATURE(*features,VIRTIO_BLK_F_BLK_SIZE);
    DISABLE_FEATURE(*features,VIRTIO_BLK_F_TOPOLOGY);
}

u64 virtioblock_add_device(u32 addr, u32 iobase, u64* hw_index)
{
    u32 i;
    u32 irq = pci_getIOAPICIRQ(addr);

    // Find empty device slot
    int dev = -1;
    for (i = 0; i <32; i++) if (block_devices[i].block_count == 0)
    {
        dev = i;
        break;
    }
    if (dev == -1) 
    {
        pf("Maximum number of virtio block devices reached");
        C_BREAKPOINT();
        return 0;
    }

    *hw_index = dev;

    struct virtio_device_info* devInfo = &block_devices[dev];
    devInfo->iobase = iobase;
    devInfo->deviceAddress = addr;
    devInfo->irq = irq; 

    registerIRQ(&handler,devInfo->irq);
    pci_enableBusMastering(devInfo->deviceAddress);

    // get sector count
    u32 tmp;
    INPORTL(tmp,devInfo->iobase+0x18);
    block_devices[dev].block_count = ((u64)tmp)<<32;
    INPORTL(tmp,devInfo->iobase+0x14);
    block_devices[dev].block_count |= ((u64)tmp);
   
    // virtio initialization 
    virtio_init(devInfo,&negotiate);
    virt_queue* vq = &devInfo->queues[0];

    // check if both queues were found.
    if (vq->baseAddress == 0)
    {
        C_BREAKPOINT() //TODO: handle this instead of #BP
    }

    // alloc meme for buf1 (request header) and buffer 3 (status byte). buffer2 will be user-supplied.
    u32 bufferSize = vq->queue_size*(sizeof(block_request_header)+1);
    vq->buffer = kernelAllocPages(PAGE_COUNT(bufferSize));

    return 1;
}

bool virtblock_pci_device_matches(u16 vendor, u16 device, u16 subsystem)
{
    return (vendor == 0x1AF4 && (device >= 0x1000 && device <= 0x103F) && subsystem == 2);
}

void init_virtioblock(virtioirqcallback irqcallback, virtioreadycallback readycallback)
{
    irq_callback = irqcallback;
    ready_callback = readycallback;
}

u64 virtblock_get_size(u64 dev)
{
    virtio_block_device* bdev = &block_devices[dev];
    return bdev->block_count;
}

/*
Buffer structure:
3 linked buffers for all req
    buf1: (read-only)
        u32 type
        u32 priority
        u64 sector 
    buf2: user supplied 512bytes buffer (read/or write)
    buf3: status byte (write-only)

Multiple commands will be issued if multiple sectors need to be read

unlike the ATA driver, it is possible to reqest several blocks
without waiting for the previous result. The ATA driver has to wait
current request completion before issuing a new request. The block cache
will only query one sector at a time though. But this driver is ready 
for future modifications of the block cache. We can only put 1 sector request
in at a time any way in here. But we could add several request in the queue
*/
int virtioblock_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{

    virtio_block_device* bdev = &block_devices[dev];
    virt_queue* vq = &bdev->virtio.queues[0];

    if (count != 1)
    {
        //TODO: the block cache currently only reads 1 sector at a time.
        //      What what would happen if we got a request to read more sectors
        //      than what the virtqueue can hold? We should not block in here. 
        //      Since block_cache surrently reads 1 block at a time, we will keep it
        //      simple in here for now and just read one block all the time
        C_BREAKPOINT();
    }

    buffer_info bi[3];
    char c;

    block_request_header h;
    h.type = VIRTIO_BLK_T_IN;
    h.reserved = 0;
    h.sector = sector;
    bi[0].buffer = &h;
    bi[0].size = sizeof(block_request_header);
    bi[0].flags = 0;
    bi[0].copy=true;

    //TODO: Passing the buffer addr will work because this buffer belongs to the
    //      block cache. The block cache is in kernel memory, which is identity mapped.
    //      so we don't need to convert to physical addr. But we should make this more robust
    //      because it won't work anymore if the buffer address gets a different phys
    //      mapping. This could happen any of the following case occur
    //          - kernel mem moves to other physical addr
    //          - block cache would keep buffers in virtual mem
    //          - this function would be supplied a buffer that does not belong
    //            to block cache.
    bi[1].buffer = buffer; 
    bi[1].size = 512;
    bi[1].flags = VIRTIO_DESC_FLAG_WRITE_ONLY;
    bi[1].copy=false;
    bi[2].buffer = &c;
    bi[2].size = 1;
    bi[2].flags = VIRTIO_DESC_FLAG_WRITE_ONLY;
    bi[2].copy=true;

    virtio_send_buffer(&bdev->virtio, 0, bi, 3);

    return 1;
}

int virtioblock_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{
    virtio_block_device* bdev = &block_devices[dev];
    virt_queue* vq = &bdev->virtio.queues[0];

    if (count != 1)
    {
        //TODO: the block cache currently only reads 1 sector at a time.
        //      What what would happen if we got a request to read more sectors
        //      than what the virtqueue can hold? We should not block in here. 
        //      Since block_cache surrently reads 1 block at a time, we will keep it
        //      simple in here for now and just read one block all the time
        C_BREAKPOINT();
    }

    buffer_info bi[3];
    char c;

    block_request_header h;
    h.type = VIRTIO_BLK_T_OUT;
    h.reserved = 0;
    h.sector = sector;
    bi[0].buffer = &h;
    bi[0].size = sizeof(block_request_header);
    bi[0].flags = 0;
    bi[0].copy=true;

    //TODO: Passing the buffer addr will work because this buffer belongs to the
    //      block cache. The block cache is in kernel memory, which is identity mapped.
    //      so we don't need to convert to physical addr. But we should make this more robust
    //      because it won't work anymore if the buffer address gets a different phys
    //      mapping. This could happen any of the following case occur
    //          - kernel mem moves to other physical addr
    //          - block cache would keep buffers in virtual mem
    //          - this function would be supplied a buffer that does not belong
    //            to block cache.
    bi[1].buffer = buffer; 
    bi[1].size = 512;
    bi[1].flags = 0;
    bi[1].copy=false;
    bi[2].buffer = &c;
    bi[2].size = 1;
    bi[2].flags = VIRTIO_DESC_FLAG_WRITE_ONLY;
    bi[2].copy=true;

    virtio_send_buffer(&bdev->virtio, 0, bi, 3);

    return 1;
}
