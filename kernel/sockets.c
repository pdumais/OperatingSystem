#include "sockets.h"
#include "../memorymap.h"
#include "ip.h"
#include "netcard.h"

#define EPHEMERAL_START 32768
#define EPHEMERAL_END 65535
#define SOCKETBUFFERSIZE (1500*MAX_RINGBUFFER_SOCKET_MESSAGE_COUNT)
extern uint64_t atomic_increase_within_range(uint64_t* var,uint64_t start, uint64_t end);
extern uint16_t tcp_checksum(unsigned char* buffer, uint64_t bufsize, uint32_t srcBE, uint32_t dstBE);
extern void* currentProcessVirt2phys(void* address);

// TCP protocol implementation
// This implementation is far from being up to specs. 
// These sockets SHOULD NOT be used accross 2 user threads.
//
//TODO: should use timer to detect if socket stays in a state for too long 
//      for example: waiting for ack of fin
//TODO: sockets should locked on usage
//          a socket might be used in softirq (on another CPU) while being used in a thread
//          a socket might get deleted as part of process detruction while softIRQ is using it
//          a socket might get deleted by a thread  while softIRQ is using it. So the socket list
//            AND the socket itself should be locked
//TODO: outbound messages should be added in a queue so that we can retransmit upon not receiving ACK
//TODO: when receiving messages, we should place them in order since they get be received out
//      of order (check seq number). So the user must be guaranteed to get messages in the right
//      order.
//TODO: we need a timer to delete socket in case we sent FIN, as part of close() and we never get
//      any reply. Because in that case, we would delete the socket only when receiving ack for fin
//TODO: instead of sending ack alone, we should set the ack flag in the next segment to be sent out.
//      but if no segment are to be sent out, we should send the ack alone. We are basically 
//      implementing a TCP_NO_DELAY here.
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
// A user application must call close_socket(). That will initiate the FIN sequence and ultimately
// change the socket state to CLOSED. But it will not release the socket. The socket will still 
// exist. The user application must then call release_socket to remove it from the list.
// Note that the user application must wait for the socket to be properly closed (SYN sequence
// completed) before releasing the socket.

void socket_destructor(system_handle* h);
void add_socket_in_list(socket* s);
void remove_socket_from_list(socket* s);

volatile uint64_t ephemeralPort = EPHEMERAL_START;


uint16_t getEphemeralPort()
{
    //TODO: just incrementing is bad, must make sure port is
    //      not used by another socket
    return (uint16_t)atomic_increase_within_range(&ephemeralPort,EPHEMERAL_START,EPHEMERAL_END);
}

int send_tcp_message(socket* s, uint8_t control, char* payload, uint16_t size)
{
    //TODO: doing a copy here will slow down things. We should pass down header
    //      and payload. But then again, eventually, we should buffer those segments
    //      to allow tcp retransmission.
    //      We should at least go through an object pool instead of using malloc
    uint16_t segmentSize = sizeof(tcp_header)+size;
    uint16_t paddedSegmentSize = (segmentSize+1)&~1; // make size a multiple of 2 bytes

    char* tmp = (char*)malloc(paddedSegmentSize);
    tmp[paddedSegmentSize-1] = 0;

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
    h->acknowledgement = __builtin_bswap32(s->tcp.nextExpectedSeq);
    h->flags = 0x5000|control;
    h->flags = __builtin_bswap16(h->flags);
    h->window = __builtin_bswap16(0x100);
    h->checksum = 0;

    //TODO: checksum does not pad
    h->checksum = tcp_checksum((unsigned char*)tmp, segmentSize, __builtin_bswap32(s->sourceIP), __builtin_bswap32(s->destinationIP));
    int ret = ip_send(sourceInterface, s->destinationIP, tmp, segmentSize, 6);

    free(tmp);
    return (ret-sizeof(tcp_header));
}

socket* create_socket()
{
    socket* s = (socket*)malloc(sizeof(socket));
    memclear64(s,sizeof(socket));

    s->handle.destructor = &socket_destructor;
    s->messages = (socket_message*)currentProcessVirt2phys(malloc(SOCKETBUFFERSIZE));

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
}

void delete_socket(socket* s)
{
    remove_socket_from_list(s);
    free((void*)s->messages);
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
    s->tcp.seqNumber++; // increase for next transmission
    s->tcp.state = SOCKET_STATE_WAITSYNACK;
}

void socket_destructor(system_handle* h)
{
    // No need to release buffers since they are part of the process 
    // and page tables will be destroyed
    // TODO: we should send a rst
    // TODO: release locks
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

uint16_t send(socket* s, char* buffer, uint16_t length)
{
    uint16_t sent = send_tcp_message(s, (1<<4), buffer, length);
    s->tcp.seqNumber+=sent;

    if (sent!=length)
    {
        //TODO: we must support this. But right now, the ip layer
        //      wont work because fragmentation will be restarted on 
        //      next resume. Or does it matter?
      __asm("int $3");
    }

    return sent;
}

uint16_t recv(socket* s, char* buffer, uint16_t max)
{
    if (s->qin == s->qout) return 0;

    //TODO: should do this in assembly
    uint16_t ret = 0;
    while (ret!=max)
    {
        buffer[ret] = s->messages[s->qout];
        ret++;
        s->qout++;
        if (s->qout >= SOCKETBUFFERSIZE) s->qout = 0;
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
    // no need to increase ackNumber or seqNumber
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
        s->messages[s->qin] = *payload;
        payload++;
        size--;
        s->qin++;
        if (s->qin >= SOCKETBUFFERSIZE) s->qin = 0;
        if (s->qin == s->qout)
        {
            // TODO: BUFFER OVERRUN!!! close connection
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//                          TCP Finite State Machine
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
void tcp_process_syn(socket* s, char* payload, uint16_t size, uint32_t seq)
{
    s->tcp.nextExpectedSeq = seq+1;
    tcp_send_ack(s);
    s->tcp.state = SOCKET_STATE_CONNECTED;
}

void tcp_process_rst(socket* s, char* payload, uint16_t size)
{
    s->tcp.state = SOCKET_STATE_RESET;
}

void tcp_process_ack(socket* s, char* payload, uint16_t size)
{
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

    s->tcp.state = SOCKET_STATE_RESET;
}

void tcp_process(char* buffer, uint16_t size, uint32_t from, uint32_t to)
{
    tcp_header *h = (tcp_header*)buffer;
    uint16_t flags = __builtin_bswap16(h->flags);
    uint16_t offset = (flags&0xF000)>>10; // SHR 12 * 4 = SHR 10
    char* payload = (char*)&buffer[offset];
    size = size-offset;

    socket *s = find_socket(to,from,__builtin_bswap16(h->destination), __builtin_bswap16(h->source));
    //TODO: between here and the end of the function, is a window where a user could delete the socket
    //      so this is very dangerous
    if (s == 0) return;
    if (s->tcp.state >= 0x80) return;

    //TCP flags:
    // F: 0, S:1, R:2, P:3, A:4, U:5
    if ((flags&0b00010) == 0b000010)
    {
        tcp_process_syn(s,payload,size-offset,__builtin_bswap32(h->sequence));
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
