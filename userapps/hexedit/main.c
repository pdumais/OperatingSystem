#include "threads.h"
#include "console.h"
#include "utils.h"

#define WIDTH 16
#define HEIGHT 20

#define KEY_PGUP   278
#define KEY_PGDOWN 279
#define KEY_UP 280
#define KEY_DOWN 281
#define KEY_LEFT 282
#define KEY_RIGHT 283

struct Context
{
    void (*updateCursor)();
    void (*processBrowseKey)(uint16_t);
};

uint64_t baseAddress = 0x08000000;
uint64_t currentByte = 0x08000000;
uint64_t currentNibble = 0;
uint64_t currentCommandOffset = 0;
struct Context* currentContext;
char command[128];
struct Context hexContext;
struct Context asciiContext;
struct Context commandContext;

void changeContext(struct Context* ctx)
{
    currentContext = ctx;
    ctx->updateCursor();
}

void updateMemoryDisplay()
{
    uint64_t i,n;
    char line[81];
    uint64_t address = baseAddress;    

    printf("\033[1;0H");
    for (i=0;i<HEIGHT;i++)
    {
        char* lineBuffer = (char*)&line[0];
        for (n=0;n<WIDTH;n++)
        {
            char byte = *((char*)(address+n));
            char n1 = byte&0x0F;
            char n2 = (byte>>4)&0x0F;
            char d1 = '0'+n1;
            char d2 = '0'+n2;
            if (n1>9) d1 +=7;
            if (n2>9) d2 +=7;

            *lineBuffer = d2;
            *(lineBuffer+1) = d1;
            *(lineBuffer+2) = ' ';
            lineBuffer+=3;
        }
        *lineBuffer=' ';
        lineBuffer++;
        for (n=0;n<WIDTH;n++)
        {
            char byte = *((char*)(address+n));
            if (byte<32 || byte==127 || byte==255) byte='.';
            *lineBuffer = byte;
            lineBuffer++;
        }
        *lineBuffer = 0;
        printf("%X: %s\r\n",address, line);
        address += WIDTH;
    }

//TODO: will need virt2phys
}

void updateHexCursor()
{
    unsigned char col;
    unsigned char row;
    col = (currentByte-baseAddress)%WIDTH;
    row = (currentByte-baseAddress)/WIDTH;
    col = 14 + (col*3)+currentNibble;
    row += 1;

    printf("\033[%i;%iH\003",row,col);
}

void updateAsciiCursor()
{
    unsigned char col;
    unsigned char row;
    col = (currentByte-baseAddress)%WIDTH;
    row = (currentByte-baseAddress)/WIDTH;
    col = (14+16*3+1) + col;
    row += 1;

    printf("\033[%i;%iH\003",row,col);
}

void updateCommandCursor()
{
    // we put a big blank space to make sure that backspaces erases old chars
    printf("\033[22;0H                          \r> %s\003",command);
}

