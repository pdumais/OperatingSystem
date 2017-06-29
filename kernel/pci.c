#include "../memorymap.h"
#include "config.h"
#include "printf.h"

extern unsigned long ioAPICGetRedirect(unsigned long index);
extern void getInterruptInfoForBus(unsigned long bus, unsigned int* buffer);
#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC
#define OUTPORTW(val,port) asm volatile( "outw %0, %1" : : "a"(val), "Nd"((unsigned short)port) );
#define OUTPORTL(val,port) asm volatile( "outl %0, %1" : : "a"(val), "Nd"((unsigned short)port) );
#define INPORTL(ret,port) asm volatile( "inl %1, %0" : "=a"(ret) : "Nd"((unsigned short)port) );


// http://www.freewebz.com/mrd/os/pci/pci.html
// http://books.google.ca/books?id=tVfeqL5F1DwC&pg=PA78&lpg=PA78&dq=CONFIG_ADDRESS+pci&source=bl&ots=d0GeHkoa9Y&sig=FHnsqd0rb1dA2Pktt7MWhl0Pm64&hl=en&sa=X&ei=ao2WUun-BNPmsASWn4CwCw&ved=0CDYQ6AEwAg#v=onepage&q=CONFIG_ADDRESS%20pci&f=false
// http://www.acm.uiuc.edu/sigops/roll_your_own/7.c.html
// http://www.acm.uiuc.edu/sigops/roll_your_own/7.c.0.html

struct PCIDevice
{
    unsigned int    bars[6];
    unsigned short  bus;
    unsigned short  devnum;
    unsigned short  vendorID;
    unsigned short  deviceID;
    unsigned char   irq;
    unsigned char   pciIntPin;
    unsigned char   classCode;
    unsigned char   type;
    unsigned char   function;
    unsigned char   ioapicPin;
    unsigned char   ioapicID;
    unsigned short   subsystemID;
}__attribute((packed))__;


unsigned int ioapicDevices[128];
struct PCIDevice pciDevices[MAX_PCI_DEVICES_SUPPORTED]={0};
unsigned long pciDeviceIndex = 0;

void pci_setIOAPICInterruptIndex(struct PCIDevice *dev)
{
    unsigned int i;

    for (i=0;i<128&&ioapicDevices[i]!=0;i++)
    {
        unsigned char ioapicpin = (ioapicDevices[i])&0xFF;
        unsigned char devnum = (ioapicDevices[i]>>10)&0b11111;
        unsigned char pin = (ioapicDevices[i]>>8)&0b11;
        unsigned char bus = (ioapicDevices[i]>>16)&0xFF;

        if (bus==dev->bus && devnum==dev->devnum && pin==(dev->pciIntPin-1))
        {
            dev->ioapicPin = ioapicpin;
            dev->ioapicID = 0; // TODO: should not hardcode that
            return;
        }
    }
}

// access device by Bus,Device,Function (BDF)
unsigned int getPCIData(unsigned long bus, unsigned long device, unsigned long function, unsigned long reg)
{
    unsigned int address = (1<<31)|(bus<<16)|(device<<11)|(function<<8)|((reg)&0b11111100);
    OUTPORTL(address,CONFIG_ADDRESS);
    unsigned int ret;
    INPORTL(ret,CONFIG_DATA);
    return ret;
}


void pci_enableBusMastering(unsigned int address)
{
    address = (1<<31)|address|(4);
    OUTPORTL(address,CONFIG_ADDRESS);
    unsigned int ret;
    INPORTL(ret,CONFIG_DATA);
    OUTPORTL(ret|0b0000000100,CONFIG_DATA); // bus mastering
}

