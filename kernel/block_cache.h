#include "includes/kernel/types.h"

#define BLOCK_SIZE 512

// Transitions:
// A block is initially free
// FREE -> RESERVED_FOR_NEW_WRITE_CREATION -> PENDING_USER_WRITE -> PENDING_WRITE
//      Guaranteed to happen on same thread
// FREE -> RESERVED_FOR_NEW_READ_CREATION -> PENDING_READ
//      Guaranteed to happen on same thread
// PENDING_WRITE -> WRITING
//      Done by IRQ handler or any thread
// PENDING_READ -> READING
//      Done by IRQ handler or any thread
// READING -> IDLE
//      Done by IRQ handler
// WRITING -> IDLE
//      Done by IRQ handler
// IDLE-> PENDING_USER_WRITE -> PENDING_WRITE
//      This transition can be done by any thread. so protection is needed
// IDLE -> TRASHING
// - Any other transition is impossible.
// - A block is considered UPTODATE if it is idle, pending_write or writing
//      - to read a block, it must be up to date. so the "readers" count
//        will prevent the block from getting TRASHING or PENDING_USER_WRITE while we read it
//      - to write to a block, it must be IDLE or FREE. The transition from
//        IDLE->PENDING_USER_WRITE could be attempted by 2 competing threads so this 
//        transtion must be protected
// - The following properties are observed:
//      - Several threads can read from an UPTODATE block
//      - only one thread can write in block and no other threads can read at the same time.
//      - no threads can write to block while other threads are reading

#define BLOCK_CACHE_FREE                                0
#define BLOCK_CACHE_RESERVED_FOR_NEW_WRITE_CREATION     1
#define BLOCK_CACHE_RESERVED_FOR_NEW_READ_CREATION      2
#define BLOCK_CACHE_PENDING_READ                        3
#define BLOCK_CACHE_PENDING_WRITE                       4
#define BLOCK_CACHE_PENDING_USER_WRITE                  5
#define BLOCK_CACHE_PENDING_USER_READ                   6
#define BLOCK_CACHE_IDLE                                7
#define BLOCK_CACHE_WRITING                             8
#define BLOCK_CACHE_READING                             9
#define BLOCK_CACHE_TRASHED                             10
    

struct block_cache_entry
{
    uint64_t idle_transition_critical_section;
    uint64_t bufferlock;
    unsigned long block;
    // The readers count is used so that the block stays up to 
    // date while threads are reading it. While the count is greater
    // than zero, the block cannot be deleted nor get overwritten
    // by writer.
    volatile uint64_t readers;
    char *data;
    unsigned char device;
    volatile uint8_t state;
    unsigned long lastAccess;
} __attribute((packed))__;


void block_cache_init(char* cacheAddress);
int block_cache_read(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks);
int block_cache_write(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks);
