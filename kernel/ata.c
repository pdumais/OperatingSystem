#include "utils.h"
#include "../memorymap.h"
#include "display.h"

// WARNING
//  we only support 48bitLBA (but we dont check for it)
//  dont support cdrom (ATAPI)

extern unsigned long long atomic_set(unsigned long long var_addr,unsigned long long bit);
extern void getInterruptInfoForBus(unsigned long bus, unsigned int* buffer);
extern unsigned int pci_getBar(unsigned int dev,unsigned char bar);
typedef void (*atairqcallback)(unsigned char, unsigned long, unsigned long);
typedef void (*atareadycallback)(unsigned char);

// Warning: we only support PATA now and we dont get registers from PCI
#define CH1BASE 0x1F0
#define CH1CONTROL 0x3F6
#define CH2BASE 0x170
#define CH2CONTROL 0x376

#define DATAPORT 0
#define FEATURE 1
#define SECTORCOUNT 2
#define COMMAND 7

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

// for status register (base+7)
#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

// for error register (base+1)
#define ATA_ER_BBK      0x80
#define ATA_ER_UNC      0x40
#define ATA_ER_MC       0x20
#define ATA_ER_IDNF     0x10
#define ATA_ER_MCR      0x08
#define ATA_ER_ABRT     0x04
#define ATA_ER_TK0NF    0x02
#define ATA_ER_AMNF     0x01


struct PRD
{
    unsigned int addr;
    unsigned short size;
    unsigned short reserved;
} __attribute((packed))__;

struct operation
{
    unsigned char dev;
    volatile unsigned char pending;
    unsigned long block;
    unsigned long count;
};

extern int pci_getDevice(unsigned long vendor, unsigned long device);
extern void pci_enableBusMastering(unsigned long addr);
extern void registerIRQ(void* handler, unsigned long irq);
extern unsigned short pci_getIRQ(unsigned long addr);

char tmpBuffer[512];
char channelSlaveSelection=0;
unsigned long busMasterRegister;
struct operation pendingRequest[2];
unsigned long long devices_lock[2];

atairqcallback irq_callback;
atareadycallback ready_callback;

void atahandler(unsigned short base, unsigned char channel)
{
    unsigned char val;
    INPORTB(val,base+7);
    if (val&1)
    {
        INPORTB(val,base+1);
        pf("ERROR! %x\r\n",val);
    }
    else if (pendingRequest[channel].pending)
    {
        pendingRequest[channel].pending = 0;
        irq_callback(pendingRequest[channel].dev,pendingRequest[channel].block,pendingRequest[channel].count);
    }
    INPORTB(val,busMasterRegister+2+(channel*8));
    OUTPORTB(4,busMasterRegister+2+(channel*8));

    devices_lock[channel] = 0;
    ready_callback(pendingRequest[channel].dev);
}

void atahandler1()
{
    atahandler(CH1BASE,0);
}

void atahandler2()
{
    atahandler(CH2BASE,1);
}

void ata_select_device(unsigned short dev, unsigned char slave)
{
    unsigned char val;
    unsigned int reg = CH1BASE - (dev<<7);

    if ((channelSlaveSelection&(1<<dev)) == ((slave&1)<<dev)) return;

    //OUTPORTB(0xA0 | (slave<<4),dev+ATA_REG_HDDEVSEL); // send "Select" command
    OUTPORTB(0xE0 | (slave<<4),reg+ATA_REG_HDDEVSEL); // send "Select" command
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);

    channelSlaveSelection &= ~(1<<dev);    
    channelSlaveSelection |= ((slave&1)<<dev);    

}

void ata_init_dev(unsigned short dev, unsigned char slave)
{
    unsigned int i;
    unsigned char val;
    unsigned int val2;
    unsigned int signature;
    unsigned int reg = CH1BASE - (dev<<7);

    pf("ATA DEVICE %x,%x: ",reg,slave);
    
    ata_select_device(dev,slave);

    OUTPORTB(ATA_CMD_IDENTIFY,reg+ATA_REG_COMMAND); // identify
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);
    INPORTB(val,reg+0x206);

    while (1)
    {
        INPORTB(val,reg+ATA_REG_STATUS);
        if (val == 0)
        {
            pf("none\r\n");
            return;
        } 
        else if (val&ATA_SR_ERR)
        {
            // Identify command does not work for ATAPI devices. Need to send IDENTIFY_PACKET.
            // but we will just ignore it here, we dont care about cdroms yet.
            pf("ATAPI\r\n");
            return;
        } 
        else if (!(val&ATA_SR_BSY)&&(val&ATA_SR_DRQ))
        {
            break;
        }
    }



    for (i=0;i<128;i++)
    {
        INPORTL(val2,reg+ATA_REG_DATA);
        ((unsigned int*)tmpBuffer)[i] = val2;
    }

    val2 = *((unsigned int *)(tmpBuffer+164));
    if (val2 & (1<<26))
    {
        val2 = *((unsigned int *)(tmpBuffer+200));
        pf("supported 48bit LBA device of size %x\r\n",val2*512);
        OUTPORTB(3,reg+ATA_REG_FEATURES); // DMA
    }
    else
    {
        pf("unsupported drive\r\n");
    }

}

