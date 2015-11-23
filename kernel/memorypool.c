#include "memorypool.h"
extern void memclear64(void* destination, uint64_t size);
extern void spinLock(uint64_t*);
extern void spinUnlock(uint64_t*);
extern bool atomic_set(void* var, uint8_t bit);
extern bool atomic_cmpxchg(void* var, uint64_t value, uint64_t oldvalue);
extern void* kernelAllocPages(uint64_t count);
extern void kernelReleasePages(uint64_t addr, uint64_t count);


memory_pool_node* memory_pool_expand(uint64_t pool);
memory_pool pools[MAX_MEMORY_POOLS];



void init_memory_pools()
{
    memclear64((void*)pools,sizeof(memory_pool)*MAX_MEMORY_POOLS);
}

uint64_t create_memory_pool(uint64_t objSize)
{
    uint64_t i;
    for (i = 0; i < MAX_MEMORY_POOLS;i++)
    {
        // atomic_cmpxchg will only succeed if pool node_size was 0
        if (atomic_cmpxchg(&(pools[i].node_size), objSize, 0))
        {
            pools[i].first = 0;
            memory_pool_expand(i);  // Allocate at least one page for the pool
            return i;
        }
    }
    __asm("int $3");
    return 0;
}

// This function will guarantee that each node reside on consecutive 
// physical pages. 
memory_pool_node* memory_pool_expand(uint64_t pool)
{
    uint64_t i;
    uint64_t node_size = pools[pool].node_size+sizeof(memory_pool_node);
    memory_pool_node* new_nodes = 0;

    //////////////////////////////////////////////////////////////////////////
    // Create several pages with 1 new node or several nodes withing one 
    // page.
    //////////////////////////////////////////////////////////////////////////

    // If a node is bigger than half a page, then we will need 1 page per objects
    // but if the object is smaller, then we can fit several in one page.
    if (node_size >2048)
    {
        uint64_t page_count = (node_size+0xFFF) >> 12;
        new_nodes = (memory_pool_node*)kernelAllocPages(page_count);         
        if (new_nodes == 0) return 0;
        memclear64((void*)new_nodes,page_count*4096);
        // This new node will start at begining of page and we will only have 1 node
    }
    else
    {
        uint64_t object_count = 4096/node_size;
        new_nodes = (memory_pool_node*)kernelAllocPages(1);        
        if (new_nodes == 0) return 0;
        memclear64((void*)new_nodes,4096);
        memory_pool_node* n = new_nodes;

        // several nodes will exist in that page. Set the "next" pointer to 
        // the folldwing node. Leave the last one to 0
        uint64_t addr = (uint64_t)new_nodes;
        for (i = 0; i < object_count-1;i++)
        {
            addr += node_size;
            n->next = (memory_pool_node*)addr;
            n = n->next;
        }
    }
    
    //////////////////////////////////////////////////////////////////////////
    // Now, atomically append the new node(s) to the end of the pool
    //////////////////////////////////////////////////////////////////////////
    spinLock(&(pools[pool].lock));
    memory_pool_node* n = pools[pool].first;
    if (n==0)
    {
        pools[pool].first = new_nodes;
    }
    else
    {
        while (n->next != 0) n = n->next;
        n->next = new_nodes;
    }
    spinUnlock(&(pools[pool].lock));

    return new_nodes;
}

void* reserve_object(uint64_t pool)
{
    if (pool >= MAX_MEMORY_POOLS) return 0;
    if (pools[pool].node_size == 0) return 0;

    // The pool is guaranteed to contain at least 1 node.
    // might not be free though. There is no need to lock
    // the function since we only set the "used" flag in the node
    // so we can do it with BTS. If we need to expand the pool
    // and add more nodes in the linked list, then the 
    // expand function will lock.
    while (1)
    {
        memory_pool_node* node = pools[pool].first;
        while (node != 0)
        {
            // We just need to be protected against other threads trying to reserve
            // the same slot at the same time.
            if (!atomic_set(&node->flags,0))
            {
                uint64_t addr = ((uint64_t)node)+sizeof(memory_pool_node);
                return (void*)addr;
            }
            node = node->next;
        }
        //If none was found, expand the pool and search again.
        //But if pool expansion failed, then return 0
        //TODO: should prevent two threads from expanding the pool
        //      if they get here at the time. The expand function does
        //      lock to prevent corrupting the linked list but
        //      that won't prevent two threads from expanding the pool,
        //      possibly creating an unnecessary expansion.
        if (memory_pool_expand(pool) == 0) return 0;
    }

    return 0;
}

void release_object(uint64_t pool, void* obj)
{
    uint64_t addr = ((uint64_t)obj)-sizeof(memory_pool_node);
    memory_pool_node* node = (memory_pool_node*)addr;
    //TODO: we should shrink the pool if last objects are free
    node->flags &= 0b11111110;
}

// 
// Obviously, no other thread should use objects from the pool when
// the pool is being destroyed. The destroy function does not lock
// so other threads must make sure they dont use the pool or any 
// objects in it.
//
void destroy_memory_pool(uint64_t pool)
{
    if (pool >= MAX_MEMORY_POOLS) return;

    //TODO: release all pages for all nodes

    //No need to lock here    
    pools[pool].node_size = 0;
}
