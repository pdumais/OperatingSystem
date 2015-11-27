#include "display.h"
#include "utils.h"
#include "netcard.h"
#include "sockets.h"
#include "ip.h"

#define MAX_IP_PAYLOAD_SIZE 1480
#define MAX_PACKET_COUNT 32

extern unsigned long ip_routing_route(unsigned int destinationIP, unsigned char* interface);
extern struct NetworkConfig* net_getConfig(unsigned char index);
extern unsigned long net_getMACAddress(unsigned char index);
extern unsigned long arp_getMAC(unsigned int ip, unsigned char* interface);
extern unsigned short checksum_1complement(unsigned char* buf, unsigned short size);
extern void icmp_process(char* buffer, unsigned short size, unsigned int from, unsigned long sourceInterface);
extern char* kernelAllocPages();
extern void memclear64(char* buf, unsigned long size);
extern unsigned long net_send(unsigned char interface, unsigned long destinationMAC, unsigned short vlan, unsigned short ethertype, struct NetworkBuffer* netbuf);
extern void memcpy64(char* source, char* destination, uint64_t size);

/*
    When a frame comes in, we will try to find a slot in the buffer that has a matching
    IP source and packet ID. If none are found, we will use the first available slot.
    We will then copy buffer to fragment offset within buffer and update received_size.
    By using the fragment offset this way, we will elliminate a potential problem
    where we could receive fragment #2 before fragment #1.
    when received_size == expected_size, send packet to ICMP/TCP/UDP layer
    When we have sent the packet to upper layer, we will free the slot by writing 0 in
    the IP source addr field.
*/

struct IPHeader
{
    unsigned char ihl:4;
    unsigned char version:4;
    unsigned char tos;
    unsigned short size;
    unsigned short id;
    unsigned short flagsOffset;
    unsigned char ttl;
    unsigned char protocol;
    unsigned short checksum;
    unsigned int source;
    unsigned int destination;
}__attribute__((packed));



/*
 Using 64k slots is not very efficient since we wont' get a lot of packets
of that size so it will be wastefull. But I will keep it this way for now
for simplicity's sake. If using slots the same size as the expected packet
size (as indicated in the IP header), this will introduce a concept
of memory fragmentation everytime we will free up a slot.
*/
//TODO: these should be in memory pool instead
struct PacketBufferSlot
{
    unsigned int ip;
    unsigned short id;
    unsigned short expectedSize;
    unsigned short receivedSize;
    unsigned short reserved;
    char payload[0x10000-12];
}__attribute__((packed));

unsigned short ipID=0;
struct PacketBufferSlot* packetBuffers;


void ip_init()
{
    unsigned long i;
    packetBuffers = kernelAllocPages((MAX_PACKET_COUNT*sizeof(struct PacketBufferSlot)/4096));

    for (i=0;i<MAX_PACKET_COUNT;i++)
    {
        packetBuffers[i].ip = 0;
    }    

}


struct PacketBufferSlot* getPacketBufferSlot(unsigned int ip, unsigned short id)
{
    //TODO: if packet is never completed because a fragment was lost, we should release 
    // the buffer and discard the packet. To do this, we should timestamp each packet.
    // then, when we need a buffer and all are used, we should try to delete the oldest
    // one if it is sufficiently old. Could use performance counter?? would need a very high 
    // resolution timestatmp.
    struct PacketBufferSlot* freeSlot = 0;
    unsigned int i;
    for (i=0;i<32;i++)
    {
        struct PacketBufferSlot* slot = &packetBuffers[i];
        if (slot->ip == 0) freeSlot = slot;
        if (slot->ip == ip && slot->id == id)
        {
            return slot;
        }

        if (slot->ip!=0) 
        {
            //TODO: unknown slot with size slot->receivedSize
            // Handle the failure
            asm("int $3");
        }
    }
    //TODO: must consider a slot free after some timeout.
    return freeSlot;
}



