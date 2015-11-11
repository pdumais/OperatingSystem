#include "display.h"
#include "utils.h"
#include "netcard.h"

extern struct NetworkConfig net_getConfig();
extern unsigned long net_getMACAddress();
extern unsigned long arp_getMAC(unsigned int ip);


unsigned short ip_send(unsigned int destIP, unsigned char* buf, unsigned short size)
{
    unsigned long destMAC = 0;
    struct NetworkConfig conf = net_getConfig();
    unsigned int net1 = destIP & conf.subnetmask;
    unsigned int net2 = conf.ip & conf.subnetmask;
    if (net1==net2)
    {
        destMAC = arp_getMAC(destIP);
    }
    else
    {
        destMAC = arp_getMAC(conf.gateway);
    }

    // TODO: prepare IP header, copy data in buffer.
    

//    netcard_send(destMAC,0x0100

    
}
