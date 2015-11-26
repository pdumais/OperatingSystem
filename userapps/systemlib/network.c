#include "network.h"
#include "kernel/intA0.h"

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

void release_socket(socket* s)
{
    __asm("int $0xA0" : : "D"(s),  "a"(INTA0_RELEASE_SOCKET));
}

void connect(socket *s, uint32_t destination, uint16_t port)
{
    __asm("int $0xA0" : : "D"(s),"S"(destination),"d"(port), "a"(INTA0_CONNECT));
}

char isconnected(socket* s)
{
    if (s->tcp.state == SOCKET_STATE_RESET) return -1;
    if (s->tcp.state == SOCKET_STATE_CONNECTED) return 1;
    return 0;
}

char isclosed(socket* s)
{
    if (s->tcp.state == SOCKET_STATE_RESET) return -1;
    if (s->tcp.state == SOCKET_STATE_CLOSED) return 1;
    return 0;
}

int recv(socket* s, char* buffer, uint16_t max)
{
    int ret;
    __asm("int $0xA0" :"=a"(ret) : "D"(s),"S"(buffer),"d"(max), "a"(INTA0_RECV));
    return ret;
}

int send(socket* s, char* buffer, uint16_t length)
{
    int ret;
    __asm("int $0xA0" :"=a"(ret) : "D"(s),"S"(buffer),"d"(length), "a"(INTA0_SEND));
    return ret;
}

void listen(socket*s, uint32_t source, uint16_t port, uint16_t backlog)
{
    __asm("int $0xA0" : : "D"(s),"S"(source),"d"(port),"c"(backlog), "a"(INTA0_LISTEN));
}

socket* accept(socket* s)
{
    socket* ret;
    __asm("int $0xA0" :"=a"(ret) : "D"(s), "a"(INTA0_ACCEPT));
    return ret;
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
