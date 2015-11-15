#include "../memorymap.h"
#include "../types.h"
#include "config.h"
#include "display.h"

unsigned long mutex=0;
uint64_t cpus[10];

extern void mutexLock(unsigned long mutex);
extern void mutexUnlock(unsigned long mutex);
extern uint64_t calculateMemoryUsage();
extern void registerIPIHandler(uint64_t vector, void* handler);
extern void sendIPI(uint64_t vector, uint64_t data,  uint64_t msgID);
extern uint64_t getTicksSinceBoot();

void testThread2()
{
}

void IPIHandler(uint64_t data, uint64_t msgID, uint64_t apicID)
{
    cpus[apicID] += data;
}

void initTestIPI()
{
    uint64_t i;
    for (i=0;i<10;i++) cpus[i]=0;
    registerIPIHandler(IPI_LAST_VECTOR-1,&IPIHandler);
}

void testIPI()
{
    sendIPI(IPI_LAST_VECTOR-1,0x02,242);
}

void testThread(unsigned int param)
{   
    int i;
    char c = '0';
    unsigned int* apic = (unsigned int*)(APIC_BASE+0x20);
    char* buf = (char*)(0xB8000+(160*(25-param)));
    buf[8*2] = 48+param;

    while(1)
    {
        mutexLock(mutex); // no real need for mutex. This is just for testing
        unsigned int cpuid = (*apic)>>24;
        
        c++;
        if (c>'9') c='0';
        buf[150] = c;
        buf[152] = ' ';
        buf[154] = 48+cpuid;
        buf[156] = ' ';
        mutexUnlock(mutex);
        
    }
}

void showMem()
{
    pf("Memory usage: %x\r\n",calculateMemoryUsage());
}

void showIPITestResult()
{
    pf("IPI: %x %x %x %x %x\r\n", cpus[0],cpus[1],cpus[2],cpus[3],cpus[4]);
}

void testGetTicksSinceBoot()
{
    pf("%x\r\n",getTicksSinceBoot());
}
