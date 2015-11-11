#include "../memorymap.h"
#include "display.h"
#include "utils.h"

#define RX_BUFFER_SIZE 8192 


extern unsigned int pci_getDevice(unsigned long vendor, unsigned long device);
extern unsigned int pci_getBar(unsigned int dev,unsigned char bar);
extern unsigned short pci_getIRQ(unsigned int dev);
extern void pci_enableBusMastering(unsigned int dev);
extern void registerIRQ(void* handler, unsigned long irq);

/*
http://linuxgazette.net/156/jangir.html


WARNING: This driver has some limitations:
    1) we are assuming that only one nic can exist
    2) we assume that 1 IO base and 1 memory area exists (but that might be ok)
    3) we assume that the BIOS has configured the IRQ and memory address for that device

*/

volatile unsigned char readIndex=0;
volatile unsigned char writeIndex=0;
volatile unsigned char currentTXDescriptor;
volatile unsigned char transmittedDescriptor;

unsigned int deviceAddress = 0;
unsigned short iobase = 0;
unsigned long memoryAddress = 0;
unsigned short irq = 0;
unsigned long macAddress = 0;

unsigned char rxBuffers[16][2048]={0};
unsigned char txbuf[4][2048]={0};
unsigned char rxbuf[RX_BUFFER_SIZE+16]={0};
unsigned short rxBufferIndex = 0;

void handler()
{
    unsigned short isr;
    INPORTW(isr,iobase+0x3E);

    OUTPORTW(0xFFFF,iobase + 0x3E);
    unsigned int status;
    unsigned char  cmd=0;
    unsigned short size;
    unsigned short i;
    if (isr&1)                  // ROK
    {
        while (!(cmd&1))   // check if CMD.BUFE == 1
        {
            // if last frame overflowed buffer, this won't will start at rxBufferIndex%RX_BUFFER_SIZE instead of zero
            if (rxBufferIndex>=RX_BUFFER_SIZE) rxBufferIndex = (rxBufferIndex%RX_BUFFER_SIZE);

            status =*(unsigned int*)(rxbuf+rxBufferIndex);
            size = status>>16;
    
            for (i=0;i<size;i++)
            {
                rxBuffers[writeIndex][i] = rxbuf[rxBufferIndex];
                rxBufferIndex++;
            }

            rxBufferIndex = ((rxBufferIndex+4+3)&0xFFFC);
            // This is ridiculous, you need to substract 16 from CAPR. This is not mentionned anywhere. I had to look in
            // some other driver's source code to find out. I don't understand where that comes from
            OUTPORTW(rxBufferIndex-16,iobase+0x38);

            writeIndex = (writeIndex+1)&0x0F;
            if (writeIndex==readIndex)
            {
                // Buffer overrun
            }

            INPORTB(cmd,iobase+0x37);
        }
    }
    if (isr&0b100)              //TOK
    {
//pf("TX interrupt\r\n");
        unsigned long tsdCount = 0;
        unsigned int tsdValue;
        while (tsdCount <4)
        {
            unsigned short tsd = 0x10 + (transmittedDescriptor*4);
            transmittedDescriptor = (transmittedDescriptor+1)&0b11;
            INPORTL(tsdValue,iobase+tsd);
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
            OUTPORTL(0x2000,iobase+tsd); // set lenght to zero to clear the other flags but leave OWN to 1
            tsdCount++;
        }
    }
}

unsigned long rtl8139_getMACAddress()
{
    return macAddress;
}

