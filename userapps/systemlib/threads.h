#include "types.h"
uint64_t getCurrentCPU();
uint64_t virt2phys(uint64_t addr);
uint64_t loadProcess(char* name);
void waitForProcessDeath(uint64_t processID);
void getDateTime(char* str);
