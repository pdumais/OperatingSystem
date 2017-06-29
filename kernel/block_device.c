#include "block_device.h"
#include "printf.h"
#include "macros.h"
#include "utils.h"

extern int init_ata();
extern int ata_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern int ata_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern bool ata_pci_device_matches(u16 vendor, u16 device, u16 subsystem);
extern u64 ata_add_device(u32 addr, u32 iobase, u64* hw_index);
extern u64 ata_get_size(u64 dev);

extern int init_virtioblock();
extern int virtioblock_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern int virtioblock_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern bool virtblock_pci_device_matches(u16 vendor, u16 device, u16 subsystem);
extern u64 virtioblock_add_device(u32 addr, u32 iobase, u64* hw_index);
extern u64 virtblock_get_size(u64 dev);

extern unsigned int pci_getDeviceByClass(unsigned char class, unsigned char index, unsigned long* vendor, unsigned long* device, unsigned short* sub);
extern unsigned int pci_getBar(unsigned int dev,unsigned char bar);
extern unsigned short pci_getIRQ(unsigned int dev);
extern unsigned short pci_getIOAPICIRQ(unsigned int dev);

static block_device devices[32];
static blockirqcallback irq_callback;
static blockreadycallback ready_callback;

typedef struct
{
    int (*read)(unsigned int,unsigned long, char*, unsigned long);
    int (*write)(unsigned int,unsigned long, char*, unsigned long);
    int (*pci_device_matches)(unsigned short, unsigned short, unsigned short);
    int (*add_device)(u32,u32,u64*);
    u64 (*get_size)(u64);
    u8 type;
} driver_info;

driver_info drivers[] = {
    { 
        .read = &ata_read, 
        .write = &ata_write,
        .pci_device_matches = &ata_pci_device_matches,
        .type = BLOCK_DEVICE_TYPE_ATA,
        .get_size = &ata_get_size,
        .add_device = &ata_add_device
    },
    { 
        .read = &virtioblock_read, 
        .write = &virtioblock_write,
        .pci_device_matches = &virtblock_pci_device_matches,
        .add_device = &virtioblock_add_device,
        .get_size = &virtblock_get_size,
        .type = BLOCK_DEVICE_TYPE_VIRTIO
    }
};

#define DRIVER_COUNT (sizeof(drivers)/sizeof(driver_info))

void ata_irq(unsigned char dev, unsigned long block, unsigned long count)
{
    unsigned int i;
    for (i = 0; i <32; i++)
    {
        if (devices[i].hw_device_number == dev && devices[i].type == BLOCK_DEVICE_TYPE_ATA)
        {
            irq_callback(i,block,count);
            return;
        }
    }
}

void ata_ready(unsigned char dev)
{
    unsigned int i;
    for (i = 0; i <32; i++)
    {
        if (devices[i].hw_device_number == dev && devices[i].type == BLOCK_DEVICE_TYPE_ATA)
        {
            ready_callback(i);
            return;
        }
    }
}

void virtio_irq(unsigned char dev, unsigned long block, unsigned long count)
{
    unsigned int i;
    for (i = 0; i <32; i++)
    {
        if (devices[i].hw_device_number == dev  && devices[i].type == BLOCK_DEVICE_TYPE_VIRTIO)
        {
            irq_callback(i,block,count);
            return;
        }
    }
}

void virtio_ready(unsigned char dev)
{
    unsigned int i;
    for (i = 0; i <32; i++)
    {
        if (devices[i].hw_device_number == dev && devices[i].type == BLOCK_DEVICE_TYPE_VIRTIO)
        {
            ready_callback(i);
            return;
        }
    }
}

void init_block(blockirqcallback icb, blockreadycallback rcb)
{
    unsigned int i,n;
    unsigned int dev_num = 0;
    unsigned int dev_count;
    u64 device,vendor;
    u16 subsystem;

    irq_callback = icb;
    ready_callback = rcb;

    init_ata(&ata_irq,&ata_ready);
    init_virtioblock(&virtio_irq, &virtio_ready);

    for (n = 0; n < DRIVER_COUNT; n++)
    {
        driver_info* driver = &drivers[n];
        for (i = 0; i < 32; i++)
        {
            vendor =0;  
            device = 0;
            subsystem = 0;
            u32 addr = pci_getDeviceByClass(1,i,&vendor,&device,&subsystem);
            if (addr==0xFFFFFFFF) continue;
            if (driver->pci_device_matches(vendor,device,subsystem))
            {
                u32 iobase = 0;
                int i2;
                for (i2=0;i2<6;i2++)
                {
                    u32 m = pci_getBar(addr,i2);
                    if (m==0) continue;
                    if (m&1)
                    {
                        iobase = m & 0xFFFC;
                        break;
                    }
                }
                if (iobase == 0) C_BREAKPOINT();

                u64 hw_index;
                u64 count = driver->add_device(addr, iobase, &hw_index);
                for (i2=0;i2<count;i2++)      
                {
                    devices[dev_num].hw_device_number = hw_index; 
                    devices[dev_num].read = driver->read;
                    devices[dev_num].write = driver->write;
                    devices[dev_num].get_size = driver->get_size;
                    devices[dev_num].type = driver->type;
                    pf("Adding block device number %x, type [%x], hw index [%x], size [%x]\r\n",
                        dev_num,driver->type, hw_index, (driver->get_size(hw_index)*512));

                    dev_num++;
                    hw_index++;
                    if (dev_num >= 32) C_BREAKPOINT();
                }
            }
        }
    }
}

int block_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{
    block_device* d = &devices[dev];
    return d->read(d->hw_device_number,sector,buffer,count);
}

int block_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count)
{
    block_device* d = &devices[dev];
    return d->write(d->hw_device_number,sector,buffer,count);
}

u64 block_get_size(unsigned int dev)
{
    block_device* d = &devices[dev];
    return d->get_size(d->hw_device_number);
}
