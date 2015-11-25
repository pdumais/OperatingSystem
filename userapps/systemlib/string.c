#include "string.h"

bool strcompare(char* src, char* dst)
{
    while ((*src !=0) && (*dst !=0))
    {
        if ((*src)!=(*dst)) return false;
        src++;
        dst++;
    }

    if ((*src==0) && (*dst==0)) return true;
    return false;
}

size_t strfind(char* src, char token)
{
    size_t i =0;
    while (src[i]!=0 && src[i]!=token) i++;

    if (src[i]==0) return -1;
    return i;
}

void strcpy(char* src, char* dst)
{
    while (*src!=0)
    {
        *dst = *src;
        dst++;
        src++;
    }
    *dst = 0;
}

size_t strlen(char* src)
{
    size_t i=0;
    while (src[i] != 0) i++;
    return i;
}
