#include "ip.h"
#include "netcard.h"
#include "display.h"
#include "utils.h"
#include "config.h"

//tcpdump -i tap3 -nn -vvv -XX arp


extern void ip_init();

extern void* initrtl8139(unsigned int addr, char* buffer, unsigned int buffserSize);
extern void rtl8139_start();
extern unsigned long rtl8139_getMACAddress();
extern unsigned long rtl8139_receive(unsigned char** buffer,struct NetworkCard*);
extern void rtl8139_recvProcessed(struct NetworkCard*);
extern unsigned long rtl8139_send(struct NetworkBuffer *,struct NetworkCard*);

extern void* initvirtionet(unsigned int addr, char* buffer, unsigned int buffserSize);
extern void virtionet_start();
extern unsigned long virtionet_getMACAddress();
extern unsigned long virtionet_receive(unsigned char** buffer,struct NetworkCard*);
extern void virtionet_recvProcessed(struct NetworkCard*);
extern unsigned long virtionet_send(struct NetworkBuffer *,struct NetworkCard*);

extern void spinLock_softirq(spinlock_softirq_lock*);
extern void spinUnlock_softirq(spinlock_softirq_lock*);
extern void arp_process(struct Layer2Payload* payload);
extern void arp_learn(struct Layer2Payload* payload);
extern void yield();
extern char* kernelAllocPages();
extern unsigned int pci_getDeviceByClass(unsigned char class, unsigned char index, unsigned long* vendor, unsigned long* device, unsigned short* sub);
extern void ip_routing_addRoute(unsigned int network, unsigned int netmask, unsigned int gateway, unsigned char metric, unsigned long interface);
extern void tcp_init();

struct NetworkCard networkCards[32]={0};

unsigned char net_getNumberOfInterfaces()
{
    unsigned char i;
    unsigned char n = 0;
    for (i=0;i<32;i++)
    {
        if (networkCards[i].init!=0) n++;
    }
    return n;
}

unsigned char net_getInterfaceIndex(unsigned int ip)
{
    unsigned char i;
    unsigned int ipBE = ip;
    SWAP4(ipBE);
    for (i=0;i<32;i++)
    {
        if (networkCards[i].ownNetworkConfig.ip == ipBE) return i;
    }
    return 255;
}

void net_init()
{
    unsigned char i;
    for (i=0;i<32;i++)
    {
        unsigned long device,vendor;
        unsigned short subsystem;
        unsigned int addr = pci_getDeviceByClass(2,i,&vendor,&device,&subsystem);
        if (addr==0xFFFFFFFF) break;

        struct NetworkCard* netcard = &networkCards[i];
        if (vendor == 0x10EC && device == 0x8139)
        {
            netcard->init = &initrtl8139;
            netcard->start = &rtl8139_start;
            netcard->getMACAddress = &rtl8139_getMACAddress;
            netcard->receive = &rtl8139_receive;
            netcard->recvProcessed = &rtl8139_recvProcessed;
            netcard->send = &rtl8139_send;
            netcard->deviceInfo = (void*)netcard->init(addr);
        }
        if (vendor == 0x1AF4 && (device >= 0x1000 && device <= 0x103F))
        {
            netcard->init = &initvirtionet;
            netcard->start = &virtionet_start;
            netcard->getMACAddress = &virtionet_getMACAddress;
            netcard->receive = &virtionet_receive;
            netcard->recvProcessed = &virtionet_recvProcessed;
            netcard->send = &virtionet_send;
            netcard->deviceInfo = (void*)netcard->init(addr);
        }

    }
    ip_init();
    tcp_init();
}

void net_start()
{
    unsigned char i;
    for (i=0;i<32;i++) if (networkCards[i].start!=0) networkCards[i].start(&networkCards[i]);
}

unsigned long net_getMACAddress(unsigned char index)
{
    if (index>=32) return 0;
    if (networkCards[index].getMACAddress==0) return 0;
    return networkCards[index].getMACAddress(&networkCards[index]);
}

struct NetworkConfig* net_getConfig(unsigned char index)
{
    struct NetworkCard *netcard = &networkCards[index]; //TODO: do not hardcode index
    if (netcard->init==0) return 0;
    return &netcard->ownNetworkConfig;
}


void net_setDefaultGateway(unsigned int gateway, unsigned long cardIndex)
{
    ip_routing_addRoute(0, 0, gateway, 255, cardIndex);
}

void net_setIPConfig(unsigned int ip, unsigned int subnetmask, unsigned short vlan, unsigned long cardIndex)
{
    // Convert to big endian
    SWAP4(ip);
    SWAP4(subnetmask);
    SWAP2(vlan);

    struct NetworkCard *netcard = &networkCards[cardIndex];
    netcard->ownNetworkConfig.ip = ip;
    netcard->ownNetworkConfig.subnetmask = subnetmask;
    netcard->ownNetworkConfig.vlan = vlan;

    ip_routing_addRoute(ip&subnetmask,subnetmask,0,1,cardIndex);

}

// There is no need to lock in this function because
// only the softirq will call this. Locking with the 
// interrupt handler will is implemented in the netcard
void net_process()
{
    unsigned char cardIndex;
    for (cardIndex=0;cardIndex<32;cardIndex++)
    {
        struct NetworkCard *netcard = &networkCards[cardIndex]; 
        if (netcard->init==0) break;

        struct Layer2Payload payload;
        unsigned short destinationType;
        unsigned char* buf;
        unsigned short size = 1;
        while (size>0)
        {
            size  = netcard->receive(&buf,netcard);
            payload.size = 0;
            if (size>0)
            {
                payload.interface = cardIndex;
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
                    ip_process(&payload);
                }
                netcard->recvProcessed(netcard);
            }
        }
    }
}


unsigned long net_send(unsigned char interface, unsigned long destinationMAC, unsigned short vlan, unsigned short ethertype, struct NetworkBuffer* netbuf)
{
    struct NetworkCard *netcard = &networkCards[interface]; 
    unsigned long ret;
    unsigned char buf[20];
    unsigned short i;
    unsigned short payloadIndex;
    *((unsigned long*)&buf[0]) = destinationMAC;
    *((unsigned long*)&buf[6]) = net_getMACAddress(interface); 
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

    netbuf->layer2Data = (unsigned char*)&buf;
    netbuf->layer2Size = payloadIndex;

    unsigned char retry = 200;
    ret = 0;
    while (retry>0 && ret==0)
    {
        // Here, we use a spinlock_softirq. This will clear interrupts and acquire a spinlock.
        // The dangers are that this function can be called from the softirq. On mono-processsor
        // systems, a thread could acquire the lock and then be preempted in favor of the softirq.
        // Since the softirq has higher priority, on each schedule(), it would be scheduled back,
        // never leaving time for the thread to release the spinlock. So by clearing interrupts,
        // we can be guaranteed that this task will not be interrupted by a softirq and thus
        // creating a deadlock. On multi-cpu, the worst that would have happened is that this
        // CPU would continue to wait in the softirq context until the locking thread gets
        // scheduled on another CPU. So this allows us to send data over the network safely
        // from the softIRQ context.
        spinLock_softirq(&netcard->send_mutex);
        ret = netcard->send(netbuf,netcard);
        spinUnlock_softirq(&netcard->send_mutex);
        if (ret==0) retry--;
    }
    return ret-payloadIndex;
}

