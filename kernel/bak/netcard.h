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
};

struct NetworkConfig
{
    unsigned int ip;
    unsigned int subnetmask;
    unsigned int gateway;
    unsigned short vlan;
};


