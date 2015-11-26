#include "includes/kernel/types.h"
#define MAX_MEMORY_POOLS 50

struct _memory_pool_node
{
    struct _memory_pool_node *next;
    char flags;
    char reserved[7];   // this is to pack struct to 16bytes
} __attribute__((__packed__)); 

typedef struct _memory_pool_node memory_pool_node;

typedef struct
{
    memory_pool_node* first;
    uint64_t          node_size;
    uint64_t          lock;
} __attribute__((__packed__)) memory_pool;

void memory_pool_reclaim();
void init_memory_pools();
uint64_t create_memory_pool(uint64_t objSize);
void* reserve_object(uint64_t pool);
void release_object(uint64_t pool, void* obj);
void destroy_memory_pool(uint64_t pool);
