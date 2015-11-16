#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"
#include "files.h"

int main(uint64_t param)
{
    uint64_t i;

    //TODO: get filename from params
    file_handle* f = fopen("01:/bootscript",0);
    if (f == 0)
    {
        printf("hexdump: File not found\r\n");
        return;
    }

    uint64_t size = fgetsize(f);
    char* buf = (char*)malloc(size);
    fread(f,size,buf);
    fclose(f);

    size = 64;
    for (i=0;i<size;i++)
    {
        if (i%16 == 0)
        {
            printf("\r\n%X: ",i);
        }
        printf("%x ",buf[i]);
    }

    printf("\r\n");
    free((void*)buf);
}