// sourceInterface is the interface that we will take the IP from and put it in the "from" of the IP packet. It is not
// the interface on which the packet will go out
// ip_send can send packets up to 64k in size. it will fragment message if too big for 1 MTU
int ip_send(unsigned long sourceInterface, unsigned int destIP, char* buffer, unsigned short payloadSize, unsigned char protocol)
{
    uint64_t i;
    
    ipID++; //TODO: this is not thread safe:
    unsigned char interface;
    unsigned long destMAC = ip_routing_route(destIP,&interface);

    if (destMAC==0) return IP_SEND_ERROR_NO_MAC;
    SWAP6(destMAC);

    struct NetworkConfig* conf = net_getConfig(sourceInterface);
    struct IPHeader header={0};
    header.version = 4;
    header.ihl = 5; // we don't support options for now
    header.tos = 0;
    header.id = ipID;
    header.ttl = 128;
    header.protocol = protocol; // should be passed as big-endian already
    header.source = conf->ip;
    header.destination = destIP;

    // we will fragment the packet in several pieces of max MTU size
    unsigned short sizeLeft = payloadSize;
    unsigned short ret = 0;
    unsigned short offset = 0;
    
    while (sizeLeft > 0)
    {   
        unsigned short size = (sizeLeft>MAX_IP_PAYLOAD_SIZE)?MAX_IP_PAYLOAD_SIZE:sizeLeft;
        sizeLeft-= size;
        header.flagsOffset = (sizeLeft>0)?0b001:0b000; // fragment?
        header.flagsOffset = header.flagsOffset<<13;
        header.flagsOffset |= (offset>>3);
        SWAP2(header.flagsOffset);
        header.size = (header.ihl<<2)+size;
        SWAP2(header.size);
        header.checksum = 0;
        header.checksum = checksum_1complement((unsigned char*)&header, sizeof(struct IPHeader));

        struct NetworkBuffer netbuf={0};
        netbuf.payload = (char*)&buffer[offset];
        netbuf.payloadSize = size;
        netbuf.layer3Data = (unsigned char*)&header;
        netbuf.layer3Size = sizeof(struct IPHeader);

        // Warning, we are working with big-endian here
        unsigned short r = net_send(interface, destMAC, 0x0100, 0x0008, &netbuf);
        if (r==0)
        {
            return IP_SEND_ERROR_HW;
        }
        ret+=(r-sizeof(struct IPHeader));

        offset+= size;
    }

    return ret;
}


void ip_process(struct Layer2Payload* payload)
{
    struct IPHeader* header = (struct IPHeader*)&payload->data[0];
    struct NetworkConfig* conf = net_getConfig(payload->interface);
    unsigned short dataOffset = header->ihl<<2;
    unsigned short size = header->size;
    SWAP2(size);
    unsigned short offset = header->flagsOffset;
    SWAP2(offset);
    unsigned short flags = offset >> 13;
    offset &= 0b0001111111111111;
    
    size = size - dataOffset;

    if (header->destination != conf->ip)
    {
        return;
    }

    unsigned int source = header->source;
    unsigned short id = header->id;

    struct PacketBufferSlot* slot = getPacketBufferSlot(source,id);
    if (slot == 0)
    {
        __asm("int $3");
    }
    if (slot->ip == 0)
    {
        slot->ip = header->source;  
        slot->id = header->id;
        slot->receivedSize = 0;
    }

    if ((flags&0b001)==0)
    {
        slot->expectedSize = (offset<<3) + size;
    }
    
    slot->receivedSize += size;

    memcpy64((char*)&payload->data[dataOffset],(char*)&slot->payload[offset<<3],size);
    if (slot->receivedSize == slot->expectedSize)
    {
        switch (header->protocol)
        {
            case 1: // ICMP
            {
                icmp_process((char*)&slot->payload[0],slot->receivedSize, header->source,payload->interface);
            }
            break;
            case 6: // TCP
            {
                tcp_process((char*)&slot->payload[0],slot->receivedSize, header->source, header->destination);
            }
            break;
            case 17: // UDP
            {
            }
            break;
        }

        // free slot back again
        slot->ip = 0;
    }

}
