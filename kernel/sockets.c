#include "sockets.h"
#include "../memorymap.h"
#include "ip.h"
#include "netcard.h"

#define EPHEMERAL_START 32768
#define EPHEMERAL_END 65535
#define MEMORY_POOL_COUNT 100

extern uint64_t atomic_increase_within_range(uint64_t* var,uint64_t start, uint64_t end);
extern uint16_t tcp_checksum(unsigned char* buffer, uint64_t bufsize, uint32_t srcBE, uint32_t dstBE);
extern void* currentProcessVirt2phys(void* address);
extern void* malloc(uint64_t size);
extern void free(void* buffer);


// TCP protocol implementation
// This implementation is far from being up to specs. 
// These sockets SHOULD NOT be used accross 2 user threads, which is a reasonable limitation.
//
//TODO: we should use memory pool to create temporary buffer in send_tcp_message
//TODO: if netcard buffer overflow, send() will return the number of bytes sent and the user
//      will have to send the rest. But that won't work because the tcp header was already sent
//      out so the checksum is built on the full payload. Also, the last packet sent had the
//      "more fragments" bit set so the other end is expecting the last packet of the series 
//      we can test this by simulating a netcard send failure
//
//TODO: sockets should locked on usage
//          a socket might be used in softirq (on another CPU) while being used in a thread
//          a socket might get deleted as part of process detruction while softIRQ is using it
//          a socket might get deleted by a thread  while softIRQ is using it. So the socket list
//            AND the socket itself should be locked
//TODO: should use timer to detect if socket stays in a state for too long 
//      for example: waiting for ack of fin
//TODO: outbound messages should be added in a queue so that we can retransmit upon not receiving ACK
//TODO: when receiving messages, we should place them in order since they get be received out
//      of order (check seq number). So the user must be guaranteed to get messages in the right
//      order.
//TODO: instead of sending ack alone, we should set the ack flag in the next segment to be sent out.
//      but if no segment are to be sent out, we should send the ack alone. We are basically 
//      implementing a TCP_NO_DELAY here.
//TODO: we cannot select the source interface of a socket. It is hardcoded to if0
//
//TCP stack limitations:
//    We do not verify checksum on ingress segments
//    We ignore acks, therefore not implementating retransmission. 
//    No timers to detect if socket stays in a state for too long.
//        We are basically open to SYN flood attacks    
//    Acks are sent in individual segments instead of piggy-backing
//        on other egress segments when they can
//    No effort is made to put ingress segments in order using the 
//        sequence number
//
// Closing a socket:
//    A user application must call close_socket(). That will initiate the FIN sequence and ultimately
//    change the socket state to CLOSED. But it will not release the socket. The socket will still 
//    exist. The user application must then call release_socket to remove it from the list.
//    Note that the user application must wait for the socket to be properly closed (SYN sequence
//    completed) before releasing the socket.

void socket_destructor(system_handle* h);
void add_socket_in_list(socket* s);
void remove_socket_from_list(socket* s);
void tcp_send_synack(socket* s);
void tcp_send_rst(socket* s);
void tcp_close(socket* s);

volatile uint64_t ephemeralPort = EPHEMERAL_START;
uint64_t socket_pool = -1;

typedef struct
{
    tcp_header header;
    char payload[65536-sizeof(tcp_header)];
} tcp_segment;

void tcp_init()
{
    socket_pool = create_memory_pool(sizeof(socket));
}

uint16_t getEphemeralPort()
{
    //TODO: just incrementing is bad, must make sure port is
    //      not used by another socket because we could wrap around and
    //      an old socket was still using that port. Also, it is a security issues
    //      if next port is sequential
    return (uint16_t)atomic_increase_within_range(&ephemeralPort,EPHEMERAL_START,EPHEMERAL_END);
}

int send_tcp_message(socket* s, uint8_t control, char* payload, uint16_t size)
{
    if (size>(65536-sizeof(tcp_header))) return 0;
    uint16_t segmentSize = sizeof(tcp_header)+size;
    uint16_t paddedSegmentSize = (segmentSize+1)&~1; // make size a multiple of 2 bytes

    // Doing a copy here will slow down things. We should pass down header
    // and payload. But then again, eventually, we should buffer those segments
    // to allow tcp retransmission. 
    //TODO: should use memory pool instead of malloc
    tcp_segment* segment = (tcp_segment*)malloc(sizeof(tcp_segment));

    if (segment == 0)
    {
        // hmmm, what should we do about that?
        __asm("int $3");
    }
    segment->payload[paddedSegmentSize-1] = 0;

    if (size>0)
    {
        memcpy64(payload,segment->payload,size);
    }
 
    uint64_t sourceInterface = (uint64_t)net_getInterfaceIndex(__builtin_bswap32(s->sourceIP));

    segment->header.source = __builtin_bswap16(s->sourcePort);
    segment->header.destination = __builtin_bswap16(s->destinationPort);
    segment->header.sequence = __builtin_bswap32(s->tcp.seqNumber);
    segment->header.acknowledgement = __builtin_bswap32(s->tcp.nextExpectedSeq);
    segment->header.flags = 0x5000|control;
    segment->header.flags = __builtin_bswap16(segment->header.flags);
    segment->header.window = __builtin_bswap16(0x100);
    segment->header.checksum = 0;

    segment->header.checksum = tcp_checksum(segment, segmentSize, __builtin_bswap32(s->sourceIP), __builtin_bswap32(s->destinationIP));
    int ret = ip_send(sourceInterface, s->destinationIP, segment, segmentSize, 6);

    free(segment);
    return (ret-sizeof(tcp_header));
}

