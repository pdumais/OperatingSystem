#include "types.h"
#include "systemhandle.h"
#define MTU 1500
#define MAX_RINGBUFFER_SOCKET_MESSAGE_COUNT 500

#define SOCKET_MESSAGE_SYN 0
#define SOCKET_MESSAGE_ACK 1
#define SOCKET_MESSAGE_SYNACK 2
#define SOCKET_MESSAGE_FIN 3
#define SOCKET_MESSAGE_PAYLOAD 4

#define SOCKET_STATE_NEW 0
#define SOCKET_STATE_WAITSYNACK 1
#define SOCKET_STATE_CONNECTED 2
#define SOCKET_STATE_CLOSING 4
#define SOCKET_STATE_LISTENING 5
#define SOCKET_STATE_CONNECTING 6
#define SOCKET_STATE_CLOSED 0x80
#define SOCKET_STATE_RESET 0x81


//TODO: a linked list for the sockets can be slow to search.
typedef struct 
{
    // nextExpectedSeq will be used in the ack number field.
    // it represents the the next expected sequence number we expext
    // to receive and acknowledges the previous packet received
    uint32_t nextExpectedSeq;   
    uint32_t seqNumber;
    uint8_t state;
} tcp_state;

typedef struct _socket_message socket_message;

struct _socket
{
    system_handle handle;
    tcp_state tcp;
    struct _socket* next;                       
    struct _socket* previous;                       
    struct _socket* backlog;                       
    
    uint16_t backlogSize;
    uint32_t destinationIP;
    uint32_t sourceIP;
    uint16_t destinationPort;
    uint16_t sourcePort;
    uint32_t qin;
    uint32_t qout;
    uint16_t messageTypeWanted;
    uint64_t queueLock;
    uint64_t owner;
    char* messages;
};

typedef struct _socket socket;
