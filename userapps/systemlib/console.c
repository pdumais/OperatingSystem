#include "console.h"

#define INTA0_PRINTF 0x20
#define INTA0_POLL_IN 0x22

#define MAX_PRINTF_STR 1024
#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_end(v)     __builtin_va_end(v)
#define va_copy(d,s)  __builtin_va_copy(d,s)
typedef __builtin_va_list va_list;

unsigned long itoh(unsigned long v, char* outbuf,unsigned long digits)
{
    unsigned int i;
    for (i=0;i<digits;i++)
    {
        unsigned char c = (v >> ((digits-i-1)*4))&0b1111;
        if (c<10) c+=48; else c+=55;
        *outbuf=c;
        outbuf++;
    }
    *outbuf = 0;
    return digits;
}

unsigned long itoa(unsigned int v, char* outbuf)
{
    unsigned int digits=0;
    unsigned int i;
    unsigned int div = 1000000000;
    unsigned char sequenceStarted = 0;
    while (div)
    {
        i = v/div;
        v = v%div;
        div = div/10;
        if (sequenceStarted | i!=0)
        {
            *outbuf=(char)(i+48);
            outbuf++;
            sequenceStarted = 1;
            digits++;
        }
    }
    *outbuf = 0;
    return digits;
}

/////////////////////////////////////////////////////////////////////////////////////////
// WARNING: no protection. Resulting string must not go over 1024 bytes
// This file must be compiled with no SSE support otherwise it compiles a whole bunch
// of xmm transfers with will result in device not ready exception at every context
// switch.
/////////////////////////////////////////////////////////////////////////////////////////
void printf(char* fmt, ...)
{
    va_list list;
    va_start(list,fmt);

    int stIndex = 0;
    char st[MAX_PRINTF_STR];
    while (*fmt!=0)
    {
        if (*fmt!='%')
        {
            st[stIndex++] = *fmt;
        }
        else
        {
            fmt++;
            if (*fmt=='s')
            {
                char *sta = va_arg(list,char *);
                while (*sta!=0)
                {
                    st[stIndex++]=*sta;
                    sta++;
                }
            }
            else if (*fmt=='c')
            {
                char c = (char)va_arg(list,unsigned int);
                st[stIndex++]=c;
            }
            else if (*fmt=='i')
            {
                int long v = (unsigned int)va_arg(list,unsigned int);
                stIndex += itoa(v,(char*)&st[stIndex]);
            }
            else if (*fmt=='x')
            {
                unsigned long v = va_arg(list,unsigned long);
                if (v&0xFFFFFFFF00000000)   // 64bit
                {
                    stIndex += itoh(v,(char*)&st[stIndex],16);
                }
                else if (v&0xFFFF0000)   // 32bit
                {
                    stIndex += itoh(v,(char*)&st[stIndex],8);
                }
                else if (v&0xFF00)  //16bit
                {
                    stIndex += itoh(v,(char*)&st[stIndex],4);
                }
                else    // 8 bit
                {
                    stIndex += itoh(v,(char*)&st[stIndex],2);
                }
            }
            else if (*fmt=='X') // special case for 12 bytes display
            {
                unsigned long v = va_arg(list,unsigned long);
                stIndex += itoh(v,(char*)&st[stIndex],12);
            }
        }
        fmt++;
        if (stIndex>=(MAX_PRINTF_STR-1)) break;
    }
    st[stIndex] = 0;

    __asm("int $0xA0" : : "D"(st), "a"(INTA0_PRINTF));
    va_end(list);
}

uint16_t poll_in()
{
    uint16_t ret;
    __asm("int $0xA0" : "=a"(ret) : "a"(INTA0_POLL_IN));
    return ret;
}