socket* create_socket()
{
    socket* s = (socket*)malloc(sizeof(socket));
    memclear64(s,sizeof(socket));

    s->handle.destructor = &socket_destructor;

    __asm("mov %%cr3,%0" : "=r"(s->owner));
    add_socket_in_list(s);

    return s;
}

// This function will close the socket but will not release it.
// a user application can wait until the socket il closed successfully
// before releasing it.
void close_socket(socket* s)
{
    if (s->tcp.state == SOCKET_STATE_CONNECTED)
    {
        tcp_close(s);
    }
    else if (s->tcp.state == SOCKET_STATE_LISTENING)
    {
        s->tcp.state = SOCKET_STATE_CLOSED;
    }
}

void delete_socket(socket* s)
{
    remove_socket_from_list(s);
    if (s->backlog)
    {
        //TODO: should delete all those connections?
    }
    free((void*)s);
}

void release_socket(socket* s)
{
    if (s->tcp.state == SOCKET_STATE_CONNECTED)
    {
        tcp_send_rst(s);
    }

    delete_socket(s);
}

//TODO: This call is not trully non-blocking. The underlying ARP query
//      might block if entry not in cache
void connect(socket *s, uint32_t ip, uint16_t port)
{
    //TODO: The source IP should be given to the IP layer and let it
    //      find the interface number
    struct NetworkConfig* config = net_getConfig(0);
    if (config == 0) return;
    s->sourceIP = config->ip; 
    
    s->tcp.state = SOCKET_STATE_NEW;
    s->destinationIP = ip;
    s->destinationPort = port;    
    s->sourcePort = getEphemeralPort();
    uint8_t control = 1<<1; //SYN
    send_tcp_message(s,control,0,0);
    s->tcp.seqNumber++; // increase for next transmission
    s->tcp.state = SOCKET_STATE_WAITSYNACK;
}

void listen(socket*s, uint32_t source, uint16_t port, uint16_t backlog)
{
    //TODO: make sure that the source port is not in use by something else
    memclear64(s->backlog,sizeof(socket_info)*backlog);
    s->backlogSize = backlog;
    s->destinationIP = 0;
    s->destinationPort = 0;
    s->sourcePort= port;
    s->sourceIP = source;
    s->tcp.state = SOCKET_STATE_LISTENING;
}

socket* accept(socket*s)
{
    // No need to lock the backlog list because the softirq is inserting sockets in it
    // and this is retrieving them. There will be no conflicting access
    // no need to lock the socket that resides in the list because
    // it won't be in use by any other thread (since it was not retrieved
    // from backlog). The softIRQ might change the state (if peer disconnected)
    // or might add messages in it but it won't conflict with what we are
    // doing here. 
    // TODO: there is a window, in the softIRQ where the socket might not be found
    //       in the main list and won't be found in backlog either because we removed it here.

    uint16_t i;
    for (i = 0; i < s->backlogSize; i++)
    {
        if (s->backlog[i].tcp.state == SOCKET_STATE_CONNECTING)
        {
            socket* s2 = (socket*)malloc(sizeof(socket));
            memclear64(s2,sizeof(socket));
            s2->handle.destructor = &socket_destructor;

            s2->tcp.seqNumber = 0; // This should be random
            s2->tcp.nextExpectedSeq = s->backlog[i].tcp.nextExpectedSeq; 
            s2->tcp.state = SOCKET_STATE_CONNECTING; 
            s2->sourceIP = s->backlog[i].sourceIP;
            s2->sourcePort = s->backlog[i].sourcePort;
            s2->destinationIP = s->backlog[i].destinationIP;
            s2->destinationPort = s->backlog[i].destinationPort;
            __asm("mov %%cr3,%0" : "=r"(s2->owner));
            add_socket_in_list(s2);

            tcp_send_synack(s2);
            s2->tcp.seqNumber++;
            s->backlog[i].tcp.state = 0;
            return s2;
        }
    }

    return 0;
}

