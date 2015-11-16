#include "sockets.h"
#include "../memorymap.h"

void socket_destructor(system_handle* h);
void add_socket_in_list(socket* s);
void remove_socket_from_list(socket* s);



socket* create_socket()
{
    socket* s = (socket*)malloc(sizeof(socket));
    memclear64(s,sizeof(socket));

    s->handle.destructor = &socket_destructor;
    s->messages = (socket_message*)malloc(sizeof(socket_message)*MAX_RINGBUFFER_SOCKET_MESSAGE_COUNT);

    __asm("mov %%cr3,%0" : "=r"(s->owner));
    add_socket_in_list(s);

    return s;
}

void close_socket(socket* s)
{
    remove_socket_from_list(s);

    free((void*)s->messages);
    free((void*)s);
}

void connect(socket *s, uint32_t ip, uint16_t port)
{
}

uint64_t isconnected(socket* s)
{
    return 1;
}

void socket_destructor(system_handle* h)
{
    //No need to release buffers since they are part of the process and page tables will be destroyed
    //TODO: release locks
    socket* s = (socket*)h;

    pf("destroying socket\r\n");
}

void add_socket_in_list(socket* s)
{
    //TODO: should lock: multiple cpi/threads will use this.
    socket* first = *((socket**)SOCKETSLIST);

    s->next = 0;
    if (first==0)
    {
        s->previous = 0;
        *((socket**)SOCKETSLIST) = s;
    }
    else
    {
        while (first->next != 0) first = first->next;

        first->next =s;
        s->previous = first;
    }
}


void remove_socket_from_list(socket* s)
{
    socket *previous = s->previous;
    socket *next = s->next;

    //TODO: this should lock so that it would be multi-thread safe
    if (previous == 0)
    {
        *((socket**)SOCKETSLIST) = next;
        if (next != 0) next->previous = 0;
    }
    else
    {
        previous->next = next;
        if (next!=0) next->previous = previous;
    }
}

void destroy_sockets(uint64_t pid)
{
    socket* s = *((socket**)SOCKETSLIST);

    //TODO: this should lock so that it would be multi-thread safe
    while (s)
    {
        socket* victim = s;
        s = s->next;       

        if (victim->owner == pid)
        {
            victim->handle.destructor((system_handle*)victim);
            socket *previous = victim->previous;
            socket *next = victim->next;
            if (previous == 0)
            {
                *((socket**)SOCKETSLIST) = next;
                if (next != 0) next->previous = 0;
            }
            else
            {
                previous->next = next;
                if (next!=0) next->previous = previous;
            }
        }
    }
}

