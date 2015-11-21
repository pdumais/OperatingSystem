#include "includes/kernel/types.h"
#define MAX_MEMORY_POOLS 50

struct _memory_pool_node
{
    struct _memory_pool_node* next;
    
    char data[];
}; 

typedef struct _memory_pool_node memory_pool_node;

typedef struct
{
    memory_pool_node* first;
    memory_pool_node* last;
    uint64_t          node_size;
} memory_pool;


void init_memory_pools();
uint64_t create_memory_pool(uint64_t objSize);
void* reserve_object(uint64_t pool);
void release_object(uint64_t pool, void* obj);
void destroy_memory_pool(uint64_t pool);
