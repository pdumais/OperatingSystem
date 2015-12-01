#include "includes/kernel/types.h"
#include "block_cache.h"
#include "../memorymap.h"
#include "utils.h"

// if increasing that value, make sure the space allocated in memorymap is still valid
#define CACHE_SIZE 128

// external declarations
extern void yield();
extern void init_ata();
extern int ata_read(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern int ata_write(unsigned int dev, unsigned long sector, char* buffer, unsigned long count);
extern unsigned char ata_isBusy(unsigned char);
extern void memcpy64(char* source, char* dest, unsigned long size);
extern unsigned long getTicksSinceBoot();
extern void spinLock(uint64_t*);
extern void spinUnlock(uint64_t*);
extern void rwlockWriteLock(uint64_t*);
extern void rwlockWriteLock(uint64_t*);
extern void rwlockWriteUnlock(uint64_t*);
extern void rwlockReadLock(uint64_t*);
extern void rwlockReadUnlock(uint64_t*);
extern void rwlockWriteUnlock(uint64_t*);
extern void rwlockReadLock(uint64_t*);
extern void rwlockReadUnlock(uint64_t*);

// forward declarations
void onxferComplete(unsigned char dev, unsigned long block, unsigned long count);
void onataReady(unsigned char dev);
void schedule_io(int dev);
bool transitionFromIdle(struct block_cache_entry* cacheEntry, uint8_t newstate);

uint64_t cache_lock = 0;
uint64_t scheduleio_lock = 0;

// The block cache should not be fixed-sized. It should grow indefinitely.
//      it should also be implemented as a tree for faster search
struct block_cache_entry cache[CACHE_SIZE]={0};


// This will return either 
//  - a new empty block 
//  - an existing block
//  - zero is returned if cache is full and no block was found
//
// If the block existed already, the usage count is increased and
// the block is returned. The calling function will need to wait until the 
// block becomes ready if it is not.
// If two threads attempted to read the same block or one read and other thread writes,
// it would be impossible that two blocks would get created since this function is 
// serializing (becaue of the spinlock) So if the first thread attempted to read, then
// a new block would get created. The second thread, attempting to read or write would
// find that block and wait until it becomes uptodate.
struct block_cache_entry* getOrCreateCacheEntry(unsigned char dev, unsigned long block, bool forWrite)
{
    // Since we are locking the whole cache here, there is no way
    // that two threads can create a new block at the same time.
    spinLock(&cache_lock);
    unsigned int i;
    struct block_cache_entry* entry = 0;
    uint8_t newstate = BLOCK_CACHE_RESERVED_FOR_NEW_WRITE_CREATION; 
    if (forWrite == 0)  newstate = BLOCK_CACHE_RESERVED_FOR_NEW_READ_CREATION;

    for (i=0;i<CACHE_SIZE;i++)
    {
        uint8_t blockstate = cache[i].state;
        if ((cache[i].device == dev) && (cache[i].block == block) && (blockstate != BLOCK_CACHE_FREE))
        {
            //TODO: make sure the block is still not free and increment usage count
            cache[i].lastAccess = getTicksSinceBoot(); 
            spinUnlock(&cache_lock);
            return &cache[i];
        }
        else if (blockstate == BLOCK_CACHE_FREE)
        {
            // If an entry is 0, then it is a candidate for a new block
            // in case we don't find a match
            // Since this function is the only one that can assign a new block
            // and it locks, then it is safe to say that the state will stay 0.
            entry = &cache[i];
        }
    }

    if (entry == 0)
    {
        // block was not found and there are no candidates for a new block
        spinUnlock(&cache_lock);
        return 0;
    }

    entry->state = newstate;
    entry->lastAccess = getTicksSinceBoot();
    entry->block = block;
    entry->device = dev;

    spinUnlock(&cache_lock);
    return entry;

}


// This will return an entry that is pending read. 
struct block_cache_entry* getFirstPendingReadCacheEntry(unsigned char dev)
{
    unsigned int i;
    for (i=0;i<CACHE_SIZE;i++)
    {
        if (cache[i].state == BLOCK_CACHE_PENDING_READ) return &cache[i];
    }
    return 0;
}

struct block_cache_entry* getFirstPendingWriteCacheEntry(unsigned char dev)
{
    unsigned int i;
    for (i=0;i<CACHE_SIZE;i++)
    {
        if (cache[i].state == BLOCK_CACHE_PENDING_WRITE) return &cache[i];
    }
    return 0;
}



// This will delete some cache entries to be reused.
// We delete the oldest blocks
void clearCacheEntries(unsigned int count)
{
    uint64_t i;

    while (count)
    {
        unsigned long leastAccess = 0xFFFFFFFFFFFFFFFF;
        struct block_cache_entry* entry = 0;
        for (i=0;i<CACHE_SIZE;i++)
        {
            if ((cache[i].state == BLOCK_CACHE_IDLE) && (cache[i].lastAccess < leastAccess))
            {
                leastAccess = cache[i].lastAccess;
                entry = &cache[i];
            }
        }
    
        if (entry == 0) return;

        // we set it to TRASHED temporarily just so it wont be used by something else
        if (transitionFromIdle(entry, BLOCK_CACHE_TRASHED))
        {
            entry->device=-1;
            entry->block=-1;
            entry->state = BLOCK_CACHE_FREE;
            count--;
        }
    }
}


void block_cache_init(char* cacheAddress)
{
    unsigned int i;
    uint64_t baseAddr = (uint64_t)cacheAddress;
    for (i=0;i<CACHE_SIZE;i++)
    {
        cache[i].data = (char*)baseAddr;
        cache[i].state = 0;
        cache[i].bufferlock = 0;
        cache[i].idle_transition_critical_section = 0;
        baseAddr+=512;
    }

    init_ata(&onxferComplete, &onataReady);
}

void onataReady(unsigned char dev)
{
    // This handler is called when the ata handler has changed to ready state.
    // But it is possible that another competing thread initiates a transfer 
    // in that same time frame so the device would become unavailable again.
    schedule_io((unsigned int)dev);
}

void onxferComplete(unsigned char dev, unsigned long block, unsigned long count)
{
    unsigned long i,n;
    unsigned int temp;
    struct block_cache_entry* entry;

    for (i=block;i<(block+count);i++)
    {
        for (n=0;n<CACHE_SIZE;n++)
        {
            if (cache[n].device == dev && cache[n].block == i)
            {
                if (cache[n].state == BLOCK_CACHE_READING || cache[n].state == BLOCK_CACHE_WRITING)
                {
                    cache[n].state = BLOCK_CACHE_IDLE;
                }
                else
                {
                    __asm("int $3");
                }
            }
        }
    }
}

// Upon exiting that function, it is guaranteed that the block
// will either be in pending_write, writing or uptodate. 
// The state will not transition to any other state (but could
// transition to 1 of those 3) because the readers count will be 
// increased.
void lockBlockForUserRead(struct block_cache_entry* cacheEntry)
{
    // We increase the readers count. At that point, the block
    // will not transition to a state other than those that
    // imply that the block is up to date. But it could be to one
    // of those other states already so we will wait until it
    // becomes up to date
    // The idle_transition critical section must be locked when
    // incrementing the readers counter since it is not allowed
    // to transition from IDLE when readers are present
    spinLock(&cacheEntry->idle_transition_critical_section);
    __asm("lock incq (%0)" : : "r"(&cacheEntry->readers));
    spinUnlock(&cacheEntry->idle_transition_critical_section);

    while ((cacheEntry->state != BLOCK_CACHE_PENDING_WRITE) &&
           (cacheEntry->state != BLOCK_CACHE_IDLE) &&
           (cacheEntry->state != BLOCK_CACHE_WRITING))
    {
        yield();
    }
}   

//TO IMPROVE: find number of consecutive blocks that are not cached before 
//      issuing a read request but we must not exceed cache size
//      because right now, we issue a read sector by sector. but IRQ handler can already handle several sectors
//      will need to modify schedule_io as well if we do that.
int block_cache_read(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks)
{
    unsigned long i;
    unsigned int oldFlags;
    struct block_cache_entry* cacheEntry = 0;

    for (i = blockNumber; i< blockNumber+numberOfBlocks; i++)
    {
        // Get a cache entry. either a valid entry or a new empty entry will be returned.
        // If zero is returned, it means the cache is full. We will then delete some old entries.
        cacheEntry = getOrCreateCacheEntry(dev, i, false);
        while (cacheEntry == 0)
        {
            clearCacheEntries(1);
            cacheEntry = getOrCreateCacheEntry(dev, i, false);
            // if after attempting to delete entries the cache is still full, we should yield and wait
            // for some entries to free up.
            if (cacheEntry == 0)
            {
                yield();
            }
        }

        if (cacheEntry->state == BLOCK_CACHE_RESERVED_FOR_NEW_READ_CREATION)
        {
            // when block is reserved for creation, it is guaranteed not to be used
            // by any other threads so it safe to test and change the state without locking
            cacheEntry->state = BLOCK_CACHE_PENDING_READ;
            schedule_io(dev);
        }
    
        // At this point we are guaranteed to have a new block or an existing block.
        lockBlockForUserRead(cacheEntry);

        // At this point, the buffer PENDING_USER_READ
        rwlockReadLock(&cacheEntry->bufferlock);
        memcpy64(cacheEntry->data,(char*)&buffer[512*(i-blockNumber)],512);
        rwlockReadUnlock(&cacheEntry->bufferlock);
        __asm("lock decq (%0)" : : "r"(&cacheEntry->readers));
    }
}


void schedule_io(int dev)
{
    // We prioritize read requests over writes. write requests will be delayed until
    // no more read requests. Again, this is not optimal, so it should be reworked
    spinLock(&scheduleio_lock);
    struct block_cache_entry* entry = getFirstPendingReadCacheEntry(dev);

    if (entry != 0)
    {
        entry->state = BLOCK_CACHE_READING;
        if (ata_read(dev, entry->block, entry->data, 1))
        {
            spinUnlock(&scheduleio_lock);
            return;
        }
        entry->state = BLOCK_CACHE_PENDING_READ;
    }
    
    entry = getFirstPendingWriteCacheEntry(dev);
    if (entry != 0)
    {
        entry->state = BLOCK_CACHE_WRITING;
        if (!ata_write(dev, entry->block, entry->data, 1))
        {
            entry->state = BLOCK_CACHE_PENDING_WRITE;
        }
    }
    spinUnlock(&scheduleio_lock);
}

bool transitionFromIdle(struct block_cache_entry* cacheEntry, uint8_t newstate)
{
    bool ret = false;
    spinLock(&cacheEntry->idle_transition_critical_section);
    if (cacheEntry->state == BLOCK_CACHE_IDLE)
    {
        cacheEntry->state = newstate;
        ret = true;
    }
    spinUnlock(&cacheEntry->idle_transition_critical_section);
    return ret;
}

// This function will guarantee that the block is PENDING_USER_WRITE
// upon returning. If the block was already PENDING_USER_WRITE, it
// will wait since the function succeeds only if transitioning from 
// IDLE to PENDING_USER_WRITE
void transitionBlockToFillPending(struct block_cache_entry* cacheEntry)
{
    if (cacheEntry->state == BLOCK_CACHE_RESERVED_FOR_NEW_WRITE_CREATION)
    {
        cacheEntry->state = BLOCK_CACHE_PENDING_USER_WRITE;
        return;
    }
        
    while (1)
    {
        // We will retest, atomically, that condition in transitionFromIdle
        if (cacheEntry->state == BLOCK_CACHE_IDLE && cacheEntry->readers ==0)
        {
            if (transitionFromIdle(cacheEntry,BLOCK_CACHE_PENDING_USER_WRITE)) break;
        }
        yield();
    }
}

//TO IMPROVE: It would be nice if we could overwrite a block that is already WRITE_PENDING
//            or READ_PENDING
//            It would also be nice to copy in more than one block at a time.
int block_cache_write(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks)
{
    unsigned long i;
    unsigned int oldFlags;
    struct block_cache_entry* cacheEntry = 0;

    for (i = blockNumber; i< blockNumber+numberOfBlocks; i++)
    {
        // Get a cache entry. either a valid entry or a new empty entry will be returned.
        // If zero is returned, it means the cache is full. We will then delete some old entries.
        // We set the block as FILL_PENDING initially because a read request could come it from 
        // another thread between the time that we have allocated the block and the time where we 
        // copy the data in the buffer
        cacheEntry = getOrCreateCacheEntry(dev, i, true);
        while (cacheEntry == 0)
        {
            clearCacheEntries(1);
            cacheEntry = getOrCreateCacheEntry(dev, i, true);

            // if after attempting to delete entries the cache is still full, we should yield and wait
            // for some entries to free up.
            if (cacheEntry == 0)
            {
                yield();
            }
        }

        // At this point, we have a new block or an existing block.
        // We must transition the block to user_write but only
        // if there are no readers and the block is either new or idle.
        transitionBlockToFillPending(cacheEntry);

        // At this point, block is guaranteed to be in USER_WRITE state
        //TODO: RWlock buffer
        rwlockWriteLock(&cacheEntry->bufferlock);
        memcpy64((char*)&buffer[512*(i-blockNumber)],cacheEntry->data,512);
        rwlockWriteUnlock(&cacheEntry->bufferlock);
        cacheEntry->state = BLOCK_CACHE_PENDING_WRITE;
        schedule_io(dev);
    }
}


