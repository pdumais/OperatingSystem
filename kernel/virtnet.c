#include "virtio.h"
#include "../memorymap.h"
#include "display.h"
#include "utils.h"
#include "netcard.h"
#include "includes/kernel/types.h"

#define RX_BUFFER_SIZE 8192 
#define FRAME_SIZE 1526

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
}    

unsigned long virtionet_getMACAddress(struct NetworkCard* netcard)
{
    struct virtio_device_info *dev = (struct virtio_device_info*)netcard->deviceInfo;
    return dev->macAddress;
}

void* initvirtionet(unsigned int deviceAddr, char* buffer, unsigned int bufferSize)
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

    devInfo->rxBuffer = (unsigned char*)buffer;
    devInfo->rxBufferSize = bufferSize*2048;
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
    dev->readIndex = 0;
    dev->writeIndex = 0;
    dev->transmittedDescriptor = 0;
    dev->currentTXDescriptor = 0;

    virtio_init(dev,&virtionet_negotiate);

    // check if both queues were found.
    if (dev->queues[0].baseAddress == 0 || dev->queues[1].baseAddress == 0)
    {
        __asm("int $3"); //TODO: handle this instead of #BP
    }

    // check if the buffer passed by netcard is big enough
    if (dev->queues[0].queue_size*FRAME_SIZE > dev->rxBufferSize)
    {
        // buffer is currently hardcoded to 128*4096
        __asm("int $3"); //TODO: handle this instead of #BP
    }

    // Queues 0 is the rx queue. That's always the case for virtnet
    for (i = 0; i < dev->queues[0].queue_size; i++)
    {
        dev->queues[0].buffers[i].address = (u64)&dev->rxBuffer[FRAME_SIZE*i];
        dev->queues[0].buffers[i].length = FRAME_SIZE;
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

// checks if a frame is available and returns 0 if none are available
// If a frame is available, it is copied to the buffer (needs to be at least the size of an MTU)
// and returns the size of the data copied.
//WARNING: There is no locking mechanism here. Caller must make sure that this wont be executed by 2 threads
//   at the same time
unsigned long virtionet_receive(unsigned char** buffer, struct NetworkCard* netcard)
{
/*    struct RTL8139DeviceInfo* dev = (struct RTL8139DeviceInfo*)netcard->deviceInfo;
    if (dev->readIndex != dev->writeIndex)
    {
        unsigned short size;
        unsigned short i;
        unsigned char* p = (char*)&dev->rxBuffers[dev->readIndex*2048];
        size = p[2] | (p[3]<<8);
        if (!(p[0]&1))
        {
            pf("what should we do??\r\n");
            return 0; // PacketHeader.ROK
        }
        *buffer = (char*)&p[4]; // skip header
        return size;
    }
    else
    {
        return 0;
    }*/
}

void virtionet_recvProcessed(struct NetworkCard* netcard)
{
//    struct RTL8139DeviceInfo* dev = (struct RTL8139DeviceInfo*)netcard->deviceInfo;
//    dev->readIndex = (dev->readIndex+1) & (dev->rxBufferCount-1); // increment read index and wrap around 16
}


unsigned long virtionet_send(struct NetworkBuffer *netbuf, struct NetworkCard* netcard)
{
    struct virtio_device_info* dev = (struct virtio_device_info*)netcard->deviceInfo;

    net_header* h;
    
    h->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
    //TODO: set checksum_start to start of packet and checksum_offset to size

    unsigned short size = netbuf->layer2Size+netbuf->layer3Size+netbuf->payloadSize;
    if (size>1792)
    {
        pf("can't send. Frame too big: l2:%x, l3: %x, payload size: %x\r\n",netbuf->layer2Size,netbuf->layer3Size,netbuf->payloadSize);
        return 0;
    }
    
    // We are not doing zero-copy networking. It would be nice to do it later. But keep it simple for now,
    unsigned short i;
    unsigned char *buf = 0; //TODO: this should be the virtio buf Get buffer from queue
    unsigned char *buf2 = buf; 
    memcpy64((char*)&h,buf2,sizeof(net_header));  buf2+=sizeof(net_header);
    memcpy64((char*)&netbuf->layer2Data[0],buf2,netbuf->layer2Size); buf2+=netbuf->layer2Size;
    memcpy64((char*)&netbuf->layer3Data[0],buf2,netbuf->layer3Size); buf2 += netbuf->layer3Size;
    memcpy64((char*)&netbuf->payload[0],buf2,netbuf->payloadSize); buf2 += netbuf->payloadSize;

    //TODO: tell virtio that buffer is ready
    
    return size;
}

