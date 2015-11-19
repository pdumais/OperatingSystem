#include "../memorymap.h"
#include "display.h"
#include "utils.h"
#include "netcard.h"

#define RX_BUFFER_SIZE 8192 

extern unsigned int pci_getBar(unsigned int dev,unsigned char bar);
extern unsigned short pci_getIRQ(unsigned int dev);
extern void pci_enableBusMastering(unsigned int dev);
extern void registerIRQ(void* handler, unsigned long irq);
extern void setSoftIRQ(unsigned long);


/*
http://linuxgazette.net/156/jangir.html


WARNING: This driver has some limitations:
    1) we are assuming that only one nic can exist
    2) we assume that 1 IO base and 1 memory area exists (but that might be ok)
    3) we assume that the BIOS has configured the IRQ and memory address for that device

*/

struct RTL8139DeviceInfo
{
    unsigned char readIndex;
    unsigned char writeIndex;
    unsigned char currentTXDescriptor;
    unsigned char transmittedDescriptor;

    unsigned int deviceAddress;
    unsigned short iobase;
    unsigned long memoryAddress;
    unsigned short irq;
    unsigned long macAddress;

    unsigned long rxBufferCount;
    unsigned char* rxBuffers;
    unsigned char txbuf[4][2048];
    unsigned char rxbuf[RX_BUFFER_SIZE+2048];
    unsigned short rxBufferIndex;
};

struct RTL8139DeviceInfo devInfoList[32]={0};

extern void memcpy64(char* source, char* dest, unsigned long size);

void handler()
{
    unsigned char deviceIndex;
    for (deviceIndex = 0; deviceIndex < 32; deviceIndex++)
    {
        struct RTL8139DeviceInfo* dev = &devInfoList[deviceIndex];
        if (dev->iobase==0) continue;
        unsigned short isr;
        INPORTW(isr,dev->iobase+0x3E);
        OUTPORTW(0xFFFF,dev->iobase + 0x3E);
        unsigned int status;
        unsigned char  cmd=0;
        unsigned short size;
        unsigned short i;

        if (isr&1)                  // ROK
        {
            // It is possible that we get here and CMD.BUFE is still set. So check it right now
            INPORTB(cmd,dev->iobase+0x37);
            while ((cmd&1) == 0)   // check if CMD.BUFE == 1
            {
                // if last frame overflowed buffer, this won't will start at rxBufferIndex%RX_BUFFER_SIZE instead of zero
                if (dev->rxBufferIndex>=RX_BUFFER_SIZE) dev->rxBufferIndex = (dev->rxBufferIndex%RX_BUFFER_SIZE);
    
                status =*(unsigned int*)(dev->rxbuf+dev->rxBufferIndex);
                size = status>>16;
    
                memcpy64((char*)&dev->rxbuf[dev->rxBufferIndex],(char*)&dev->rxBuffers[dev->writeIndex*2048],size);
    
                dev->rxBufferIndex = ((dev->rxBufferIndex+size+4+3)&0xFFFC);
    
    
                // This is ridiculous, you need to substract 16 from CAPR. This is not mentionned anywhere. I had to look in
                // some other driver's source code to find out. I don't understand where that comes from
                OUTPORTW(dev->rxBufferIndex-16,dev->iobase+0x38);
    
                dev->writeIndex = (dev->writeIndex+1)&(dev->rxBufferCount-1);
                if (dev->writeIndex==dev->readIndex)
                {
                    pf("There is a buffer overrun in RTL8139 rcv buffer. Receive thread could read corrupted data\r\n");
                    while(1);   
                    // Buffer overrun
                }
    
                INPORTB(cmd,dev->iobase+0x37);
            }
            setSoftIRQ(SOFTIRQ_NET);
        }
        if (isr&0b100)              //TOK
        {
            unsigned long tsdCount = 0;
            unsigned int tsdValue;
            while (tsdCount <4)
            {
                unsigned short tsd = 0x10 + (dev->transmittedDescriptor*4);
                dev->transmittedDescriptor = (dev->transmittedDescriptor+1)&0b11;
                INPORTL(tsdValue,dev->iobase+tsd);
                if (tsd&0x2000) // OWN is set, so it means that the data was transmitted to FIFO
                {
                    if ((tsd&0x8000)==0)
                    {
                        //TOK is false, so the packet transmission was bad. Ignore that for now. We will drop it.
                    }
                }
                else
                {
                    // this frame is pending transmission, we will get another interrupt.
                    break;
                }
                //OUTPORTL(0x2000,iobase+tsd); // set lenght to zero to clear the other flags but leave OWN to 1
                tsdCount++;
            }
        }
    }
}    


unsigned long rtl8139_getMACAddress(struct NetworkCard* netcard)
{
    struct RTL8139DeviceInfo *dev = (struct RTL8139DeviceInfo*)netcard->deviceInfo;
    return dev->macAddress;
}

