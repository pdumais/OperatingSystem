#include "virtio.h"
#include "../memorymap.h"
#include "display.h"
#include "utils.h"

extern char* kernelAllocPages(unsigned int pageCount);
extern void memclear64(char* destination, uint64_t size);
extern void memcpy64(char* source, char* dest, unsigned long size);


bool virtio_queue_setup(struct virtio_device_info* dev, unsigned char index)
{
    unsigned short c;
    unsigned short queueSize; 
    unsigned int i;

    virt_queue* vq = &dev->queues[index];

    // get queue size
    OUTPORTW(index,dev->iobase+0x0E);
    INPORTW(queueSize,dev->iobase+0x0C);
    vq->queue_size = queueSize;
    if (queueSize == 0) return false;

    // create virtqueue memory
    u32 sizeofBuffers = (sizeof(queue_buffer) * queueSize);
    u32 sizeofQueueAvailable = (2+queueSize)*sizeof(u16); 
    u32 sizeofQueueUsed = (2*sizeof(u16))+(queueSize*2*sizeof(u32));
    u32 queuePageCount = PAGE_COUNT(sizeofBuffers + sizeofQueueAvailable) + PAGE_COUNT(sizeofQueueUsed);
    char* buf = kernelAllocPages(queuePageCount);
    memclear64(buf,queuePageCount<<12);
    u32 bufPage = ((u32)buf)>>12;
    OUTPORTL(bufPage,dev->iobase+0x08);

    vq->baseAddress = (void*)buf;
    vq->available_flags = (u16*)&buf[sizeofBuffers];
    vq->available_index = vq->available_flags+sizeof(u16);
    vq->available_rings = vq->available_index+sizeof(u16);
    vq->available_event_idx = vq->available_rings+(queueSize*sizeof(u16));
    vq->used_flags = (u16*)&buf[PAGE_COUNT(sizeofBuffers + sizeofQueueAvailable)<<12];
    vq->used_index = (u16*)vq->used_flags+sizeof(u16);
    vq->used_rings = (used_ring*)vq->used_index+sizeof(u16);
    vq->used_event_idx = (u16*)vq->used_rings + (queueSize*sizeof(used_ring));

    return true;
}

bool virtio_init(struct virtio_device_info* dev, void (*negotiate)(u32* features))
{
    unsigned char c,v;
    unsigned int i;

    //Virtual I/O Device (VIRTIO) Version 1.0, Spec 4, section 3.1.1:  Device Initialization
    c = VIRTIO_ACKNOWLEDGE;
    OUTPORTB(c,dev->iobase+0x12);
    c |= VIRTIO_DRIVER;
    OUTPORTB(c,dev->iobase+0x12);

    INPORTL(i,dev->iobase+0x00); // read features offered by device
    negotiate(&i);
    OUTPORTL(i,dev->iobase+0x04); 

    c |= VIRTIO_FEATURES_OK;
    OUTPORTB(c,dev->iobase+0x12);
    INPORTB(v,dev->iobase+0x12);
    if (v&VIRTIO_FEATURES_OK == 0)
    {
        //TODO: should set to driver_failed
        pf("Feature set not accepted\r\n");
        return;
    }

    // Setup virt queues
    for (i = 0; i < 16; i++) virtio_queue_setup(dev,i);

    c |= VIRTIO_DRIVER_OK;
    OUTPORTB(c,dev->iobase+0x12);
}

bool virtio_send_buffer(virt_queue* vq, u8* data, u32 size)
{
    return false;
}
