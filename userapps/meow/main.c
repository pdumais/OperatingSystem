#include "threads.h"
#include "console.h"

uint64_t testRWVar=0x123456789;
uint64_t testBSSVar;

int meow(uint64_t tid)
{
    // the terminating \003 is non-standard way to force flush
    printf("\033[?25l\033[s\033[16;6HRunning on CPU %x   %x\033[u\033[?25h\003",tid, testBSSVar);
}

int main(uint64_t param)
{
    uint16_t ch;

    printf("User Thread %x  %x\r\n",param, testRWVar);

    while (1)   
    {
        testBSSVar++;
        meow(getCurrentCPU());
        ch = poll_in();
        if (ch!=0)
        {
            if (ch == 0x1B)
            {
                printf("\033[2J\003");
            }
            else if (ch=='A')
            {
                printf("\033[1A\003");
            }
            else if (ch=='B')
            {
                printf("\033[1B\003");
            }
            else if (ch=='C')
            {
                printf("\033[1C\003");
            }
            else if (ch=='D')
            {
                printf("\033[1D\003");
            }
            else if (ch=='Q')
            {
                return 0;
            }
            else
            {
                printf("keypress: %c   [%x]\r\n",(char)ch,ch);
            }
        }
    }
    
}
