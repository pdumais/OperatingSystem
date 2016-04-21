#include "virtio.h"
#include "macros.h"
#include "../memorymap.h"
#include "display.h"
#include "utils.h"
#include "netcard.h"
#include "includes/kernel/types.h"

#define RX_BUFFER_SIZE 8192 
#define FRAME_SIZE 1526 // including the net_header

extern unsigned int pci_getBar(unsigned int dev,unsigned char bar);
extern unsigned short pci_getIRQ(unsigned int dev);
extern void pci_enableBusMastering(unsigned int dev);
extern void registerIRQ(void* handler, unsigned long irq);
extern void setSoftIRQ(unsigned long);
extern unsigned short pci_getIOAPICIRQ(unsigned int dev);
extern char* kernelAllocPages(unsigned int pageCount);
extern void memclear64(char* destination, uint64_t size);
extern void memcpy64(char* source, char* dest, unsigned long size);


static struct virtio_device_info devInfoList[32]={0};


static void handler()
{
    u8 deviceIndex;
    u8 v;
    for (deviceIndex = 0; deviceIndex < 32; deviceIndex++)
    {
        struct virtio_device_info* dev = &devInfoList[deviceIndex];
        if (dev->iobase==0) continue;
        INPORTB(v,dev->iobase+0x13);
        if (v&1 == 1) setSoftIRQ(SOFTIRQ_NET);
    }
}    

unsigned long virtionet_getMACAddress(struct NetworkCard* netcard)
{
    struct virtio_device_info *dev = (struct virtio_device_info*)netcard->deviceInfo;
    return dev->macAddress;
}

void* initvirtionet(unsigned int deviceAddr)
{
    unsigned int templ;
    unsigned short tempw;
    unsigned long i;
    unsigned long tempq;

    struct virtio_device_info *devInfo=0;    
    for (i=0;i<32;i++)
    {
        if (devInfoList[i].iobase==0)
        {
            devInfo = &devInfoList[i];
            break;
        }
    }
    if (devInfo==0) return 0;

    devInfo->deviceAddress = deviceAddr;
    if (devInfo->deviceAddress == 0xFFFFFFFF)
    {
        pf("No network card found\r\n");
        return 0;
    }

    for (i=0;i<6;i++)
    {
        unsigned int m = pci_getBar(devInfo->deviceAddress,i);
        if (m==0) continue;
        if (m&1)
        {
            devInfo->iobase = m & 0xFFFC;
        }
        else
        {
            devInfo->memoryAddress = m & 0xFFFFFFF0;
        }
    }

    devInfo->irq = pci_getIOAPICIRQ(devInfo->deviceAddress);
    registerIRQ(&handler,devInfo->irq);
    pci_enableBusMastering(devInfo->deviceAddress);

    pf("VirtIO netcard found. IRQ=0x%x, IO=0x%x\r\n",devInfo->irq,devInfo->iobase);
 
    return devInfo;
}

void virtionet_negotiate(u32* features)
{
    // do not use control queue
    DISABLE_FEATURE(*features,VIRTIO_CTRL_VQ);

    // Disable tcp/udp packet size
    DISABLE_FEATURE(*features,VIRTIO_GUEST_TSO4);
    DISABLE_FEATURE(*features,VIRTIO_GUEST_TSO6);
    DISABLE_FEATURE(*features,VIRTIO_GUEST_UFO);
    DISABLE_FEATURE(*features,VIRTIO_EVENT_IDX);
    DISABLE_FEATURE(*features,VIRTIO_MRG_RXBUF);
   
    ENABLE_FEATURE(*features,VIRTIO_CSUM);
}

