#include "netcard.h"
#include "display.h"
#include "utils.h"

//tcpdump -i tap3 -nn -vvv -XX arp

extern void initrtl8139();
extern void rtl8139_start();
extern unsigned long rtl8139_getMACAddress();
extern unsigned long rtl8139_receive(unsigned char** buffer);
extern unsigned long rtl8139_send(unsigned char* buffer, unsigned short size);
extern void mutexLock(unsigned long*);
extern void mutexUnlock(unsigned long*);
extern void arp_process(struct Layer2Payload* payload);
extern void arp_learn(struct Layer2Payload* payload);

unsigned long recv_mutex=0;
unsigned long send_mutex=0;

struct NetworkConfig ownNetworkConfig={0};

void net_init()
{
    initrtl8139();
}

void net_start()
{
    rtl8139_start();
}

unsigned long net_getMACAddress()
{
    return rtl8139_getMACAddress();
}

struct NetworkConfig net_getConfig()
{
    return ownNetworkConfig;
}


void net_setIPConfig(unsigned int ip, unsigned int subnetmask, unsigned int gateway, unsigned short vlan)
{
    // Convert to big endian
    SWAP4(ip);
    SWAP4(subnetmask);
    SWAP4(gateway);
    SWAP2(vlan);

    ownNetworkConfig.ip = ip;
    ownNetworkConfig.subnetmask = subnetmask;
    ownNetworkConfig.gateway = gateway;
    ownNetworkConfig.vlan = vlan;
}

void net_process()
{
    struct Layer2Payload payload;
    unsigned short destinationType;
    unsigned char* buf;
    unsigned short size;
    mutexLock(&recv_mutex);
    size  = rtl8139_receive(&buf);
    mutexUnlock(&recv_mutex);
    payload.size = 0;
    if (size>0)
    {
        // WARNING: everything in net buffer is big-endiand
        payload.size = size - 4; // remove CRC at the end
        payload.to = *((unsigned long*)&buf[0])&0x0000FFFFFFFFFFFF;
        payload.from = *((unsigned long*)&buf[4])>>16;
        payload.protocol = *((unsigned short*)&buf[12]);
        if (payload.protocol == 0x0081) // vlan tag
        {
            unsigned short vlan = *((unsigned short*)&buf[14]);
            payload.vlan = vlan; // warning: this is also big-endian 
            payload.protocol = *((unsigned short*)&buf[16]);
            payload.data = (unsigned char*)&buf[18];
        }
        else
        {
            payload.vlan = 1;
            payload.data = (unsigned char*)&buf[14];
        }
        if (payload.protocol==(0x0608))
        {
            arp_process(&payload);
        } 
        else if (payload.protocol==(0x0008))
        {
            arp_learn(&payload);
        //    ip_process(&payload);
        }



        //pf("Received frame: size=%x, to=%x, from=%x, proto=%x\r\n",payload.size,payload.to,payload.from,payload.protocol);
        /*unsigned short i;
        for (i=0;i<12;i++)
        {
            unsigned char c = buf[i];
            pf("%x ",c);
        }
        pf("\r\n");
        while(1);*/

    }
}

unsigned long net_send(unsigned long destinationMAC, unsigned short vlan, unsigned short ethertype, unsigned char* payload, unsigned short payloadSize)
{

    unsigned long ret;
    unsigned char buf[1792];
    unsigned short i;
    unsigned short payloadIndex;
    *((unsigned long*)&buf[0]) = destinationMAC;
    *((unsigned long*)&buf[6]) = net_getMACAddress();
    if (vlan!=0x0100) // big-endian
    {
        *((unsigned long*)&buf[12]) = 0x0081;
        *((unsigned long*)&buf[14]) = vlan;
        *((unsigned long*)&buf[16]) = ethertype;
        payloadIndex = 18;
    }
    else
    {
        *((unsigned long*)&buf[12]) = ethertype;
        payloadIndex = 14;
    }
    for (i=0;i<payloadSize;i++) buf[payloadIndex+i] = payload[i];

    unsigned char retry = 20;
    ret = 0;
    while (retry>0 && ret==0)
    {
        mutexLock(&send_mutex);
        ret = rtl8139_send((char*)&buf[0],payloadSize+payloadIndex);
        mutexUnlock(&send_mutex);
        // TODO: that should return -1 if it was because buffers were not ready. we don't want to retry for another reason
        if (ret==0)
        {
            YIELD();
            retry--;
        }
    }
    return ret;
}

