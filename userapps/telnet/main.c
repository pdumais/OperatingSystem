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
    connect(s,ip,6687);

    while (!isconnected(s));

    close_socket(s);


}
