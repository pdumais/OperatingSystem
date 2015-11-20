#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "network.h"

int main(uint64_t param)
{
    uint64_t i;

    socket* s = create_socket();

    /*uint32_t ip = 0x0301A8C0;    
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
    }*/

    listen(s,0x1C01A8C0,242,2);
    printf("listening\r\n");
    socket *s2 = 0;
    while (!s2)
    {
        s2 = accept(s);
    }
    printf("Accepted\r\n");
    while (!isconnected(s2));
    printf("connected\r\n");
    send(s2,"Hello!\r\n",8);

    char cmd[255];
    char rbuf[255];
    uint8_t index = 0;
    cmd[0] = 0;
   
    char ch; 
    while (!strcompare(cmd,"quit"))
    {
        ch = poll_in();
        if ((ch>='0' && ch<='9')||(ch>='a' && ch<='z')||(ch>='A' && ch<='Z')||(ch==' ')) 
        {
            cmd[index] = ch;
            index++;
            cmd[index]=0;
            printf("\r%s\003",cmd);
        }
        else if (ch == 0x0A)
        {
            send(s2,cmd,index);
            index = 0;
            cmd[index]=0;
            printf("\r\n");
        }
        int received = recv(s2,rbuf,254);
        if (received == -1)
        {
            printf("Connection closed by peer\r\n");
            break;
        }
        else if (received > 0)
        {
            rbuf[received] = 0;
            printf("[%s]\r\n",rbuf);
        }
        
    }

    printf("Closing sockets\r\n");
    close_socket(s);
    close_socket(s2);
    while (!isclosed(s) || !isclosed(s2));
    release_socket(s2);
    release_socket(s);
    printf("goodbye\r\n");
}
