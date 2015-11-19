#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "network.h"

int main(uint64_t param)
{
    uint64_t i;

    socket* s = create_socket();

    uint32_t ip = 0x0301A8C0;    
    connect(s,ip,25);

    char r=0;
    while (!r) r = isconnected(s);
    if (r==-1)
    {
        printf("Connection refused\r\n");
    }
    else
    {
        printf("Connection established\r\n");
    }

    i = 0x40000000;
    char* buf = (char*)malloc(0x10000);
    while (i) i--;

    uint16_t received = recv(s,buf,0x10000-1);
    if (received > 0)
    {  
        buf[received] = 0;
        printf("Got %x bytes: %s\r\n",received,buf);
    }

    close_socket(s);
    r=0;
    while (!r) r = isclosed(s);

    release_socket(s);

}
