#include "../types.h"
#include "systemhandle.h"

//WARNING: struct should be bigger than 4k
struct ConsoleData
{
    system_handle   handle;

    char*       backBuffer;
    uint64_t    streamPointer; 
    uint64_t    backBufferPointer; 
    char        streamBuffer[512];
    uint16_t    keyboardBuffer[64];
    uint64_t    kQueueIn;
    uint64_t    kQueueOut;
    uint64_t    owningProcess;
    uint64_t    previousOwningProcess;
    uint64_t    lock;
    void        (*flush_function)();
    char        ansiData[8];
    uint8_t     ansiIndex;
    uint16_t    ansiSavedPosition;
    bool        cursorOn;
} __attribute((packed))__;

void storeCharacter(uint16_t c);
uint16_t pollChar();

void flushTextVideo();
void streamCharacters(char* str);
