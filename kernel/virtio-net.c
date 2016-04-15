#include "../memorymap.h"
#include "display.h"
#include "utils.h"
#include "netcard.h"
#include "includes/kernel/types.h"

// Documentation:
// http://ozlabs.org/~rusty/virtio-spec/virtio-0.9.5.pdf, appendix C
// http://docs.oasis-open.org/virtio/virtio/v1.0/cs04/virtio-v1.0-cs04.html

//Virtual I/O Device (VIRTIO) Version 1.0, Spec 4, section 5.1.3:  Feature bits
#define VIRTIO_CSUM                 0       
#define VIRTIO_GUEST_CSUM           1       
#define VIRTIO_CTRL_GUEST_OFFLOADS  2
#define VIRTIO_MAC                  5       
#define VIRTIO_GUEST_TSO4           7       
#define VIRTIO_GUEST_TSO6           8       
#define VIRTIO_GUEST_ECN            9       
#define VIRTIO_GUEST_UFO            10      
#define VIRTIO_HOST_TSO4            11      
#define VIRTIO_HOST_TSO6            12      
#define VIRTIO_HOST_ECN             13      
#define VIRTIO_HOST_UFO             14      
#define VIRTIO_MRG_RXBUF            15      
#define VIRTIO_STATUS               16      
#define VIRTIO_CTRL_VQ              17      
#define VIRTIO_CTRL_RX              18      
#define VIRTIO_CTRL_VLAN            19      
#define VIRTIO_CTRL_RX_EXTRA        20   
#define VIRTIO_GUEST_ANNOUNCE       21  
#define VIRTIO_MQ                   22      
#define VIRTIO_CTRL_MAC_ADDR        23
#define VIRTIO_EVENT_IDX            29

#define VIRTIO_ACKNOWLEDGE 1
#define VIRTIO_DRIVER 2
#define VIRTIO_FAILED 128
#define VIRTIO_FEATURES_OK 8
#define VIRTIO_DRIVER_OK 4
#define VIRTIO_DEVICE_NEEDS_RESET 64

#define VIRTIO_DESC_FLAG_NEXT           1 
#define VIRTIO_DESC_FLAG_WRITE_ONLY     2 
#define VIRTIO_DESC_FLAG_INDIRECT       4 

#define PAGE_COUNT(x) (((x+0xFFF)&0xFFF)>>12)
#define DISABLE_FEATURE(v,feature) v &= ~(1<<feature)
#define ENABLE_FEATURE(v,feature) v |= (1<<feature)
#define HAS_FEATURE(v,feature) (v & (1<<feature))

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

typedef struct
{
    u64 address;
    u32 length;
    u16 flags;
    u16 next;
} queue_buffer;

typedef struct
{
    u32 index;
    u32 length;
} used_ring;

typedef struct
{
    u16 queueSize;
    union
    {
        queue_buffer* buffers;
        void* baseAddress;
    };
    u16* available_flags;
    u16* available_index;
    u16* available_rings;
    u16* available_event_idx;
    u16* used_flags;
    u16* used_index;
    used_ring* used_rings;
    u16* used_event_idx;
} virt_queue;

struct virtio_device_info
{
    unsigned char readIndex;
    unsigned char writeIndex;
    unsigned char currentTXDescriptor;
    unsigned char transmittedDescriptor;
    unsigned int rxBufferSize;
    unsigned char rxBuffer;

    unsigned int deviceAddress;
    unsigned short iobase;
    unsigned long memoryAddress;
    unsigned short irq;
    unsigned long macAddress;
    
    u16 rxQueueSize;
    u16 txQueueSize;
    virt_queue* rxQueue;
    virt_queue* txQueue;
};

static struct virtio_device_info devInfoList[32]={0};



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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

    devInfo->rxQueue = 0;
    devInfo->txQueue = 0;
    devInfo->rxBuffer = buffer;
    devInfo->rxBufferSize = bufferSize*2048;
    devInfo->irq = pci_getIOAPICIRQ(devInfo->deviceAddress);
    registerIRQ(&handler,devInfo->irq);
    pci_enableBusMastering(devInfo->deviceAddress);

    pf("VirtIO netcard found. IRQ=0x%x, IO=0x%x\r\n",devInfo->irq,devInfo->iobase);
 
    return devInfo;
}


