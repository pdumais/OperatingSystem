#include "includes/kernel/sockets.h"

typedef struct
{
    uint16_t source;
    uint16_t destination;
    uint32_t sequence;
    uint32_t acknowledgement;
    uint16_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_header;

socket* create_socket();
void close_socket(socket* s);
void destroy_sockets(uint64_t pid);
void connect(socket *s, uint32_t ip, uint16_t port);
void tcp_process(char* buffer, uint16_t size, uint32_t from, uint32_t to);
