#include "systemhandle.h"

#define MTU 1500
#define MAX_RINGBUFFER_SOCKET_MESSAGE_COUNT 500

#define SOCKET_MESSAGE_SYN 0
#define SOCKET_MESSAGE_ACK 1
#define SOCKET_MESSAGE_SYNACK 2
#define SOCKET_MESSAGE_FIN 3
#define SOCKET_MESSAGE_PAYLOAD 4


//TODO: a linked list for the sockets can be slow to search.

typedef struct 
{
    uint32_t ackNumber;
    uint32_t seqNumber;
} tcp_state;

typedef struct
{
    uint16_t source;
    uint16_t destination;
    uint32_t sequence;
    uint32_t acknowledgement;
        unsigned char offset:4;
        unsigned char reserved:3;
        unsigned char ecn:3;
        unsigned char control:6;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_header;

struct _socket_message
{
    char payload[MTU];
    uint16_t size;
    uint16_t type;
};

typedef struct _socket_message socket_message;

struct _socket
{
    system_handle handle;
    tcp_state tcp;
    struct _socket* next;                       
    struct _socket* previous;                       
    
    uint32_t destinationIP;
    uint32_t sourceIP;
    uint16_t destinationPort;
    uint16_t sourcePort;
    uint32_t qin;
    uint32_t qout;
    uint16_t messageTypeWanted;
    uint64_t queueLock;
    uint64_t owner;
    socket_message* messages;
};

typedef struct _socket socket;

socket* create_socket();
void close_socket(socket* s);
void destroy_sockets(uint64_t pid);
void connect(socket *s, uint32_t ip, uint16_t port);
uint64_t isconnected(socket* s);