void* initrtl8139(unsigned int deviceAddr, char* buffer, unsigned int bufferSize)
{
    unsigned int templ;
    unsigned short tempw;
    unsigned long i;
    unsigned long tempq;

    struct RTL8139DeviceInfo *devInfo=0;    
    for (i=0;i<32;i++)
    {
        if (devInfoList[i].iobase==0)
        {
            devInfo = &devInfoList[i];
            break;
        }
    }
    if (devInfo==0) return 0;


    devInfo->rxBuffers = buffer;
    devInfo->rxBufferCount = bufferSize;

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

    pf("RTL8139 netcard found. IRQ=0x%x, IO=0x%x\r\n",devInfo->irq,devInfo->iobase);

    // Activate card
    OUTPORTB(0,devInfo->iobase+0x52);

    // reset
    unsigned char v=0x10;
    OUTPORTB(v,devInfo->iobase+0x37);
    while ((v&0x10)!=0)
    {
        INPORTB(v,devInfo->iobase+0x37);
    }

    
    INPORTL(templ,devInfo->iobase+4);
    tempq = templ;
    tempq = tempq <<32;
    INPORTL(templ,devInfo->iobase);
    tempq |= templ;
    devInfo->macAddress = tempq;

    return devInfo;
}


void rtl8139_start(struct NetworkCard* netcard)
{
    struct RTL8139DeviceInfo* dev = (struct RTL8139DeviceInfo*)netcard->deviceInfo;
    dev->readIndex = 0;
    dev->writeIndex = 0;
    dev->transmittedDescriptor = 0;
    dev->currentTXDescriptor = 0;

    // Enable TX and RX: 
    OUTPORTB(0b00001100,dev->iobase+0x37);

    // We need to uses physical addresses for the RX and TX buffers. In our case, we are fine since
    // we are using identity mapping with virtual memory.
    //Set the Receive Configuration Register (RCR)
    OUTPORTL(0x8F, dev->iobase+0x44);

    //Set the Transmit Configuration Register (TCR)
    OUTPORTL(0x03000600, dev->iobase+0x40);

    // set receive buffer address
    OUTPORTL((unsigned char*)&dev->rxbuf[0], dev->iobase+0x30);

    // set TX descriptors
    OUTPORTL((unsigned char*)&dev->txbuf[0][0], dev->iobase+0x20);
    OUTPORTL((unsigned char*)&dev->txbuf[1][0], dev->iobase+0x24);
    OUTPORTL((unsigned char*)&dev->txbuf[2][0], dev->iobase+0x28);
    OUTPORTL((unsigned char*)&dev->txbuf[3][0], dev->iobase+0x2C);
    // enable Full duplex 100mpbs
    OUTPORTB(0b00100001, dev->iobase+0x63);

    //enable TX and RX interrupts:
    OUTPORTW(0b101, dev->iobase+0x3C);

}

// checks if a frame is available and returns 0 if none are available
// If a frame is available, it is copied to the buffer (needs to be at least the size of an MTU)
// and returns the size of the data copied.
//WARNING: There is no locking mechanism here. Caller must make sure that this wont be executed by 2 threads
//   at the same time
unsigned long rtl8139_receive(unsigned char** buffer, struct NetworkCard* netcard)
{
    struct RTL8139DeviceInfo* dev = (struct RTL8139DeviceInfo*)netcard->deviceInfo;
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
    }
}

void rtl8139_recvProcessed(struct NetworkCard* netcard)
{
    struct RTL8139DeviceInfo* dev = (struct RTL8139DeviceInfo*)netcard->deviceInfo;
    dev->readIndex = (dev->readIndex+1) & (dev->rxBufferCount-1); // increment read index and wrap around 16
}

unsigned long rtl8139_send(struct NetworkBuffer *netbuf, struct NetworkCard* netcard)
{
    struct RTL8139DeviceInfo* dev = (struct RTL8139DeviceInfo*)netcard->deviceInfo;

    unsigned short size = netbuf->layer2Size+netbuf->layer3Size+netbuf->layer4Size+netbuf->payloadSize;
    if (size>1792)
    {
        pf("can't send. Frame too big: l2:%x, l3: %x, l4: %x payload size: %x\r\n",netbuf->layer2Size,netbuf->layer3Size,netbuf->layer4Size,netbuf->payloadSize);
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
        memcpy64((char*)&netbuf->layer4Data[0],(char*)&buf[netbuf->layer2Size+netbuf->layer3Size],netbuf->layer4Size);
        memcpy64((char*)&netbuf->payload[0],(char*)&buf[netbuf->layer2Size+netbuf->layer3Size+netbuf->layer4Size],netbuf->payloadSize);

        tsdValue = size;
        OUTPORTL(tsdValue,dev->iobase+tsd);
        dev->currentTXDescriptor = (dev->currentTXDescriptor+1)&0b11; // wrap around 4
        return size;
    }
}

