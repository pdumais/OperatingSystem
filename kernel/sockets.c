#include "sockets.h"
#include "../memorymap.h"
#include "ip.h"
#include "netcard.h"

#define EPHEMERAL_START 32768
#define EPHEMERAL_END 65535

extern void* currentProcessVirt2phys(void* address);
void socket_destructor(system_handle* h);
void add_socket_in_list(socket* s);
void remove_socket_from_list(socket* s);
extern uint64_t atomic_increase_within_range(uint64_t* var,uint64_t start, uint64_t end);
extern uint16_t tcp_checksum(unsigned char* buffer, uint64_t bufsize, uint32_t srcBE, uint32_t dstBE);

volatile uint64_t ephemeralPort = EPHEMERAL_START;


//TODO: sockets should locked on usage
//          a socket my be used between threads, should we lock or leave the user to deal with that?
//          a socket might be used in softirq (on another CPU) while being used in a thread
//          a socket might get deleted as part of process detruction while softIRQ is using it

uint16_t getEphemeralPort()
{
    //TODO: just incrementing is bad, must make sure port is
    //      not used by another socket
    return (uint16_t)atomic_increase_within_range(&ephemeralPort,EPHEMERAL_START,EPHEMERAL_END);
}

void send_tcp_message(socket* s, uint8_t control, char* payload, uint16_t size)
{
    unsigned char tmp[1500];
    tcp_header *h = (tcp_header*)tmp;
    if (size>0)
    {
        unsigned char* payloadCopy = (unsigned char*)&tmp[sizeof(tcp_header)];
        memcpy64(payload,payloadCopy,size);
    }
    
    uint64_t sourceInterface = (uint64_t)net_getInterfaceIndex(__builtin_bswap32(s->sourceIP));

    h->source = __builtin_bswap16(s->sourcePort);
    h->destination = __builtin_bswap16(s->destinationPort);
    h->sequence = __builtin_bswap32(s->tcp.seqNumber);
    h->acknowledgement = __builtin_bswap32(s->tcp.ackNumber);
    h->flags = 0x5000|control;
    h->flags = __builtin_bswap16(h->flags);
    h->window = __builtin_bswap16(0x100);
    h->checksum = 0;

    //TODO: checksum does not pad
    h->checksum = tcp_checksum((unsigned char*)h, size + sizeof(tcp_header), __builtin_bswap32(s->sourceIP), __builtin_bswap32(s->destinationIP));
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
    s->sourceIP = config->ip; 
    
    s->tcp.state = SOCKET_STATE_CLOSED;
    s->destinationIP = ip;
    s->destinationPort = port;    
    s->sourcePort = getEphemeralPort();
    uint8_t control = 1<<1; //SYN
    send_tcp_message(s,control,0,0);
    s->tcp.state = SOCKET_STATE_WAITSYNACK;
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
        *((socket**)SOCKETSLIST) = (socket*)currentProcessVirt2phys(s);
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

// TODO: Warning: this function is dangerous since it returns a pointer to a socket. We must make
// sure that softIRQ is not using the socket when we attempt to delete it. Maybe we should lock
// the socket itself.
socket* find_socket(uint32_t sourceIP, uint32_t destinationIP, uint16_t sourcePort, uint16_t destinationPort)
{
    socket* s = *((socket**)SOCKETSLIST);

    //TODO: this should lock so that it would be multi-thread safe
    //TODO: we should use a hashlist to search faster.
    while (s)
    {
//__asm("int $3" : : "a"(destinationIP),"b"(sourceIP),"c"(destinationPort),"d"(sourcePort),"D"(s));
        if (s->destinationIP == destinationIP && s->sourceIP == sourceIP && 
            s->destinationPort == destinationPort && s->sourcePort == sourcePort)
        {
            return s;
        }
        s = s->next;
    }

    return 0;
}


void send_ack(socket* s)
{
    uint8_t control = 1<<4; //ACK
    send_tcp_message(s,control,0,0);
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//                          TCP Finite State Machine
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
void tcp_process_synack(socket* s, char* payload, uint16_t size)
{
    s->tcp.ackNumber++;
    s->tcp.seqNumber++;
    send_ack(s);
    s->tcp.state = SOCKET_STATE_CONNECTED;
}


void tcp_process_rstack(socket* s, char* payload, uint16_t size)
{
    s->tcp.state = SOCKET_STATE_RESET;
}

void tcp_process(char* buffer, uint16_t size, uint32_t from, uint32_t to)
{
    tcp_header *h = (tcp_header*)buffer;
    uint16_t flags = __builtin_bswap16(h->flags);
    uint16_t offset = (flags&0xF000)>>10; // >>12 * 4 so >>10
    char* payload = (char*)&buffer[offset];
    
    socket *s = find_socket(to,from,__builtin_bswap16(h->destination), __builtin_bswap16(h->source));
    if (s == 0) return;

    s->tcp.ackNumber = __builtin_bswap32(h->sequence);

    //TODO: support syn: we are getting a connection request
    //TODO: support ack: for packet ack
    //TODO: support disconnection (fin?)

    //TCP flags:
    // F: 0, S:1, R:2, P:3, A:4, U:5
    if ((flags&0b11111) == 0b010010)
    {
        tcp_process_synack(s,payload,size-offset);
    }
    else if ((flags&0b11111) == 0b010100)
    {
        tcp_process_rstack(s,payload,size-offset);
    }
}
