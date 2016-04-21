#include "utils.h"
#define MAC_DESTINATION_MULTICAST   0x8000
#define MAC_DESTINATION_UNICAST     0x4000
#define MAC_DESTINATION_BROADCAST   0x2000

struct Layer2Payload
{
    unsigned long from;
    unsigned long to;
    unsigned short vlan;
    unsigned short protocol;
    unsigned short size;
    unsigned char* data;
    unsigned char interface;
};

struct NetworkConfig
{
    unsigned int ip;
    unsigned int subnetmask;
    unsigned int gateway;
    unsigned short vlan;
};



struct NetworkBuffer
{
    unsigned char* payload;
    unsigned short payloadSize;
    unsigned char* layer3Data;
    unsigned short layer3Size;
    unsigned char* layer2Data;
    unsigned short layer2Size;
};


struct NetworkCard
{
    spinlock_softirq_lock send_mutex;
    struct NetworkConfig ownNetworkConfig;
    void* deviceInfo;

    void* (*init)(unsigned int);
    void (*start)(struct NetworkCard*);
    unsigned long (*getMACAddress)(struct NetworkCard*);
    unsigned long (*receive)(unsigned char** buffer, struct NetworkCard*);
    void (*recvProcessed)(struct NetworkCard*);
    unsigned long (*send)(struct NetworkBuffer *, struct NetworkCard*);
};

unsigned char net_getInterfaceIndex(unsigned int ip);
struct NetworkConfig* net_getConfig(unsigned char index);
