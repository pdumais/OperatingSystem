#include "display.h"
#include "keyboard.h"
#include "utils.h"
#include "console.h"
#include "../memorymap.h"

#define MAX_CONSOLES 128

extern void* userAllocPages(uint64_t pageCount);
extern uint64_t getPTEntry(uint64_t virtual_address);
extern void memcpy64(char* source, char* dest, uint64_t size);
extern void memclear64(char* dest, uint64_t size);
extern void* currentProcessVirt2phys(void* address);
extern void mutexLock(uint64_t*);
extern void mutexUnlock(uint64_t*);
extern void rwlockWriteLock(uint64_t*);
extern void rwlockWriteUnlock(uint64_t*);
extern void rwlockReadLock(uint64_t*);
extern void rwlockReadUnlock(uint64_t*);
extern void* malloc(uint64_t size);

volatile uint64_t frontLineSwitchLock;
volatile uint64_t frontLineConsoleIndex;
volatile uint64_t requestedConsoleIndex;
struct ConsoleData* consoles[MAX_CONSOLES];  
uint64_t consoleListLock;

//
// When a thread is dead, it wont attempt to access its console's screen
// since this is done my thread code using functions such as printf.
// so a dead console is guaranteed not to use its output buffer
// 
// On keyboard input, if the current frontline process is a dead
// process, then we risk writing in a buffer that does not exist anymore
// since the process memory might have been destroyed.
// For this reason, before killing a process, we must unregister
// its console.
//


// This function could be called on 2 CPUs at the same time but not for the same
// console since there are 1 console per thread.
// but eventually, when a process can havle multiple thread, they will share the same
// console so we will have to protect that.
void streamCharacters(char* str)
{
    struct ConsoleData* cd = *((struct ConsoleData**)CONSOLE_POINTER);
    if (cd == 0) return;
    while (*str!=0)
    {
        char c = *str;
        if (cd->streamPointer>=512)
        {
            cd->flush_function(cd);
        }
   
        if (c==0x03) // unconventional use of ascii char 0x03 to flush buffer
        {
            cd->flush_function(cd);
        }
        else
        {
            cd->streamBuffer[cd->streamPointer] = c;
            cd->streamPointer++;
            if (c=='\n') cd->flush_function();
        }
        str++;  
    } 
}

void scroll(char* buffer)
{
    memcpy64((char*)&buffer[160],(char*)&buffer[0],(80*(25-1)*2));
    memclear64((char*)&buffer[2*80*(25-1)],2*80);
}

void increaseBufferPointer(struct ConsoleData* cd, uint64_t count, char* buffer)
{
    cd->backBufferPointer+=count;
    if (cd->backBufferPointer >= (80*2*25))
    {
        scroll(buffer);
        cd->backBufferPointer-=(80*2);
    }
}
void enableCursor(bool enabled)
{
    char tmp;
    OUTPORTB(0x0A,0x3D4);
    INPORTB(tmp,0x3D5)
    if (enabled)
    {
        tmp = (tmp & 0b11111); 
    }
    else
    {
        tmp = (tmp | 0b100000);
    }
    OUTPORTB(0x3D4, 0x0A);
    OUTPORTB(tmp,0x3D5);
}

void updateTextCursor(struct ConsoleData *cd)
{
    enableCursor(cd->cursorOn);
    if (cd->cursorOn)
    {
        uint16_t position = cd->backBufferPointer >> 1;
        OUTPORTB(0x0F,0x3D4);
        OUTPORTB((unsigned char)position, 0x3D5);
        OUTPORTB(0x0E, 0x3D4);
        OUTPORTB((unsigned char )(position>>8), 0x3D5);
    }
}