static bool setupvirt_queues(struct virtio_device_info* dev, unsigned char index, virt_queue* vq, u16* qs)
{
    unsigned short c;
    unsigned short queueSize; 
    unsigned int i;

    OUTPORTW(index,dev->iobase+0x0E);
    INPORTW(queueSize,dev->iobase+0x0C);
    if (queueSize == 0) return false;

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

    *qs = queueSize;
    return true;
}

void virtionet_start(struct NetworkCard* netcard)
{
    unsigned char c,v;
    unsigned int i;
    struct virtio_device_info* dev = (struct virtio_device_info*)netcard->deviceInfo;
    dev->readIndex = 0;
    dev->writeIndex = 0;
    dev->transmittedDescriptor = 0;
    dev->currentTXDescriptor = 0;

    //Virtual I/O Device (VIRTIO) Version 1.0, Spec 4, section 3.1.1:  Device Initialization
    c = VIRTIO_ACKNOWLEDGE;
    OUTPORTB(c,dev->iobase+0x12);
    c |= VIRTIO_DRIVER;
    OUTPORTB(c,dev->iobase+0x12);
    INPORTL(i,dev->iobase+0x00); // read features offered by device

    //TODO: negotiate checksum
    //TODO: what about RX_BUF?

    // do not use control queue
    DISABLE_FEATURE(i,VIRTIO_CTRL_VQ);

    // Disable tcp/udp packet size
    DISABLE_FEATURE(i,VIRTIO_GUEST_TSO4);
    DISABLE_FEATURE(i,VIRTIO_GUEST_TSO6);
    DISABLE_FEATURE(i,VIRTIO_GUEST_UFO);
    DISABLE_FEATURE(i,VIRTIO_EVENT_IDX);
    DISABLE_FEATURE(i,VIRTIO_MRG_RXBUF);
   
    ENABLE_FEATURE(i,VIRTIO_CSUM);

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
    dev->rxQueue = 0;
    for (i = 0; i < 100; i+=2)
    {      
        if(setupvirt_queues(dev,i,dev->rxQueue,&dev->rxQueueSize))
        {
            setupvirt_queues(dev,i+1, dev->txQueue,&dev->txQueueSize);
            break;
        }
    }
    if (dev->rxQueue->baseAddress == 0)
    {
        __asm("int $3");
    }
    if (dev->rxQueueSize*FRAME_SIZE > dev->rxBufferSize)
    {
        // buffer is currently hardcoded to 128*4096
        __asm("int $3");
    }

    char* rxbuf = 0; //TODO: set that to what netcard.c gave us.
    for (i = 0; i < dev->rxQueueSize; i++)
    {
        dev->rxQueue->buffers[i].address = (u64)&rxbuf[FRAME_SIZE*i];
        dev->rxQueue->buffers[i].length = FRAME_SIZE;
    }

    // TODO: enable receive interrupts
    // TODO: register IRQ and test if we do receive something


    c |= VIRTIO_DRIVER_OK;
    OUTPORTB(c,dev->iobase+0x12);

    // Get MAC address
    v=0x00;
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
    //TODO: be able to transmit

/*    unsigned short size = netbuf->layer2Size+netbuf->layer3Size+netbuf->payloadSize;
    if (size>1792)
    {
        pf("can't send. Frame too big: l2:%x, l3: %x, payload size: %x\r\n",netbuf->layer2Size,netbuf->layer3Size,netbuf->payloadSize);
        return 0;
    }
    unsigned short tsd = 0x10 + (dev->currentTXDescriptor*4);
    unsigned short tsdValue;
    INPORTW(tsdValue,dev->iobase+tsd);
    
    // it is also important to check the TOK bit. If we try to send something when
    if (tsdValue & 0x2000 == 0)
    {
        //the whole queue is pending packet sending
        return 0;
    }
    else
    {
        unsigned short i;
        unsigned char *buf = dev->txbuf[dev->currentTXDescriptor];
        memcpy64((char*)&netbuf->layer2Data[0],(char*)&buf[0],netbuf->layer2Size);
        memcpy64((char*)&netbuf->layer3Data[0],(char*)&buf[netbuf->layer2Size],netbuf->layer3Size);
        memcpy64((char*)&netbuf->payload[0],(char*)&buf[netbuf->layer2Size+netbuf->layer3Size],netbuf->payloadSize);

        tsdValue = size;
        OUTPORTL(tsdValue,dev->iobase+tsd);
        dev->currentTXDescriptor = (dev->currentTXDescriptor+1)&0b11; // wrap around 4
        return size;
    }*/
}