void init_ata(atairqcallback irqcallback, atareadycallback readycallback)
{
    int dev;
    int i;

    irq_callback = irqcallback;
    ready_callback = readycallback;

    dev = pci_getDevice(0x8086,0x7010);
    pci_enableBusMastering(dev);

    busMasterRegister = pci_getBar(dev,4) & ~1;
    OUTPORTL(PRDT1,busMasterRegister+0x04); // set PRDT1 
    OUTPORTL(PRDT2,busMasterRegister+0x0C); // set PRDT2
    pf("IDE bus mastering Device: %x, bar4=%x\r\n", dev,busMasterRegister);
    
    ata_init_dev(0,0);
    ata_init_dev(0,1);
    ata_init_dev(1,0);
    ata_init_dev(1,1);

    // We only support legacy ATA which is on the ISA bus. it always uses IRQ14 and 15
    // So let's check where ISA IRQ 14 and 15 map on the APIC.
    unsigned long irq14 = 0;
    unsigned long irq15 = 0; 
    unsigned int ioapicDevices[64];
    getInterruptInfoForBus(0x20415349, &ioapicDevices); // "ISA "
    for (i=0;i<64;i++)
    {
        if (ioapicDevices[i]==0) continue;

        unsigned char pin = ioapicDevices[i]&0xFF;  
        unsigned short busirq = ioapicDevices[i]&0xFF00;
        if (busirq == 0x0E00) irq14=pin;
        if (busirq == 0x0F00) irq15=pin;
    }
    if (irq14!=0 && irq15!=0)
    {
        registerIRQ(&atahandler1,irq14);
        registerIRQ(&atahandler2,irq15);
    }
    else
    {
        pf("Could not find IOAPIC mapping of IRQ 14 and 15: %x %x\r\n",irq14,irq15);
    }
}

void convertDevId(unsigned int dev, unsigned int *device, unsigned char *slave)
{
    switch (dev)
    {
        case 0:
            *device = 0;
            *slave = 0;
        break;
        case 1:
            *device = 0;
            *slave = 1;
        break;
        case 2:
            *device = 1;
            *slave = 0;
        break;
        case 3:
            *device = 1;
            *slave = 1;
        break;
    }
}

int ata_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{
    unsigned int device;
    unsigned char slave,val;
    unsigned short reg,bmr;
    struct PRD *prdt = (struct PRD*)PRDT1;


    convertDevId(dev,&device,&slave);
    if (atomic_set(&devices_lock[device],0) == 1) return 0;

    ata_select_device(device,slave);
    pendingRequest[device].pending = 1;
    pendingRequest[device].block = sector;
    pendingRequest[device].count = count;
    pendingRequest[device].dev = dev;

    if (device==0)
    {
        bmr = busMasterRegister;
        reg = CH1BASE;
    }
    else
    {
        bmr = busMasterRegister+8;
        reg = CH2BASE;
    }

    // setup PRD
    prdt[device].addr = buffer;
    prdt[device].size = count*512; // sector size = 512
    prdt[device].reserved = 0x8000;

    // Stop DMA
    OUTPORTB(0b00001000,bmr);

    // write sector count and LBA48 address
    OUTPORTB(((count>>8)&0xFF),reg+2);
    OUTPORTB(((sector>>24)&0xFF),reg+3);
    OUTPORTB(((sector>>32)&0xFF),reg+4);
    OUTPORTB(((sector>>40)&0xFF),reg+5);
    OUTPORTB((count&0xFF),reg+2);
    OUTPORTB((sector&0xFF),reg+3);
    OUTPORTB(((sector>>8)&0xFF),reg+4);
    OUTPORTB(((sector>>16)&0xFF),reg+5);
    OUTPORTB(0x25,reg+7);

    // Start DMA (read)
    OUTPORTB(0b00001001,bmr);
    return 1;
}

int ata_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{
    unsigned int device;
    unsigned char slave,val;
    unsigned short reg,bmr;
    struct PRD *prdt = (struct PRD*)PRDT1;

    convertDevId(dev,&device,&slave);
    if (atomic_set(&devices_lock[device],0) == 1) return 0;

    ata_select_device(device,slave);
    pendingRequest[device].pending = 1;
    pendingRequest[device].block = sector;
    pendingRequest[device].count = count;
    pendingRequest[device].dev = dev;

    if (device==0)
    {
        bmr = busMasterRegister;
        reg = CH1BASE;
    }
    else
    {
        bmr = busMasterRegister+8;
        reg = CH2BASE;
    }

    // setup PRD
    prdt[device].addr = buffer;
    prdt[device].size = count*512; // sector size = 512
    prdt[device].reserved = 0x8000;

    // Stop DMA
    OUTPORTB(0b00000000,bmr);

    // write sector count and LBA48 address
    OUTPORTB(((count>>8)&0xFF),reg+2);
    OUTPORTB(((sector>>24)&0xFF),reg+3);
    OUTPORTB(((sector>>32)&0xFF),reg+4);
    OUTPORTB(((sector>>40)&0xFF),reg+5);
    OUTPORTB((count&0xFF),reg+2);
    OUTPORTB((sector&0xFF),reg+3);
    OUTPORTB(((sector>>8)&0xFF),reg+4);
    OUTPORTB(((sector>>16)&0xFF),reg+5);
    OUTPORTB(0x35,reg+7);

    // Start DMA (read)
    OUTPORTB(0b00000001,bmr);

//TODO: need to do a cache flush after writing
    return 1;
}


unsigned char ata_isBusy(dev)
{
    unsigned int device;
    unsigned char slave;
    convertDevId(dev,&device,&slave);
    return pendingRequest[device].pending;
}
