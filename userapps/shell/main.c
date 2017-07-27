#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"

char command[255];
unsigned char quit = 0;


void processCommand()
{
    char* cmd = command;
    char* params=0;

    size_t t1 = strfind(command,' ');
    if (t1>-1)
    {
        cmd[t1] = 0;
        params = (char*)&command[t1+1];
    }

    t1 = strfind(params,' ');
    if (t1>-1) params[t1]=0;

    if (strcompare(cmd,"exit"))
    {
        quit = 1;
        return;
    }
    else if (strcompare(cmd,"load"))
    {
        char fname[255];
        strcpy("04:/",fname);
        strcpy(params,(char*)&fname[4]);
        printf("Loading [%s]\r\n",fname);
        uint64_t pid = loadProcess(fname);
        if (pid == 0)
        {
            printf("ERROR: Application not found\r\n");
        }
        else
        {
            waitForProcessDeath(pid);
        }

        return;
    }

    printf("ERROR: Command not found\r\n");
}


int main(uint64_t param)
{
    uint64_t cmdIndex = 0;
    char ch;
    
    //TODO: this is just a test
    uint64_t* test,test2;

    printf("shell> \003");
    while (quit == 0)
    {
        //TODO: this is just a test
        ch = poll_in();

        if ((ch>='0' && ch<='9')||(ch>='a' && ch<='z')||(ch>='A' && ch<='Z')||(ch==' ')||(ch=='.')) 
        {
            if (cmdIndex <255)
            {
                command[cmdIndex] = ch;
                cmdIndex++;
                printf("%c\003",ch);
            }
        }
        else if (ch==0x08)
        {
            if (cmdIndex > 0)
            {
                printf("%c %c\003",ch,ch);
                cmdIndex--;
            }
        }
        else if (ch==0x0A)
        {
            printf("\r\n");
            if (cmdIndex != 0)
            {
                command[cmdIndex]=0;
                processCommand();
                cmdIndex = 0;
            }
            printf("shell> \003");
        }
    }

    printf("\r        \r\nShell terminated\r\n");
}