void virtionet_start(struct NetworkCard* netcard)
{
    int i;
    struct virtio_device_info* dev = (struct virtio_device_info*)netcard->deviceInfo;
    char *buffer;

    virtio_init(dev,&virtionet_negotiate);

    // check if both queues were found.
    if (dev->queues[0].baseAddress == 0 || dev->queues[1].baseAddress == 0)
    {
        C_BREAKPOINT() //TODO: handle this instead of #BP
    }

    buffer = (unsigned char*)kernelAllocPages(128);
    // Queues 0 is the rx queue. That's always the case for virtnet
    for (i = 0; i < dev->queues[0].queue_size; i++)
    {
        dev->queues[0].buffers[i].address = UNMIRROR((u64)&buffer[FRAME_SIZE*i]);
        dev->queues[0].buffers[i].length = FRAME_SIZE;
        dev->queues[0].buffers[i].flags = VIRTIO_DESC_FLAG_WRITE_ONLY;
    }
    
    // Tell the device that there are 16 bufferw ready to receive data
    virtio_set_next_receive_buffer_available(&dev->queues[0],16);

    // setup the send buffers
    buffer = kernelAllocPages(PAGE_COUNT(FRAME_SIZE*dev->queues[1].queue_size));
    for (i = 0; i < dev->queues[1].queue_size; i++)
    {
        dev->queues[1].buffers[i].address = UNMIRROR((u64)&buffer[FRAME_SIZE*i]);
        dev->queues[1].buffers[i].length = 0;
    }

    // Get MAC address
    unsigned char v=0x00;
    unsigned long tempq = 0;
    for (i = 0; i <6; i++)
    {
        INPORTB(v,dev->iobase+0x14+i);
        tempq = (tempq<<8)|v;
    }
    dev->macAddress = tempq;
}

unsigned long virtionet_receive(unsigned char** buffer, struct NetworkCard* netcard)
{
    struct virtio_device_info* dev = (struct virtio_device_info*)netcard->deviceInfo;
    virt_queue* vq = &dev->queues[0]; // RX queue
    
    if (vq->last_used_index == vq->used->index) return 0;
    
    u16 index = vq->last_used_index % vq->queue_size;
    u16 buffer_index = vq->used->rings[index].index;
    *buffer = MIRROR((unsigned char*)(vq->buffers[buffer_index].address+sizeof(net_header)));

    return vq->used->rings[index].length;
}

void virtionet_recvProcessed(struct NetworkCard* netcard)
{
    struct virtio_device_info* dev = (struct virtio_device_info*)netcard->deviceInfo;
    virt_queue* vq = &dev->queues[0]; // RX queue

    vq->last_used_index++;

    // Tell the device that there is one buffer ready to receive data
    virtio_set_next_receive_buffer_available(vq, 1);
}


unsigned long virtionet_send(struct NetworkBuffer *netbuf, struct NetworkCard* netcard)
{
    struct virtio_device_info* dev = (struct virtio_device_info*)netcard->deviceInfo;
 
    unsigned short size = netbuf->layer2Size+netbuf->layer3Size+netbuf->payloadSize;
    unsigned short virtio_size = size+sizeof(net_header);
    if (size>1792)
    {
        pf("can't send. Frame too big: l2:%x, l3: %x, payload size: %x\r\n",netbuf->layer2Size,netbuf->layer3Size,netbuf->payloadSize);
        return 0;
    }
    
    // We are not doing zero-copy networking. It would be nice to do it later. But keep it simple for now,
    send_buffer sb = virtio_get_send_buffer(&dev->queues[1],virtio_size);
    if (sb.address == 0)
    {
        pf("can't find empty virtio buffer");   
        return 0;
    }

    unsigned char *buf = sb.address;
    net_header* h = (net_header*)buf;
    //TODO: must reset whole header
    h->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    h->gso_type = 0;
    h->checksum_start = 0;   
    h->checksum_offset = size;   
    unsigned short i;
    unsigned char *buf2 = buf+sizeof(net_header); 
    memcpy64((char*)&netbuf->layer2Data[0],buf2,netbuf->layer2Size); buf2+=netbuf->layer2Size;
    memcpy64((char*)&netbuf->layer3Data[0],buf2,netbuf->layer3Size); buf2 += netbuf->layer3Size;
    memcpy64((char*)&netbuf->payload[0],buf2,netbuf->payloadSize); buf2 += netbuf->payloadSize;
    virtio_send_buffer_ready(dev, 1, sb.index);

    return size;
}




