#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "network.h"

int main(uint64_t param)
{
    uint64_t i;

    socket* s = create_socket();

    uint32_t ip = 0xFB01A8C0;    
    connect(s,ip,23);

    char r=0;
    while (!r) r = isconnected(s);
    if (r==-1)
    {
        printf("Connection refused\r\n");
    }
    else
    {
        printf("Connection established\r\n");

    
        char* buf = (char*)malloc(0x10000);

        i = 0x20000000;
        while (i) i--;
        uint16_t received = recv(s,buf,0x10000-1);
        if (received > 0)
        {  
            buf[received] = 0;
            printf("Got %x bytes: %s\r\n",received,buf);
        }
        send(s,"This is a test1",15);
        send(s,"This is a test2",15);
        i = 0x10000000;
        while (i) i--;
    
        close_socket(s);
        r=0;
        while (!r) r = isclosed(s);
    }

    printf("Closing sockets\r\n");
    close_socket(s);
    while (!isclosed(s));
    release_socket(s);
    printf("goodbye\r\n");
}
