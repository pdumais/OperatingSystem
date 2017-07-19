#include "console.h"
#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_end(v)     __builtin_va_end(v)
#define va_copy(d,s)  __builtin_va_copy(d,s)
typedef __builtin_va_list va_list;



// This could be highly optimized in ASM
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

void safeWriteString(char* st)
{
    streamCharacters(st);
}

// WARNING: there is noprotection here. If resulting string goes over 256, you're deadbeef
void pf(char * fmt,...)
{
    char st[256];
    unsigned long stIndex = 0;

    va_list list;
    va_start(list,fmt);

    unsigned long i=0;
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
            else if (*fmt=='X')
            {
                unsigned long v = va_arg(list,unsigned long);
                stIndex += itoh(v,(char*)&st[stIndex],16);
            }
        }
        fmt++;
    }
    st[stIndex]=0;

    safeWriteString((char*)&st[0]);
    va_end(list);
}

void debug_writestring_dangerous(char *st)
{
    streamCharacters(st);
}

void debug_writenumber_dangerous(unsigned long number)
{
    char buf[19];
    buf[0]='0';
    buf[1]='x';
    buf[18]=0;

    unsigned int i;
    for (i=0;i<16;i++)
    {
        unsigned char c = (number >> (i*4))&0b1111;
        if (c<10) c+=48; else c+=55;
        buf[17-i]=c;
    }

    streamCharacters((char*)&buf[0]);
}

