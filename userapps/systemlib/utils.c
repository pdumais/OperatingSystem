#include "utils.h"

uint64_t hexStringToNumber(char* st)
{
    uint64_t n = 0;
    while (*st!=0)
    {
        n = n << 4;
        char c = *st;
        if (c>='0' && c<='9') n|= (c-48);
        else if (c>='a' && c<='f') n|= (c-97+10);
        else if (c>='A' && c<='F') n|= (c-65+10);
        st++;
    }

    return n;
}
