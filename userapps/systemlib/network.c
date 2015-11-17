#include "network.h"

#define INTA0_CREATE_SOCKET 0x50
#define INTA0_CLOSE_SOCKET 0x51
#define INTA0_CONNECT 0x52
#define INTA0_ISCONNECTED 0x53

socket* create_socket()
{
    socket* ret;
    __asm("int $0xA0" : "=a"(ret) : "a"(INTA0_CREATE_SOCKET));
    return ret;
}

void close_socket(socket* s)
{
    __asm("int $0xA0" : : "D"(s),  "a"(INTA0_CLOSE_SOCKET));
}

void connect(socket *s, uint32_t destination, uint16_t port)
{
    __asm("int $0xA0" : : "D"(s),"S"(destination),"d"(port), "a"(INTA0_CONNECT));
}

bool isconnected(socket* s)
{
    return (s->tcp.connected != 0);
}

uint32_t atoip(char* addr)
{
    // TODO
}

void iptoa(uint32_t addr, char* buf)
{
    // TODO
}

void resolveDNS(char* host)
{
    // TODO
}
