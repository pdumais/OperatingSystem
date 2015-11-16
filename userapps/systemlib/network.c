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

void connect(socket *s, char* host, uint16_t port)
{
    //TODO: net to resolve host or convert IP to uint32_t

    uint32_t ip = 0x0301A8C0;    
    __asm("int $0xA0" : : "D"(s),"S"(ip),"d"(port), "a"(INTA0_CONNECT));
}

bool isconnected(socket* s)
{
    bool ret;
    __asm("int $0xA0" : "=a"(ret) : "D"(s),"a"(INTA0_ISCONNECTED));
    return ret;
}
