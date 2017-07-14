#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "network.h"
#include "files.h"

#define MAX_CLIENTS 10
#define BUF_SIZE 2048

socket* clients[MAX_CLIENTS];
char buf[BUF_SIZE];

void closeClient(socket* s)
{
    printf("closing socket %x\r\n",s);
    close_socket(s);
}

void addClient(socket* s)
{
    uint64_t i;

    for (i=0;i<MAX_CLIENTS;i++)
    {
        if (clients[i] == 0)
        {
            clients[i] = s;
            return;
        }
    }

    //TODO: should reset client
    release_socket(s);
}

int main(uint64_t param)
{
    uint64_t i;

    socket* s = create_socket();
    for (i=0;i<MAX_CLIENTS;i++) clients[i] = 0;

    listen(s,0x1C01A8C0,80,10);
    printf("listening\r\n");
    while (1)
    {
        socket* ns = accept(s);
        if (ns)
        {
            printf("new connection accepted\r\n");
            addClient(ns);
        } 

        for (i=0;i<MAX_CLIENTS;i++)
        {
            if (clients[i]==0) continue;
            if (isclosed(clients[i]))
            {
                printf("releasing socket %x\r\n",clients[i]);
                release_socket(clients[i]);
                clients[i]=0;
                continue;
            }
            if (!isconnected(clients[i])) continue;

            buf[0] = 0;
            int received = recv(clients[i],buf,BUF_SIZE);
            if (received == 0) continue;
            if (received < 0)
            {
                closeClient(clients[i]);
                continue;
            }

            //TODO: process buffer
            if (buf[0]=='G' && buf[1]=='E' && buf[2]=='T' && buf[3]==' ')
            {
                bool sent = false;
                uint64_t n;
                for (n=4;n<33;n++) if (buf[n]==' ') break;
                if (n<32)
                {

                    buf[n]=0;
                    buf[1]='0';
                    buf[2]='4';
                    buf[3]=':';
                    buf[4]='/';
                    file_handle *f = fopen((char*)&buf[1],0);
                    if (f!=0) 
                    {
                        uint64_t pos = 0;
                        sprintf(buf,200,"HTTP/1.1 200 OK\r\n" \
                            "Content-Length: %i\r\n" \
                            "Content-Type: text/html\r\n" \
                            "\r\n",fgetsize(f));
                        pos = strlen(buf);
                        while (1)
                        {
                            n = fread(f,1024,(char*)&buf[pos]);
                            if (n>0)
                            {
                                send(clients[i],buf,n+pos);
                            }
                            pos = 0;
                            if (n<1024) break;
                        }
                        sent = true;
                        fclose(f);
                    }
                }

                if (!sent)
                {
                    sprintf(buf,200,"HTTP/1.1 404 NOT FOUND\r\n" \
                            "Content-Length: 0\r\n" \
                            "Content-Type: text/html\r\n" \
                            "\r\n");
                    send(clients[i],buf,strlen(buf));
                }
            }
            closeClient(clients[i]);
        }
   
    }

    printf("Closing sockets\r\n");
    close_socket(s);
    release_socket(s);
    printf("goodbye\r\n");
}
