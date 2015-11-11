#include "utils.h"
#include "display.h"
#include "netcard.h"

extern void memcpy64(char* src, char* dst, unsigned long size);
extern unsigned short ip_send(unsigned long sourceInterface, unsigned int destIP, char* buf, unsigned short size, unsigned char protocol);
extern unsigned short checksum_1complement(unsigned char* buf, unsigned short size);
extern unsigned long getSecondsSinceBoot();

// this buffer is create on heap because it would be too big for stack. It will only be used by the 
// receive thread when replying to ping requests
char buf[65536];

void icmp_process(char* buffer, unsigned short size, unsigned int from, unsigned long sourceInterface)
{
    unsigned short typecode = *(unsigned short*)&buffer[0];
    if (typecode == 0x0008)
    {
        unsigned short i;
        for (i=0;i<((size+1)&(~1));i++) buf[i]=0;
        memcpy64(buffer,(char*)&buf[0],size);
        *(unsigned short*)&buf[0] = 0;
        *(unsigned short*)&buf[2] = 0;
        unsigned short checksum = checksum_1complement((char*)&buf[0],((size+1)&(~1)));
        *(unsigned short*)&buf[2] = checksum;

        // WARNING: ip_send could make a ARP query and we would wait for the response. But 
        // we are already in the receive thread here. This is somewhat safe anyway because
        // we have learned the MAC of the guy who made the ping request so we know ip_send()
        // will not make a ARP query.
        ip_send(sourceInterface, from, (char*)&buf[0], size, 0x01);        
    }
    else if (typecode == 0x0000)
    {
    }

}
