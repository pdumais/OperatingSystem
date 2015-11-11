#include "netcard.h"
#include "display.h"
#include "utils.h"

extern struct NetworkConfig* net_getConfig(unsigned char index);
extern unsigned long net_getMACAddress(unsigned char index);
extern unsigned long arpcache_get(unsigned int ip);
extern void arpcache_put(unsigned int ip, unsigned long mac);
extern void yield();
extern unsigned long net_send(unsigned char interface, unsigned long destinationMAC, unsigned short vlan, unsigned short ethertype, struct NetworkBuffer* netbuf);
extern unsigned char net_getNumberOfInterfaces();

void arp_process(struct Layer2Payload* payload)
{

    unsigned long ownMAC = net_getMACAddress(payload->interface);
    unsigned short hwType = *((unsigned short*)&payload->data[0]);
    unsigned short protocolType = *((unsigned short*)&payload->data[2]);
    unsigned short operation = *((unsigned short*)&payload->data[6]);
    unsigned int targetIP = *((unsigned int*)&payload->data[24]);    

    struct NetworkConfig* conf = net_getConfig(payload->interface);
    if (payload->to == 0x0000FFFFFFFFFFFF)
    {
        if (hwType==0x0100 && protocolType==0x0008)
        {
            if (operation==0x0100 && targetIP == conf->ip)
            {
                unsigned char* buf = payload->data;
                unsigned char buf2[28];
                (*(unsigned short*)&buf2[0]) = 0x0100;
                (*(unsigned short*)&buf2[2]) = 0x0008;
                (*(unsigned short*)&buf2[4]) = 0x0406;
                (*(unsigned short*)&buf2[6]) = 0x0200;
                (*(unsigned short*)&buf2[8]) = (unsigned short)(ownMAC&0xFFFF);
                (*(unsigned short*)&buf2[10]) = (unsigned short)((ownMAC>>16)&0xFFFF);
                (*(unsigned short*)&buf2[12]) = (unsigned short)((ownMAC>>32)&0xFFFF);
                (*(unsigned short*)&buf2[14]) = (unsigned short)((conf->ip)&0xFFFF);
                (*(unsigned short*)&buf2[16]) = (unsigned short)((conf->ip>>16)&0xFFFF);
                (*(unsigned short*)&buf2[18]) = *(unsigned short*)&buf[8];
                (*(unsigned short*)&buf2[20]) = *(unsigned short*)&buf[10];
                (*(unsigned short*)&buf2[22]) = *(unsigned short*)&buf[12];
                (*(unsigned short*)&buf2[24]) = *(unsigned short*)&buf[14];
                (*(unsigned short*)&buf2[26]) = *(unsigned short*)&buf[16];

                struct NetworkBuffer netbuf={0};
                netbuf.layer3Data = (unsigned char*)&buf2;
                netbuf.layer3Size = 28;
                net_send(payload->interface, payload->from, 0x0100, 0x0608, &netbuf);    
            }
        }
        
    }

    if (operation  == 0x0200)
    {
        unsigned long mac = (*((unsigned long*)&payload->data[8])&0x0000FFFFFFFFFFFF);
        unsigned int ip = (*((unsigned int*)&payload->data[14]));
        SWAP4(ip);
        SWAP6(mac);
        arpcache_put(ip,mac);
    }

}

void arp_query(unsigned int ip, unsigned long interface)
{
    // Queries will only be done for nodes on the same network.
    SWAP4(ip);
    unsigned char i;

    unsigned long ret;
    unsigned char buf[28];
    struct NetworkConfig* conf = net_getConfig(interface);
    unsigned long ownMAC = net_getMACAddress(interface);
    
    (*(unsigned short*)&buf[0]) = 0x0100;
    (*(unsigned short*)&buf[2]) = 0x0008;
    (*(unsigned short*)&buf[4]) = 0x0406;
    (*(unsigned short*)&buf[6]) = 0x0100;
    (*(unsigned short*)&buf[8]) = (unsigned short)(ownMAC&0xFFFF);
    (*(unsigned short*)&buf[10]) = (unsigned short)((ownMAC>>16)&0xFFFF);
    (*(unsigned short*)&buf[12]) = (unsigned short)((ownMAC>>32)&0xFFFF);
    (*(unsigned short*)&buf[14]) = (unsigned short)((conf->ip)&0xFFFF);
    (*(unsigned short*)&buf[16]) = (unsigned short)((conf->ip>>16)&0xFFFF);
    (*(unsigned short*)&buf[18]) = 0x0000;
    (*(unsigned short*)&buf[20]) = 0x0000;
    (*(unsigned short*)&buf[22]) = 0x0000;
    (*(unsigned short*)&buf[24]) = (unsigned short)((ip)&0xFFFF);
    (*(unsigned short*)&buf[26]) = (unsigned short)((ip>>16)&0xFFFF);
    
    struct NetworkBuffer netbuf={0};
    netbuf.layer3Data = (unsigned char*)&buf;
    netbuf.layer3Size = 28;
    // Warning, we are working with big-endian here
    ret = net_send(interface, 0xFFFFFFFFFFFF, 0x0100, 0x0608, &netbuf);
}

//WARNING: this parameter takes big-endian
unsigned long arp_getMAC(unsigned int ip, unsigned long interface)
{
    unsigned int temp = ip;
    SWAP4(temp);
    unsigned long mac = 0;
    mac = arpcache_get(temp); // ARP cache is little endian
    if (mac==0)
    {
        arp_query(ip, interface);
        unsigned char retry = 50;    
        while (mac==0 && retry > 0)
        {
            yield();
            mac = arpcache_get(temp);
            retry--;
        }
    }

    return mac;
}

void arp_learn(struct Layer2Payload* payload)
{
    struct NetworkConfig* conf = net_getConfig(payload->interface);
    unsigned int sourceIP = *(unsigned int*)&payload->data[12];
    unsigned int net1 = sourceIP & conf->subnetmask;
    unsigned int net2 = conf->ip & conf->subnetmask;
    if (net1==net2)
    {
        unsigned long mac = payload->from;
        SWAP6(mac);
        SWAP4(sourceIP);
        arpcache_put(sourceIP,mac);
    }
}