void initrtl8139()
{
    unsigned int templ;
    unsigned short tempw;
    unsigned long i;
    unsigned long tempq;

    deviceAddress = pci_getDevice(0x10EC,0x8139);
    if (deviceAddress == 0xFFFFFFFF)
    {
        pf("No network card found\r\n");
        return;
    }

    for (i=0;i<6;i++)
    {
        unsigned int m = pci_getBar(deviceAddress,i);
        if (m==0) continue;
        if (m&1)
        {
            iobase = m & 0xFFFC;
        }
        else
        {
            memoryAddress = m & 0xFFFFFFF0;
        }
    }

    irq = pci_getIRQ(deviceAddress);
    registerIRQ(&handler,irq);
    pci_enableBusMastering(deviceAddress);

    pf("RTL8139 netcard found. IRQ=0x%x, IO=0x%x\r\n",irq,iobase);

    // Activate card
    OUTPORTB(0,iobase+0x52);

    // reset
    unsigned char v=0x10;
    OUTPORTB(v,iobase+0x37);
    while ((v&0x10)!=0)
    {
        INPORTB(v,iobase+0x37);
    }

    
    INPORTL(templ,iobase+4);
    tempq = templ;
    tempq = tempq <<32;
    INPORTL(templ,iobase);
    tempq |= templ;
    macAddress = tempq;
//    macAddress = ((tempq&0xFF)<<40)|((tempq&0xFF00)<<24)|((tempq&0xFF0000)<<8)|((tempq&0xFF000000)>>8)|((tempq&0xFF00000000)>>24)|((tempq&0xFF0000000000)>>40);

}


void rtl8139_start()
{
    readIndex = 0;
    writeIndex = 0;
    transmittedDescriptor = 0;
    currentTXDescriptor = 0;

    // Enable TX and RX: 
    OUTPORTB(0b00001100,iobase+0x37);

    // We need to uses physical addresses for the RX and TX buffers. In our case, we are fine since
    // we are using identity mapping with virtual memory.
    //Set the Receive Configuration Register (RCR)
    OUTPORTL(0x8F, iobase+0x44);

    // set receive buffer address
    OUTPORTL((unsigned char*)&rxbuf[0], iobase+0x30);

    // set TX descriptors
    OUTPORTL((unsigned char*)&txbuf[0][0], iobase+0x20);
    OUTPORTL((unsigned char*)&txbuf[1][0], iobase+0x24);
    OUTPORTL((unsigned char*)&txbuf[2][0], iobase+0x28);
    OUTPORTL((unsigned char*)&txbuf[3][0], iobase+0x2C);
    // enable Full duplex 100mpbs
    OUTPORTB(0b00100001, iobase+0x63);

    //enable TX and RX interrupts:
    OUTPORTW(0b101, iobase+0x3C);

}

// checks if a frame is available and returns 0 if none are available
// If a frame is available, it is copied to the buffer (needs to be at least the size of an MTU)
// and returns the size of the data copied.
//WARNING: There is no locking mechanism here. Caller must make sure that this wont be executed by 2 threads
//   at the same time
unsigned long rtl8139_receive(unsigned char** buffer)
{
//asm volatile("ud2");
    if (readIndex != writeIndex)
    {
        unsigned short size;
        unsigned short i;
        unsigned char* p = rxBuffers[readIndex];
        size = p[2] | (p[3]<<8);
        if (!(p[0]&1)) return 0; // PacketHeader.ROK
        *buffer = (char*)&p[4]; // skip header
        readIndex = (readIndex+1) & 0x0F; // increment read index and wrap around 16
        return size;
    }
    else
    {
        return 0;
    }
}

unsigned long rtl8139_send(unsigned char* buf, unsigned short size)
{
    if (size>1792) return 0;
    unsigned short tsd = 0x10 + (currentTXDescriptor*4);
    unsigned int tsdValue;
    INPORTL(tsdValue,iobase+tsd);
    
    if (tsdValue & 0x2000 == 0)
    {
        //the whole queue is pending packet sending
        return 0;
    }
    else
    {
        unsigned short i;
        for (i=0;i<size;i++) txbuf[currentTXDescriptor][i] = buf[i];
//        OUTPORTL((unsigned char*)&txbuf[currentTXDescriptor][0], iobase+tsd+0x10);
        tsdValue = size;
        OUTPORTL(tsdValue,iobase+tsd);
        currentTXDescriptor = (currentTXDescriptor+1)&0b11; // wrap around 4
        return size;
    }
}

