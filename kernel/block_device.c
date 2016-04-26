#include "block_device.h"
#include "display.h"
#include "macros.h"
#include "utils.h"

extern int init_ata();
extern int ata_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern int ata_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);

extern int init_virtioblock();
extern int virtioblock_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern int virtioblock_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);

static block_device devices[32];
static blockirqcallback irq_callback;
static blockreadycallback ready_callback;

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
    unsigned int i;
    unsigned int dev_num = 0;
    unsigned int dev_count;

    irq_callback = icb;
    ready_callback = rcb;

    // discover ata devices
    dev_count = init_ata(&ata_irq,&ata_ready);
    for (i = 0; (i < dev_count && dev_num <32); i++)
    {
        devices[dev_num].hw_device_number = i;
        devices[dev_num].read = &ata_read;
        devices[dev_num].write = &ata_write;
        devices[dev_num].type = BLOCK_DEVICE_TYPE_ATA;
        dev_num++;
    }
    if (dev_num >= 32) return;

    // now discover virtio block devices
    dev_count = init_virtioblock(&virtio_irq, &virtio_ready);
    for (i = 0; (i < dev_count && dev_num<32); i++)
    {
        pf("Adding virtio block device %x (hw index %x)\r\n",dev_num,i);
        devices[dev_num].hw_device_number = i;
        devices[dev_num].read = &virtioblock_read;
        devices[dev_num].write = &virtioblock_write;
        devices[dev_num].type = BLOCK_DEVICE_TYPE_VIRTIO;
        dev_num++;
    }
    if (dev_num >= 32) return;

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