void socket_destructor(system_handle* h)
{
    // No need to release buffers since they are part of the process 
    // and page tables will be destroyed

    socket* s = (socket*)h;
    // TODO: release locks if any
    // TODO: if this is a listening socket, we should destroy the backlog

    pf("destroying socket\r\n");
}

void add_socket_in_list(socket* s)
{
    //TODO: should lock: multiple cpu/threads will use this.
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

        first->next = (socket*)currentProcessVirt2phys(s);
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

int send(socket* s, char* buffer, uint16_t length)
{
    if (s->tcp.state != SOCKET_STATE_CONNECTED) return -1;
    uint16_t sent = send_tcp_message(s, (1<<4), buffer, length);
    s->tcp.seqNumber+=sent;

    if (sent!=length)
    {
        //TODO: we must support this. But right now, the ip layer
        //      wont work because fragmentation will be restarted on 
        //      next resume. Also, the TCP has been sent with the 
        //      checksum reflecting the payload size. so we can't
        //      just send another segment with the rest of the data
      __asm("int $3");
    }

    return sent;
}

int recv(socket* s, char* buffer, uint16_t max)
{
    if (s->tcp.state != SOCKET_STATE_CONNECTED) return -1;
    if (s->qin == s->qout) return 0;

    //TODO: should do this in assembly
    uint16_t ret = 0;
    while (ret!=max)
    {
        buffer[ret] = s->receivedSegments[s->qout];
        ret++;
        s->qout++;
        if (s->qout >= RING_BUFFER_SIZE) s->qout = 0;
        if (s->qout == s->qin) break;
    }

    return ret;
}

// Warning: this function is dangerous since it returns a pointer to a socket. We must make
// sure that softIRQ is not using the socket when we attempt to delete it. Maybe we should lock
// the socket itself.
socket* find_socket(uint32_t sourceIP, uint32_t destinationIP, uint16_t sourcePort, uint16_t destinationPort)
{
    socket* s = *((socket**)SOCKETSLIST);

    //TODO: this should lock so that it would be multi-thread safe
    //TODO: we should use a hashlist to search faster.
    while (s)
    {
        if (s->destinationIP == destinationIP && s->sourceIP == sourceIP && 
            s->destinationPort == destinationPort && s->sourcePort == sourcePort)
        {
            return s;
        }
        s = s->next;
    }

    return 0;
}

// Warning: this function is dangerous since it returns a pointer to a socket. We must make
// sure that softIRQ is not using the socket when we attempt to delete it. Maybe we should lock
// the socket itself.
socket* find_listening_socket(uint32_t sourceIP, uint16_t sourcePort)
{
    socket* s = *((socket**)SOCKETSLIST);

    //TODO: this should lock so that it would be multi-thread safe
    //TODO: we should use a hashlist to search faster.
    while (s)
    {
        if (s->tcp.state == SOCKET_STATE_LISTENING)
        {
            if (s->destinationIP == 0 && s->sourceIP == sourceIP &&
                s->destinationPort == 0 && s->sourcePort == sourcePort)
            {
                return s;
            }
        }
        s = s->next;
    }

    return 0;
}


void tcp_send_synack(socket* s)
{
    uint8_t control = (1<<4)|(1<<1); //SYNACK
    send_tcp_message(s,control,0,0);
}

void tcp_send_ack(socket* s)
{
    uint8_t control = 1<<4; //ACK
    send_tcp_message(s,control,0,0);
}

void tcp_send_fin(socket* s)
{
    uint8_t control = (1<<0)|(1<<4); //FIN
    send_tcp_message(s,control,0,0);
}

void tcp_send_rst(socket* s)
{
    uint8_t control = 1<<2; //RST
    send_tcp_message(s,control,0,0);
}

void tcp_close(socket* s)
{
    s->tcp.state = SOCKET_STATE_CLOSING;
    tcp_send_fin(s);
    s->tcp.seqNumber++;    
}

// this function does not lock. The socket must be locked before calling this.
void tcp_add_segment_in_queue(socket* s,char* payload,uint16_t size)
{
    // There is no need to save the size in the queue. TCP is a stream so
    // the data here should just appear as continuous stream


    //TODO: should do this in assembly
    while (size)
    {
        s->receivedSegments[s->qin] = *payload;
        payload++;
        size--;
        s->qin++;
        if (s->qin >= RING_BUFFER_SIZE) s->qin = 0;
        if (s->qin == s->qout)
        {
            // TODO: BUFFER OVERRUN!!! close connection
            __asm("int $3");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//                          TCP Finite State Machine
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
void tcp_process_syn(socket* s, char* payload, uint16_t size, 
                     tcp_header* h,uint32_t from)
{
    if (s->tcp.state == SOCKET_STATE_LISTENING)
    {
        // No need to lock backlog list because the softirq is the only producer.
        uint16_t slot;
        for (slot = 0; slot < s->backlogSize; slot++) if (s->backlog[slot].tcp.state == 0) break;
        if (slot == s->backlogSize)
        {
            //TODO: send rst. backlog is full
            return;    
        } 

        //__asm("int $3");
        socket_info* s2 = &(s->backlog[slot]);
        s2->tcp.state = SOCKET_STATE_CONNECTING;    
        s2->sourceIP = s->sourceIP;
        s2->sourcePort = s->sourcePort;
        s2->destinationIP = from;
        s2->destinationPort = __builtin_bswap16(h->source);
        s2->tcp.nextExpectedSeq = __builtin_bswap32(h->sequence)+1; // will be ready for when we send synack
    }
    else if (s->tcp.state == SOCKET_STATE_WAITSYNACK)
    {
        s->tcp.nextExpectedSeq = __builtin_bswap32(h->sequence)+1;
        tcp_send_ack(s);
        s->tcp.state = SOCKET_STATE_CONNECTED;
    }
}

void tcp_process_rst(socket* s, char* payload, uint16_t size)
{
    //TODO: what if we receive this for a socket in the backlog of a listening socket?
    if (s->tcp.state == SOCKET_STATE_LISTENING)
    {
        // bogus
    }
    else
    {
        s->tcp.state = SOCKET_STATE_RESET;
    }
}

void tcp_process_ack(socket* s, char* payload, uint16_t size)
{
    if (s->tcp.state == SOCKET_STATE_LISTENING)
    {
        // bogus
    }
    else if (s->tcp.state == SOCKET_STATE_CONNECTING)
    {
        // the socket had sent a SYN/ACK and now it just received the ack
        s->tcp.state = SOCKET_STATE_CONNECTED;
    }
    else
    {
        // we should process that as part of retransmission handling.
    }
}

void tcp_process_fin(socket* s, char* payload, uint16_t size)
{
    // If we are connected, then send ack and then send fin
    s->tcp.nextExpectedSeq++;
    if (s->tcp.state == SOCKET_STATE_CONNECTED)
    {
        // if we get a FIN while connected, then the peer
        // decided to close the connection. We must send
        // a ACK and a FIN. It will send us a ack back 
        tcp_send_fin(s);
        s->tcp.seqNumber++;
        // we won't wait for the ack. Although we should.
        s->tcp.state = SOCKET_STATE_CLOSED;
        return;
    }
    else if (s->tcp.state == SOCKET_STATE_CLOSING)
    {
        // if we get FIN while closing, it means that
        // we sent the FIN first and then they send it
        // back to us (with a ack before).        
        tcp_send_ack(s);
        s->tcp.state = SOCKET_STATE_CLOSED;
        return;
    }
    else if (s->tcp.state == SOCKET_STATE_LISTENING)
    {
        // bogus
    }

    s->tcp.state = SOCKET_STATE_RESET;
}

void tcp_process(char* buffer, uint16_t size, uint32_t from, uint32_t to)
{
    uint16_t i;
    tcp_header *h = (tcp_header*)buffer;
    uint16_t flags = __builtin_bswap16(h->flags);
    uint16_t offset = (flags&0xF000)>>10; // SHR 12 * 4 = SHR 10
    char* payload = (char*)&buffer[offset];
    size = size-offset;

    socket *s = find_socket(to,from,__builtin_bswap16(h->destination), __builtin_bswap16(h->source));
    if (s == 0)
    {
        // The socket was not found. Maybe the segment was destined to a listening socket
        // or a socket in the backlog of a listening socket.
        s = find_listening_socket(to,__builtin_bswap16(h->destination));
    }
    //TODO: between here and the end of the function, is a window where a user could delete the socket
    //      so this is very dangerous
    if (s == 0) return;
    if (s->tcp.state >= 0x80) return;


    //TCP flags:
    // F: 0, S:1, R:2, P:3, A:4, U:5
    if ((flags&0b00010) == 0b000010)
    {
        tcp_process_syn(s,payload,size-offset,h,from);
    }

    if ((flags&0b00100) == 0b000100)
    {
        tcp_process_rst(s,payload,size-offset);
    }

    if ((flags&0b10000) == 0b010000)
    {
        tcp_process_ack(s,payload,size-offset);
    }

    if ((flags&0b1) == 0b1)
    {
        tcp_process_fin(s,payload,size-offset);
    }

    if (size > 0)
    {
        tcp_add_segment_in_queue(s,payload,size);

        //TODO: an ack might have been sent already if the FIN flag was set.
        s->tcp.nextExpectedSeq = __builtin_bswap32(h->sequence)+size;
        tcp_send_ack(s);
    }
}
