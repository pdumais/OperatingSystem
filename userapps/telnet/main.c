#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "network.h"

int main(uint64_t param)
{
    uint64_t i;

    socket* s = create_socket();

    connect(s,"google.com",6687);

    while (!isconnected(s));

    close_socket(s);


}
