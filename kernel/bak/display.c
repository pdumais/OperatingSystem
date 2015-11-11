#define WIDTH 80
#define HEIGHT 25
#define COLOR 7
#define SCREENSIZE (WIDTH*HEIGHT*2)

#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_end(v)     __builtin_va_end(v)
#define va_copy(d,s)  __builtin_va_copy(d,s)
typedef __builtin_va_list va_list;

extern unsigned long getCurrentThreadID();
extern void disableCurrentThread();
extern void enableThread(unsigned long thread);
extern void yield();
extern mutexLock(unsigned long*);
extern mutexUnlock(unsigned long*);
extern unsigned long memcpy64(char* src, char* dest, unsigned long size);

struct VirtualScreen
{
    unsigned char   x;
    unsigned char   y;
    unsigned short  bof;
    char            buffer[SCREENSIZE];
};

struct VirtualScreen vscreen={0};
unsigned short currentPointer=0;
unsigned long displayMutex=0;
volatile unsigned long threadID=0;

void increaseY()
{
    unsigned short i;
    if (vscreen.y==(HEIGHT-1))
    {
        vscreen.bof = (vscreen.bof+(WIDTH*2))%SCREENSIZE;
        unsigned short index = (vscreen.bof-(WIDTH*2))%SCREENSIZE;
        memclear64((char*)&vscreen.buffer[index],WIDTH*2);
    }
    else
    {
        vscreen.y++;
    }
}

void increaseX()
{
    vscreen.x++;
    if (vscreen.x==WIDTH)
    {
        vscreen.x=0;
        increaseY();
    }
}

void _writeString(char *st)
{
    while (*st!=0)
    {
        if (*st == '\r')
        {
            vscreen.x = 0;
        }
        else if (*st=='\t')
        {
            increaseX();
            increaseX();
            increaseX();
        }
        else if (*st == '\n')
        {
            increaseY();
        }
        else
        {
            unsigned short index = (vscreen.bof+(vscreen.x + (vscreen.y*WIDTH))*2)%SCREENSIZE;
            vscreen.buffer[index]=*st;
            vscreen.buffer[index+1]=COLOR;
            increaseX();
        }
        st++;
    }
    enableThread(threadID);
}



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


// WARNING: there is noprotection here. If resulting string goes over 1000, you're deadbeef
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
                //TODO: show 16digits HEX value
            }
        }
        fmt++;
    }
    st[stIndex]=0;
    mutexLock(&displayMutex);
    _writeString((char*)&st[0]);
    mutexUnlock(&displayMutex);
    va_end(list);
}

void debug_writestring_dangerous(char *st)
{
    _writeString(st);
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

    _writeString((char*)&buf[0]);
}


void update_display()
{
    threadID = getCurrentThreadID();

    while(1)
    {
        //TODO: if an interrupt occurs during the next line, it could screw up the display
        unsigned short index = (vscreen.bof+(vscreen.x + (vscreen.y*WIDTH))*2)%SCREENSIZE;
        if (index != currentPointer)
        {
            currentPointer = index;
            memcpy64((char*)&vscreen.buffer[vscreen.bof],(char*)0xB8000,SCREENSIZE-vscreen.bof);
            if (vscreen.bof>0) memcpy64((char*)&vscreen.buffer[0],(char*)(0xB8000+(unsigned long)(SCREENSIZE-vscreen.bof)),vscreen.bof);
        }
     //   disableCurrentThread();
    //    yield();
    }
}
