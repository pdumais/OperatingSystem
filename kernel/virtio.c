#include "virtio.h"
#include "macros.h"
#include "../memorymap.h"
#include "display.h"
#include "utils.h"

extern char* kernelAllocPages(unsigned int pageCount);
extern void memclear64(char* destination, uint64_t size);
extern void memcpy64(char* source, char* dest, unsigned long size);
extern void spinLock(u64* lock);
extern void spinUnlock(u64* lock);

bool virtio_queue_setup(struct virtio_device_info* dev, unsigned char index)
{
    unsigned short c;
    unsigned short queueSize; 
    unsigned int i;

    virt_queue* vq = &dev->queues[index];
    memclear64(vq,sizeof(virt_queue));

    // get queue size
    OUTPORTW(index,dev->iobase+0x0E);
    INPORTW(queueSize,dev->iobase+0x0C);
    vq->queue_size = queueSize;
    if (queueSize == 0) return false;

    // create virtqueue memory
    u32 sizeofBuffers = (sizeof(queue_buffer) * queueSize);
    u32 sizeofQueueAvailable = (2*sizeof(u16)) + (queueSize*sizeof(u16)); 
    u32 sizeofQueueUsed = (2*sizeof(u16))+(queueSize*sizeof(virtio_used_item));
    u32 queuePageCount = PAGE_COUNT(sizeofBuffers + sizeofQueueAvailable) + PAGE_COUNT(sizeofQueueUsed);
    char* buf = kernelAllocPages(queuePageCount);
    memclear64(buf,queuePageCount<<12);
    u32 bufPage = ((u64)UNMIRROR(buf))>>12;

    vq->baseAddress = (u64)buf;
    vq->available = (virtio_available*)&buf[sizeofBuffers];
    vq->used = (virtio_used*)&buf[((sizeofBuffers + sizeofQueueAvailable+0xFFF)&~0xFFF)];
    vq->next_buffer = 0;
    vq->lock = 0;

    OUTPORTL(bufPage,dev->iobase+0x08);

    vq->available->flags = 0;
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

void virtio_clean_used_buffers(struct virtio_device_info* dev, u16 queue_index)
{
    virt_queue* vq = &dev->queues[queue_index];

    if (vq->last_used_index == vq->used->index) return; 

    //We have nothing to clean really...
    u16 index = vq->last_used_index;
    u16 normalized_index;
    u16 buffer_index;
    while (index != vq->used->index)
    {
        normalized_index = index % vq->queue_size;
        buffer_index = vq->used->rings[normalized_index].index;
        index++;
    }
    vq->last_used_index = index;
}

void virtio_send_buffer(struct virtio_device_info* dev, u16 queue_index, buffer_info b[], u64 count)
{
    u64 i;

    virt_queue* vq = &dev->queues[queue_index];
    spinLock(&vq->lock);

    u16 index = vq->available->index % vq->queue_size;
    u16 buffer_index = vq->next_buffer;
    u16 next_buffer_index;

    unsigned char *buf = (u8*)(&vq->buffer[vq->chunk_size*buffer_index]);
    unsigned char *buf2 = buf;

    vq->available->rings[index] = buffer_index;
    for (i = 0; i < count; i++)
    {
        next_buffer_index = (buffer_index+1) % vq->queue_size;

        buffer_info* bi = &b[i];
        vq->buffers[buffer_index].flags = bi->flags;
        if (i != (count-1)) vq->buffers[buffer_index].flags |= VIRTIO_DESC_FLAG_NEXT;

        vq->buffers[buffer_index].next = next_buffer_index;
        vq->buffers[buffer_index].length = bi->size;
        if (bi->copy)
        {
            vq->buffers[buffer_index].address = UNMIRROR(buf2);
            if (bi->buffer != 0) memcpy64(bi->buffer,buf2,bi->size);
            buf2+=bi->size;
        }
        else
        {
            // calling function wants to keep same buffer
            vq->buffers[buffer_index].address = bi->buffer;
        }
        buffer_index = next_buffer_index;
    }

    vq->next_buffer = buffer_index;

    vq->available->index++;
    OUTPORTW(queue_index, dev->iobase+0x10);

    // now, we will clear previously used buffers, it any. We do this here instead of in the interrupt
    // context. It adds latency to the calling thread instead of adding latency to any random thread
    // where the interrupt would be called from.
    virtio_clean_used_buffers(dev, queue_index);
    spinUnlock(&vq->lock);
}
