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
    connect(s,ip,80);

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

    close_socket(s);


}
