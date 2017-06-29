#include "keyboard.h"
#include "utils.h"
#include "../memorymap.h"
#include "includes/kernel/types.h"
#include "printf.h"
// http://www.computer-engineering.org/ps2keyboard/scancodes1.html

extern void getInterruptInfoForBus(unsigned long bus, unsigned int* buffer);
extern void storeCharacter(uint16_t c);
extern void registerIRQ(void* handler, unsigned long irq);

unsigned char ctrlKey;
unsigned char shiftKey;

uint16_t scanCodes[] = {
        0x0000,0x001B,'1'   ,'2'   ,'3'   ,'4'   ,'5'   ,'6'   ,'7'   ,'8'   ,'9'   ,'0'   ,'-'   ,'='   ,0x0008,0x0009,
        'q'   ,'w'   ,'e'   ,'r'   ,'t'   ,'y'   ,'u'   ,'i'   ,'o'   ,'p'   ,'['   ,']'    ,0x000A,0x0000,'a'   ,'s'   ,
        'd'   ,'f'   ,'g'   ,'h'   ,'j'   ,'k'   ,'l'   ,';'   ,'\''  ,'`'   ,0x0000,'\\'  ,'z'   ,'x'   ,'c'   ,'v'   ,
        'b'   ,'n'   ,'m'   ,','   ,'.'    ,'/'  ,0x0000,0x0000,0x0000,' '   ,0x0000,KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,
        KEY_F6,KEY_F7,KEY_F8,KEY_F9,KEY_F10,0x0000,0x0000,0x0000,KEY_UP,KEY_PGUP,0x0000,KEY_LEFT,0x0000,KEY_RIGHT,0x0000,0x0000,
        KEY_DOWN,KEY_PGDOWN,0x0000,0x0000,0x0000,0x0000,0x0000,KEY_F11,KEY_F12,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};

uint16_t shiftedScanCodes[] = {
        0x0000,0x001B,'!'   ,'@'   ,'#'   ,'$'   ,'%'   ,'^'   ,'&'   ,'*'   ,'('   ,')'   ,'_'   ,'+'   ,0x0008,0x0009,
        'Q'   ,'W'   ,'E'   ,'R'   ,'T'   ,'Y'   ,'U'   ,'I'   ,'O'   ,'P'   ,'{'   ,'}'   ,0x000A,0x0000,'A'   ,'S'   ,
        'D'   ,'F'   ,'G'   ,'H'   ,'J'   ,'K'   ,'L'   ,':'   ,'"'   ,'~'   ,0x0000,'|'   ,'Z'   ,'X'   ,'C'   ,'V'   ,
        'B'   ,'N'   ,'M'   ,'<'   ,'>'    ,'?'  ,0x0000,0x0000,0x0000,' '   ,0x0000,KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,
        KEY_F6,KEY_F7,KEY_F8,KEY_F9,KEY_F10,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,KEY_F11,KEY_F12,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};


void keyboardHandler()
{
    unsigned char  key;
    uint16_t c = 0;
    INPORTB(key,0x60);
    if (key==0xE0)
    {
        INPORTB(key,0x60);
    }


    if (key&0x80)   // break code
    {
        key = key & 0x7F;

        if  (key==0x1D)
        {
            ctrlKey = 0;
        } else if (key == 0x2A || key == 0x36)
        {
            shiftKey = 0;
        }
    }
    else            // make code
    {
        if  (key==0x1D)
        {
            ctrlKey = 1;
        }
        else if (key == 0x2A || key == 0x36)
        {
            shiftKey = 1;
        }
        else
        {
            if (shiftKey==0) c = scanCodes[key]; else c = shiftedScanCodes[key];
        }


        if (c!=0)
        {
            storeCharacter(c);
        }
    }
}

void initKeyboard()
{
    unsigned int i;
    unsigned int ioapicDevices[64];

    ctrlKey = 0;
    shiftKey = 0;

    getInterruptInfoForBus(0x20415349, &ioapicDevices); // "ISA "
    for (i=0;i<64;i++)
    {
        if (ioapicDevices[i]==0) continue;

        unsigned char pin = ioapicDevices[i]&0xFF;  
        unsigned short busirq = ioapicDevices[i]&0xFF00;
        if (busirq == 0x0100)
        {
            registerIRQ(&keyboardHandler,pin);
            return;
        }
    }
    
    pf("Could not find IOAPIC mapping of IRQ 1\r\n");
}
