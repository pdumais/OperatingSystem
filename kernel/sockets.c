#include "sockets.h"
#include "../memorymap.h"
#include "ip.h"
#include "netcard.h"

#define EPHEMERAL_START 32768
#define EPHEMERAL_END 65535

void socket_destructor(system_handle* h);
void add_socket_in_list(socket* s);
void remove_socket_from_list(socket* s);
extern uint64_t atomic_increase_within_range(uint64_t* var,uint64_t start, uint64_t end);
extern uint16_t checksum_1complement(void* buf, uint64_t size);

volatile uint64_t ephemeralPort = EPHEMERAL_START;

typedef struct
{
    uint32_t sourceIP;
    uint32_t destinationIP;
    uint8_t zero;
    uint8_t protocol;
    uint16_t length;
} pseudo_header;

//TCP flags:
// F: 0, S:1, R:2, P:3, A:4, U:5

uint16_t getEphemeralPort()
{
    //TODO: just incrementing is bad, must make sure port is
    //      not used by another socket
    return (uint16_t)atomic_increase_within_range(&ephemeralPort,EPHEMERAL_START,EPHEMERAL_END);
}

void send_tcp_message(socket* s, uint8_t control, char* payload, uint16_t size)
{
    char tmp[1500];
    pseudo_header *ph = (pseudo_header*)tmp;
    tcp_header *h = (tcp_header*)&tmp[sizeof(pseudo_header)];
    char* payloadCopy = (char*)&tmp[sizeof(pseudo_header)+sizeof(tcp_header)];
    memcpy64(payload,payloadCopy,size);
    
    uint64_t sourceInterface = (uint64_t)net_getInterfaceIndex(s->sourceIP);

    h->source = __builtin_bswap16(s->sourcePort);
    h->destination = __builtin_bswap16(s->destinationPort);
    h->sequence = __builtin_bswap32(s->tcp.seqNumber);
    h->acknowledgement = __builtin_bswap32(s->tcp.ackNumber);
    h->flags = 0x5000|control;
    h->flags = __builtin_bswap16(h->flags);
    h->window = __builtin_bswap16(0x100);

    ph->sourceIP = __builtin_bswap32(s->sourceIP);
    ph->destinationIP = __builtin_bswap32(s->destinationIP);
    ph->zero = 0;
    ph->protocol = 6;
    ph->length = size + sizeof(tcp_header); 
    h->checksum = checksum_1complement(tmp, sizeof(pseudo_header)+sizeof(tcp_header)+size);
    ip_send(sourceInterface, s->destinationIP, h, sizeof(tcp_header)+size, 6);
}

socket* create_socket()
{
    socket* s = (socket*)malloc(sizeof(socket));
    memclear64(s,sizeof(socket));

    s->handle.destructor = &socket_destructor;
    s->messages = (socket_message*)malloc(sizeof(socket_message)*MAX_RINGBUFFER_SOCKET_MESSAGE_COUNT);

    __asm("mov %%cr3,%0" : "=r"(s->owner));
    add_socket_in_list(s);

    return s;
}

void close_socket(socket* s)
{
    remove_socket_from_list(s);

    free((void*)s->messages);
    free((void*)s);
}

void connect(socket *s, uint32_t ip, uint16_t port)
{
    //TODO: choose source IP according to destination ip. Right now
    //      we hardcode it to the first interface.
    struct NetworkConfig* config = net_getConfig(0);
    if (config == 0) return;
    s->sourceIP = __builtin_bswap32(config->ip); 
    
    s->tcp.connected = 0;
    s->destinationIP = ip;
    s->destinationPort = port;    
    s->sourcePort = getEphemeralPort();
    uint8_t control = 1<<1; //SYN
    send_tcp_message(s,control,0,0);
}

void socket_destructor(system_handle* h)
{
    //No need to release buffers since they are part of the process and page tables will be destroyed
    //TODO: release locks
    socket* s = (socket*)h;

    pf("destroying socket\r\n");
}

void add_socket_in_list(socket* s)
{
    //TODO: should lock: multiple cpi/threads will use this.
    socket* first = *((socket**)SOCKETSLIST);

    s->next = 0;
    if (first==0)
    {
        s->previous = 0;
        *((socket**)SOCKETSLIST) = s;
    }
    else
    {
        while (first->next != 0) first = first->next;

        first->next =s;
        s->previous = first;
    }
}


void remove_socket_from_list(socket* s)
{
    socket *previous = s->previous;
    socket *next = s->next;

    //TODO: this should lock so that it would be multi-thread safe
    if (previous == 0)
    {
        *((socket**)SOCKETSLIST) = next;
        if (next != 0) next->previous = 0;
    }
    else
    {
        previous->next = next;
        if (next!=0) next->previous = previous;
    }
}

void destroy_sockets(uint64_t pid)
{
    socket* s = *((socket**)SOCKETSLIST);

    //TODO: this should lock so that it would be multi-thread safe
    while (s)
    {
        socket* victim = s;
        s = s->next;       

        if (victim->owner == pid)
        {
            victim->handle.destructor((system_handle*)victim);
            socket *previous = victim->previous;
            socket *next = victim->next;
            if (previous == 0)
            {
                *((socket**)SOCKETSLIST) = next;
                if (next != 0) next->previous = 0;
            }
            else
            {
                previous->next = next;
                if (next!=0) next->previous = previous;
            }
        }
    }
}