void processHexBrowseKey(uint16_t ch)
{
    if (ch==KEY_PGUP && baseAddress>(WIDTH*HEIGHT))
    {
        baseAddress -= (WIDTH*HEIGHT);
        currentByte -= (WIDTH*HEIGHT);
        updateMemoryDisplay();
        currentContext->updateCursor();
    }
    else if (ch==KEY_PGDOWN)
    {
        baseAddress += (WIDTH*HEIGHT);
        currentByte += (WIDTH*HEIGHT);
        updateMemoryDisplay();
        currentContext->updateCursor();
    }
    else if (ch==KEY_RIGHT)
    {
        currentNibble++;
        if (currentNibble == 2)
        {
            currentNibble = 0;
            currentByte++;
        }
        if (currentByte>=(baseAddress+(WIDTH*HEIGHT)))
        {
            baseAddress += WIDTH;
            updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
    else if (ch==KEY_LEFT  && currentByte>0)
    {
        if (currentNibble==0)
        {
            currentNibble = 1;
            currentByte--;
        }
        else
        {
            currentNibble = 0;
        }
        if (currentByte<baseAddress)
        {
             baseAddress -= WIDTH;
             updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
    else if (ch==KEY_DOWN)
    {
        currentByte += WIDTH;
        if (currentByte>=(baseAddress+(WIDTH*HEIGHT)))
        {
            baseAddress += WIDTH;
            updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
    else if (ch==KEY_UP && (currentByte>WIDTH))
    {
        currentByte -= WIDTH;
        if (currentByte<baseAddress)
        {
            baseAddress -= WIDTH;
            updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
}

void processAsciiBrowseKey(uint16_t ch)
{
    if (ch==KEY_PGUP && baseAddress > (WIDTH*HEIGHT))
    {
        baseAddress -= (WIDTH*HEIGHT);
        currentByte -= (WIDTH*HEIGHT);
        updateMemoryDisplay();
        currentContext->updateCursor();
    }
    else if (ch==KEY_PGDOWN)
    {
        baseAddress += (WIDTH*HEIGHT);
        currentByte += (WIDTH*HEIGHT);
        updateMemoryDisplay();
        currentContext->updateCursor();
    }
    else if (ch==KEY_RIGHT)
    {
        currentByte++;
        if (currentByte>=(baseAddress+(WIDTH*HEIGHT)))
        {
            baseAddress += WIDTH;
            updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
    else if (ch==KEY_LEFT && currentByte > 0)
    {
        currentByte--;
        if (currentByte<baseAddress)
        {
             baseAddress -= WIDTH;
             updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
    else if (ch==KEY_DOWN)
    {
        currentByte += WIDTH;
        if (currentByte>=(baseAddress+(WIDTH*HEIGHT)))
        {
            baseAddress += WIDTH;
            updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
    else if (ch==KEY_UP && currentByte > WIDTH)
    {
        currentByte -= WIDTH;
        if (currentByte<baseAddress)
        {
            baseAddress -= WIDTH;
            updateMemoryDisplay();
        }
        currentContext->updateCursor();
    }
}

void processCommandBrowseKey(uint16_t ch)
{
    if ((ch>='a' && ch <='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9'))
    {
        command[currentCommandOffset++] = ch;    
        command[currentCommandOffset]=0;
        currentContext->updateCursor();
    }
    else if (ch == 0x08)
    {
        if (currentCommandOffset>0)
        {
            currentCommandOffset--;    
            command[currentCommandOffset]=0;
            currentContext->updateCursor();
        }
    }
    else if (ch == '\n')
    {
        baseAddress = hexStringToNumber(command);
        currentByte = baseAddress;
        command[0]=0;
        currentCommandOffset = 0;
        updateMemoryDisplay();
        changeContext(&hexContext);
    }
    else if (ch == 0x1B)
    {
        command[0]=0;
        currentCommandOffset = 0;
        changeContext(&hexContext);
    }
}

int main(uint64_t param)
{
    uint16_t ch;
    hexContext.updateCursor = &updateHexCursor;
    hexContext.processBrowseKey = &processHexBrowseKey;
    asciiContext.updateCursor = &updateAsciiCursor;
    asciiContext.processBrowseKey = &processAsciiBrowseKey;
    commandContext.updateCursor = &updateCommandCursor;
    commandContext.processBrowseKey = &processCommandBrowseKey;

    currentContext = &hexContext;

    printf("Hex Editor [%i] [%i] [%i]\r\n",4, 5404, 1234567890);

    updateMemoryDisplay();
    currentContext->updateCursor();

    while (1)   
    {
        ch = poll_in();
        if (ch!=0)
        {
            if (ch=='\t')
            {
                if (currentContext == &hexContext)
                {
                    changeContext(&asciiContext);
                }
                else if (currentContext == &asciiContext)
                {
                    changeContext(&commandContext);
                }
                else if (currentContext == &commandContext)
                {
                    changeContext(&hexContext);
                }
            }
            else
            {
                currentContext->processBrowseKey(ch);
            }
        }
    }
    
}
