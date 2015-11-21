#include "memorypool.h"
extern void memclear64(char* destination, uint64_t size);

memory_pool pools[MAX_MEMORY_POOLS];



void init_memory_pools()
{
    memclear64(pools,sizeof(memory_pool)*MAX_MEMORY_POOLS);
}

uint64_t create_memory_pool(uint64_t objSize)
{
    uint64_t i;
    for (i = 0; i < MAX_MEMORY_POOLS;i++)
    {
        if (pools[i].node_size == 0)
        {
            pools[i].node_size = objSize;
            pools[i].first = 0;
            pools[i].last = 0;
            return i;
        }
    }
}

void* reserve_object(uint64_t pool)
{
}

void release_object(uint64_t pool, void* obj)
{
    //TODO: should we release pages?
}

void destroy_memory_pool(uint64_t pool)
{
    if (pool >= MAX_MEMORY_POOLS) return;
    
    pools[pool].node_size = 0;
    //TODO: release all pages for all nodes
}
