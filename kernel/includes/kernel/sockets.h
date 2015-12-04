#include "types.h"
#include "systemhandle.h"
#include "hashtable.h"

#define RING_BUFFER_SIZE (6*1024)
#define MAX_BACKLOG 10              // should allow more

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

typedef struct
{
    tcp_state tcp;
    uint32_t destinationIP;
    uint32_t sourceIP;
    uint16_t destinationPort;
    uint16_t sourcePort;

} socket_info;

typedef struct
{
    uint32_t destinationIP;
    uint32_t sourceIP;
    uint16_t destinationPort;
    uint16_t sourcePort;
    uint32_t paddingTo64BitBoundary;
} socket_description;

struct _socket
{
    system_handle handle;
    tcp_state tcp;
    socket_info backlog[MAX_BACKLOG];                       

    socket_description desc;   
    uint16_t backlogSize;
    uint32_t qin;
    uint32_t qout;
    uint16_t messageTypeWanted;
    uint64_t queueLock;
    uint64_t owner;
    hashtable_node hash_node;
    char receivedSegments[RING_BUFFER_SIZE];
};

typedef struct _socket socket;