/////////////////////////////////////////////////////////////////////
// TODO: this could be written in ASM to increase performance
// Will handle:
//  cursror position
//  cursor move up,down,left,right
//  save/restore cursor
//  clear scren
//
/////////////////////////////////////////////////////////////////////
void handleANSI(struct ConsoleData *cd, char* buffer, char c)
{
    uint8_t i;
    cd->ansiData[cd->ansiIndex] = c;
    cd->ansiIndex++;

    if ((c>='a' && c <='z') || ((c>='A' && c <='Z')))
    {
        if (*((uint16_t*)cd->ansiData) == 0x5B1B)
        {
            if (c=='H' || c== 'f')
            {
                uint8_t num1 = 0;
                uint8_t num2 = 0;
                uint8_t* num = &num1;
                for (i=2;i<cd->ansiIndex-1;i++)
                {
                    char c = cd->ansiData[i];
                    if (c == ';')
                    {
                        num = &num2;
                    }
                    else if (c>='0'&&c<='9')
                    {
                        *num *= 10;
                        *num += (c-48);
                    }
                }
                if (num1<25 && num2<80) cd->backBufferPointer = (num1*160)+(num2*2);
            }
            else if (c=='A' || c=='B' || c=='C' || c=='D')
            {
                uint8_t num = 0;
                for (i=2;i<cd->ansiIndex-1;i++)
                {
                    char c = cd->ansiData[i];
                    if (c>='0'&&c<='9')
                    {
                        num = 10;
                        num = (c-48);
                    }
                }

                if (c=='A')
                {
                    cd->backBufferPointer -= (num*160);
                    if (cd->backBufferPointer > (80*25*2)) cd->backBufferPointer=0;
                }
                else if (c=='B')
                {
                    cd->backBufferPointer += (num*160);
                    if (cd->backBufferPointer > (80*25*2)) cd->backBufferPointer=(80*25*2)-2;
                }
                else if (c=='C')
                {
                    cd->backBufferPointer -= (num*2);
                    if (cd->backBufferPointer > (80*25*2)) cd->backBufferPointer=0;
                }
                else if (c=='D')
                {
                    cd->backBufferPointer += (num*2);
                    if (cd->backBufferPointer > (80*25*2)) cd->backBufferPointer=(80*25*2)-2;
                }


            }
            else if (c=='J')
            {
                if (*((uint32_t*)cd->ansiData) == 0x4A325B1B)
                {
                    cd->backBufferPointer = 0;
                    memclear64(buffer,80*25*2);
                }
            }
            else if (c=='s')
            {
                cd->ansiSavedPosition = cd->backBufferPointer;
            }
            else if (c=='u')
            {
                cd->backBufferPointer = cd->ansiSavedPosition;
            }
            else if (c=='h')
            {
                if (*((uint32_t*)&cd->ansiData[2]) == 0x6835323F) cd->cursorOn = true;
            }
            else if (c=='l')
            {
                if (*((uint32_t*)&cd->ansiData[2]) == 0x6c35323F) cd->cursorOn = false;
            }
        }
        cd->ansiIndex = 0;
    }

    if (cd->ansiIndex == 8)
    {
        cd->ansiIndex = 0;
    }

}


void flushTextVideo()
{
    struct ConsoleData* cd = *((struct ConsoleData**)CONSOLE_POINTER);
    unsigned int i;
    uint64_t currentThreadID;
    char c;    
    char isFrontLine;
    char* outputBuffer;

    rwlockReadLock(&frontLineSwitchLock);
    outputBuffer = cd->backBuffer;
    isFrontLine = 0;
    if (frontLineConsoleIndex != -1 && consoles[frontLineConsoleIndex] != 0)
    {
        uint64_t frontLineProcess = consoles[frontLineConsoleIndex]->owningProcess;
        __asm("mov %%cr3,%0" : "=r"(currentThreadID));
        currentThreadID &= 0x00FFFFFFFFFFF000LL;

        if (currentThreadID == frontLineProcess)
        {
            outputBuffer = 0xB8000;
            isFrontLine = 1;
        }
    }

    for (i=0;i<cd->streamPointer;i++)
    {
        c = cd->streamBuffer[i];
        if (c == 0x1B || cd->ansiIndex!=0)
        {
            handleANSI(cd, outputBuffer, c);
        }
        else if (c=='\r')
        {
            cd->backBufferPointer -= (cd->backBufferPointer % 160);
        }
        else if (c=='\n')
        {
            increaseBufferPointer(cd,160,outputBuffer);
        }
        else if (c=='\t')
        {
            increaseBufferPointer(cd,8,outputBuffer);
        }
        else if (c=='\t')
        {
            if (cd->backBufferPointer >= 2)
            {
                cd->backBufferPointer -= 2; 
                outputBuffer[cd->backBufferPointer]=0;
            }
        }
        else
        {
            outputBuffer[cd->backBufferPointer] = c;
            increaseBufferPointer(cd,2, outputBuffer);
        }
    }
    cd->streamPointer = 0;
    if (isFrontLine)
    {
        updateTextCursor(cd);
    }
    rwlockReadUnlock(&frontLineSwitchLock);
}


void initConsoles()
{
    int i;
    frontLineConsoleIndex = -1;
    requestedConsoleIndex = -1;
    frontLineSwitchLock = 0;
    consoleListLock = 0;
    for (i=0;i<MAX_CONSOLES;i++) consoles[i] = 0;
    enableCursor(false);
}

void storeCharacter(uint16_t c)
{
    // We switch console focus using F2-F12, but this only allows us to use 12 consoles.
    // TODO: should find another way to let user choose its console
    if (c>=KEY_F2 && c<=KEY_F12)
    {
        requestedConsoleIndex = c-KEY_F2;
        return;
    }

    if (frontLineConsoleIndex == -1) return;


    struct ConsoleData* cd = consoles[frontLineConsoleIndex];
    if (cd == 0) return;    // console has been removed
//TODO:  another CPU could have change frontlneConsoleIndex at this point. Does it matter?

    uint64_t n = (cd->kQueueIn+1)&0x0F;
    if (n==cd->kQueueOut) return;

    cd->keyboardBuffer[cd->kQueueIn] = c;
    cd->kQueueIn = n;
    
}

uint16_t pollChar()
{
    uint16_t ret;
    struct ConsoleData* cd = *((struct ConsoleData**)CONSOLE_POINTER);
    if (cd->kQueueOut == cd->kQueueIn) return 0;
    ret =  cd->keyboardBuffer[cd->kQueueOut];
    cd->kQueueOut = (cd->kQueueOut+1)&0x0F;
    return ret;
}


