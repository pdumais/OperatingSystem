#include "threads.h"
#include "console.h"
#include "memory.h"
#include "string.h"

int main(uint64_t param)
{
    char str[32];
    str[0] = 0;   
    
    getDateTime(str);
 
    printf("%s\r\n",str);
}