unsigned long validateDevice(unsigned long bus, unsigned long device, unsigned char function)
{
    // get the vendorID (reg=0) for the bus/device/function. Function will always be zero here since
    // we won't support multi-function devices
    unsigned int data = getPCIData(bus,device,function,0);
    unsigned short vendorID = data & 0xFFFF;
    unsigned short devID = (data>>16);
    data = getPCIData(bus,device,function,0x2C);
    unsigned short subID = data>>16;
    if (vendorID != 0xFFFF)
    {
        data = getPCIData(bus,device,function,0x0C);
        pciDevices[pciDeviceIndex].type = (data>>16)&0xFF;
        data = getPCIData(bus,device,function,0x3C);
        pciDevices[pciDeviceIndex].bus = bus;
        pciDevices[pciDeviceIndex].devnum = device;
        pciDevices[pciDeviceIndex].vendorID = vendorID;
        pciDevices[pciDeviceIndex].function = function;
        pciDevices[pciDeviceIndex].deviceID = devID;
        pciDevices[pciDeviceIndex].irq = (data&0xFF);
        pciDevices[pciDeviceIndex].pciIntPin = ((data>>8)&0xFF);
        pciDevices[pciDeviceIndex].bars[0] = getPCIData(bus,device,function,0x10);
        pciDevices[pciDeviceIndex].bars[1] = getPCIData(bus,device,function,0x14);
        pciDevices[pciDeviceIndex].bars[2] = getPCIData(bus,device,function,0x18);
        pciDevices[pciDeviceIndex].bars[3] = getPCIData(bus,device,function,0x1C);
        pciDevices[pciDeviceIndex].bars[4] = getPCIData(bus,device,function,0x20);
        pciDevices[pciDeviceIndex].bars[5] = getPCIData(bus,device,function,0x24);
        pciDevices[pciDeviceIndex].subsystemID = subID,

        data = getPCIData(bus,device,function,0x08);
        pciDevices[pciDeviceIndex].classCode = (data>>24);
        
        pci_setIOAPICInterruptIndex((struct PCIDevice*)&pciDevices[pciDeviceIndex]);
        pciDeviceIndex++;
        return pciDeviceIndex-1;
    }

    return -1;
}

unsigned int getAddress(struct PCIDevice *dev)
{
    return (1<<31)|(dev->bus<<16)|(dev->devnum<<11)|(dev->function<<8);
}

unsigned int pci_getBar(unsigned int dev,unsigned char bar)
{
    unsigned long i;
    for (i=0;i<pciDeviceIndex;i++)
    {
        if (getAddress(&pciDevices[i]) == dev)
        {
            return pciDevices[i].bars[bar];
        }
    }
    return 0xFFFFFFFF;
}

unsigned short pci_getIOAPICIRQ(unsigned int dev)
{
    unsigned long i;
    for (i=0;i<pciDeviceIndex;i++)
    {
        if (getAddress(&pciDevices[i]) == dev)
        {
            return pciDevices[i].ioapicPin;
        }
    }
    return 0xFFFF;
}

unsigned short pci_getIRQ(unsigned int dev)
{
    unsigned long i;
    for (i=0;i<pciDeviceIndex;i++)
    {
        if (getAddress(&pciDevices[i]) == dev)
        {
            return pciDevices[i].irq;
        }
    }
    return 0xFFFF;
}


unsigned int pci_getDevice(unsigned long vendor, unsigned long device)
{
    unsigned long i;
    for (i=0;i<pciDeviceIndex;i++)
    {
        if (pciDevices[i].vendorID == vendor && pciDevices[i].deviceID == device)
        {
            return getAddress(&pciDevices[i]);
        }
    }
    return 0xFFFFFFFF;
}

unsigned int pci_getDeviceByClass(unsigned char class, unsigned char index, unsigned long* vendor, unsigned long* device, unsigned short* subsystem)
{
    unsigned long i;
    for (i=0;i<pciDeviceIndex;i++)
    {
        if (pciDevices[i].classCode == class)
        {
            if (index==0)
            {   
                *vendor = pciDevices[i].vendorID;
                *device = pciDevices[i].deviceID;
                *subsystem = pciDevices[i].subsystemID;
                return getAddress(&pciDevices[i]);
            }
            index--;
        }
    }
    return 0xFFFFFFFF;
}

void initPCI()
{
    pf("Scanning PCI BUS ...\r\n");
    unsigned long bus;
    unsigned long device;
    unsigned char function;
    unsigned int interrupts[64];

    getInterruptInfoForBus(0x20494350, ioapicDevices);  // "PCI "

    for (bus=0;bus<256;bus++)
    {
        for (device=0;device<32;device++)
        {
            unsigned long index = validateDevice(bus,device,0);
            if (index!=-1) if (pciDevices[index].type&0x80)
            {
                for (function=1;function<8;function++)
                {
                    validateDevice(bus,device,function);
                }
            }
        }
    }
    
    unsigned long i;
    for (i=0;i<pciDeviceIndex;i++)
    {
        pf("PCI: [%x:%x:%x]: %x/%x, class=%x, ioAPIC pin:%x\r\n",
            pciDevices[i].bus,pciDevices[i].devnum,pciDevices[i].function,pciDevices[i].vendorID,
            pciDevices[i].deviceID,pciDevices[i].classCode, pciDevices[i].ioapicPin);
    }
}