// This will give back the console to a process that got its
// console stolen by another process
void restoreTextConsole(uint64_t index, uint64_t processID)
{
}

// This will allow a process to takeover an existing console
uint64_t stealTextConsole(uint64_t procesID)
{
}

void createTextConsole()
{
    uint64_t i;
    struct ConsoleData** consoleDataPointer;
    consoleDataPointer = (struct ConsoleData**)CONSOLE_POINTER;

    //char* videoBuffer = (char*)userAllocPages(1);
    //struct ConsoleData* consoleInfo = (struct ConsoleData*)userAllocPages(1);
    char* videoBuffer = (char*)malloc(4096); //TODO: put appropriate size
    struct ConsoleData* consoleInfo = (struct ConsoleData*)malloc(4096); //TODO: put appropriate size
    *consoleDataPointer = consoleInfo;

    memclear64(videoBuffer,(2*80*25));
    consoleInfo->backBuffer = (char*)currentProcessVirt2phys(videoBuffer);
    //if (consoles[0] != 0) __asm("mov %0,%%rax; int $3" : : "r"(consoleInfo->backBuffer));
    consoleInfo->streamPointer = 0;
    consoleInfo->backBufferPointer = 0;
    consoleInfo->kQueueIn = 0;
    consoleInfo->kQueueOut = 0;
    consoleInfo->flush_function = &flushTextVideo;
    consoleInfo->lock = 0;
    consoleInfo->ansiIndex = 0;
    consoleInfo->ansiSavedPosition = 0;
    consoleInfo->cursorOn = true;
    __asm("mov %%cr3,%0" : "=r"(consoleInfo->owningProcess));
    consoleInfo->owningProcess &= 0x00FFFFFFFFFFF000LL;

    struct ConsoleData* entry = *((struct ConsoleData**)CONSOLE_POINTER);
    mutexLock(&consoleListLock);
    //TODO: must handle the case where no more consoles are available
    for (i=0;i<MAX_CONSOLES;i++)
    {
        if (consoles[i] == 0)
        {
            consoles[i] = (struct ConsoleData*)currentProcessVirt2phys((void*)entry);
            //__asm("mov %0,%%rax; int $3" : : "r"(consoles[i]));
            break;
        }
    }
    mutexUnlock(&consoleListLock);
}

void removeConsole()
{
    uint64_t i;
    struct ConsoleData* entry = *((struct ConsoleData**)CONSOLE_POINTER);
    mutexLock(&consoleListLock);
    for (i=0;i<MAX_CONSOLES;i++)
    {
        if (consoles[i] == (struct ConsoleData*)currentProcessVirt2phys((void*)entry))
        {
            // TODO: by setting this to zero, we prevent keyboard handler to 
            // write ti keyboard buffer when the frontline process is still
            // this process but its console was removed. But there is a window
            // where the keyboard has checked for zero and writes into the buffer.
            // if we set this to zero during that window, and we continue to
            // set the process as dead, and a 3rd cpu runs kernelmain and
            // destroys the memory of that process, then they keyboard handler 
            // will fault. Technically, this is impossible since the number
            // of instructions after this function, and the number of instructions
            // in the memory destruction of the process is greater than the keyboard 
            // handler. So the keyboard will have exited by the time that the
            // buffer gets destroyed. But this is non-deterministic. It 
            // would be a good thing to make 100% sure that this cannot happen.
            // CPU0                CPU1                CPU2
            // keyb_handler        ...                 ...
            // ...                 removeConsole       ...
            // ...                 set dead            ...
            // ...                 get scheduled out   ...
            // ...                 ...                 destroyMem
            // ERROR               ...
            // keyb_handler_end    ...
            // 
            // but by the time CPU1 sets process as dead and schedules it out,
            // keyboard handler will have terminated

            consoles[i] = 0;
            break;
        }
    }
    mutexUnlock(&consoleListLock);
}

void switchFrontLineProcessByIndex(uint64_t index)
{
    unsigned int i;
    if (index >= MAX_CONSOLES) return;
    if (consoles[index] == 0) return;

    rwlockWriteLock(&frontLineSwitchLock);
    if (frontLineConsoleIndex != -1 && consoles[frontLineConsoleIndex]!=0)
    {
        uint64_t oldIndex = frontLineConsoleIndex;
        memcpy64((char*)0xB8000,consoles[frontLineConsoleIndex]->backBuffer,(2*80*25));
        frontLineConsoleIndex = index;

    }
    else
    {
        frontLineConsoleIndex = index;
    }

    memcpy64(consoles[frontLineConsoleIndex]->backBuffer,(char*)0xB8000,(2*80*25));
    updateTextCursor(consoles[frontLineConsoleIndex]);
    rwlockWriteUnlock(&frontLineSwitchLock);
}


void manageConsoles()
{
    uint64_t index = requestedConsoleIndex;

    if (index != frontLineConsoleIndex)
    {
        switchFrontLineProcessByIndex(index);
    }
}

