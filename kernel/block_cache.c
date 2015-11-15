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

// forward declarations
void onxferComplete(unsigned char dev, unsigned long block, unsigned long count);
void schedule_io(int dev);

//  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING 
// When block cache was implemented, the OS did not support SMP. 
// The block cache is not safe. Should modify it as per doc/blockcache
//  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING 


// The block cache should not be fixed-sized. It should grow indefinitely.
//      it should also be implemented as a tree for faster search
//      but in order to dynamically allocate entries, we need some kind of memory pool
//      kindof like the slab allocator on linux. The OS currently does not have such a mechanism
struct block_cache_entry cache[CACHE_SIZE]={0};


// this will return a matching cache entry. If no entry is found, the first free entry is returned.
// defaultFlags are the flags that will be set if a new block is created
struct block_cache_entry* getCacheEntry(unsigned char dev, unsigned long block, 
        unsigned int creationFlags, 
        unsigned int reserveFlags, 
        unsigned int* previousFlags)
{
    unsigned int i;
    //unsigned long interruptsFlags;
    struct block_cache_entry* entry = 0;
    //CLI(interruptsFlags);

    for (i=0;i<CACHE_SIZE;i++)
    {
        if (cache[i].flags&CACHE_BLOCK_VALID)
        {
            if (cache[i].device == dev && cache[i].block == block)
            {

//TODO: if another CPU calls clearCache, this block will be deleted. 
// Could LOCK(block). Then we could know if we can operate on it. 

                // an entry was found, so use it.
                // we set the IN_USE flag because once the block is read from disk,
                // the IRQ will clear the read_pending flag. It is possible that another thread
                // would delete that cache entry (because it would need more cache entries for a big request)
                // and then the entry would get deleted before it would even be consumed by this current thread.
                // in such a case, the block is reused for another request and gets
                // fullfilled before this current thread returns. So this thread would detect the block as being ok
                // but it would not match the proper block number. 
                // the CACHE_IN_USE will prevent the block from being deleted.
                *previousFlags = cache[i].flags;
                cache[i].flags |= reserveFlags;
                cache[i].lastAccess = getTicksSinceBoot(); // That wont wrap around... trust me.
                //STI(interruptsFlags);
//TODO: if we locked "entry", then we should unlock it.

                return &cache[i];
            }
        }
        else if (!(cache[i].flags&CACHE_IN_USE)) 
        {
            // if falgs was set to CACHE_IN_USE for VALID is clear, it means that another thread
            // is currently assigning a request to that block. so don't use it.
            if (entry == 0) entry = &cache[i];
        }
    }    
    if (entry == 0)
    {
        *previousFlags = 0;
        //STI(interruptsFlags);
        return 0;
    }

//TODO: if two CPU does that, they will both own that entry. We should verify if it is still free
//      and restart again if not (cmpxchg)
 
    // At this point, we have an empty cache entry. see comment above for why we set CACHE_IN_USE
    entry->flags = creationFlags;
    entry->lastAccess = getTicksSinceBoot();
    entry->block = block;
    entry->device = dev;
    *previousFlags = 0;
    //STI(interruptsFlags);
    return entry;
}


// This will return an entry that is pending read. 
struct block_cache_entry* getFirstPendingReadCacheEntry(unsigned char dev)
{
    unsigned int i;
    for (i=0;i<CACHE_SIZE;i++)
    {
        if (((cache[i].flags&(CACHE_BLOCK_VALID|CACHE_READ_PENDING)) == (CACHE_BLOCK_VALID|CACHE_READ_PENDING)) 
            && (cache[i].device==dev))
        {
            return &cache[i];
        }
    }
    return 0;
}

struct block_cache_entry* getFirstPendingWriteCacheEntry(unsigned char dev)
{
    unsigned int i;
    for (i=0;i<CACHE_SIZE;i++)
    {
        if (((cache[i].flags&(CACHE_BLOCK_VALID|CACHE_WRITE_PENDING)) == (CACHE_BLOCK_VALID|CACHE_WRITE_PENDING)) 
            && (cache[i].device==dev))
        {
            return &cache[i];
        }
    }
    return 0;
}


// This will delete some cache entries to be reused.
// We delete the oldest blocks
void clearCacheEntries(unsigned int count)
{
    unsigned int i,i2,i3;
    //unsigned long interruptsFlags;

    // Another thread could mark the block as IN_USE while we are marking it as zero.
    //CLI(interruptsFlags);
    for (i2=0;i2<count;i2++)
    {
        unsigned long leastAccess = 0xFFFFFFFFFFFFFFFF;
        struct block_cache_entry* entry = 0;
        for (i=0;i<CACHE_SIZE;i++)
        {
            // we should never delete a cache entry that is write-pending
            if (!(cache[i].flags&CACHE_BLOCK_VALID)) continue;
            
            if (!(cache[i].flags&(CACHE_IN_USE|CACHE_WRITE_PENDING)))
            {
//TODO: if another CPU was requesting that block and it was given to him (marked as IN_USE or WRITE_PENDING)
//      we are going to delete it here and data will be corrupted.
// CPU1: getCacheEntry, found block but not marked IN_USE yet
// CPU2: is here. Clear the block
// CPU1: marks block as IN_USE and memcpy to user buffer: CORRUPTION
                if (cache[i].lastAccess < leastAccess)
                {
                    leastAccess = cache[i].lastAccess;
                    entry = &cache[i];
                    i3 = i;
                }
            }
        }
    
        if (entry == 0)
        {
            //STI(interruptsFlags);
            return;
        }
        entry->flags = 0;
        entry->device=-1;
        entry->block=-1;
    }

    //STI(interruptsFlags);
}


void block_cache_init(char* cacheAddress)
{
    unsigned int i;
    unsigned long baseAddr = cacheAddress;
    for (i=0;i<CACHE_SIZE;i++)
    {
        cache[i].data = baseAddr;
        cache[i].flags = 0;
        baseAddr+=512;
    }

    init_ata(&onxferComplete);
}

void onxferComplete(unsigned char dev, unsigned long block, unsigned long count)
{
    unsigned long i;
    unsigned int temp;
    struct block_cache_entry* entry;

    for (i=block;i<(block+count);i++)
    {
        entry = getCacheEntry(dev,i,0,0,&temp);
        if (!entry) continue;
        if (entry->flags&CACHE_BLOCK_VALID)
        {
            entry->flags &= ~(CACHE_READ_PENDING|CACHE_WRITE_PENDING);
        }
    }

    // prepare the next request now that we know we are done with one.
    //TODO: if we are blocking in that function, it should not be called here. since we are in a IRQ handler right now.
    schedule_io(dev);
}



//TO IMPROVE: find number of consecutive blocks that are not cached before 
//      issuing a read request but we must not exceed cache size
//      because right now, we issue a read sector by sector. but IRQ handler can already handle several sectors
//      will need to modify schedule_io as well \if we do that.
int block_cache_read(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks)
{
    unsigned long i;
    unsigned int oldFlags;
    struct block_cache_entry* cacheEntry = 0;

    for (i = blockNumber; i< blockNumber+numberOfBlocks; i++)
    {
        // Get a cache entry. either a valid entry or a new empty entry will be returned.
        // If zero is returned, it means the cache is full. We will then delete some old entries.
//TODO: getCacheEntry should have atomically reserved the block for us. We should be guaranteed 
// that know one will mess with it.
        cacheEntry = getCacheEntry(dev, i,CACHE_READ_PENDING|CACHE_BLOCK_VALID|CACHE_IN_USE, CACHE_BLOCK_VALID|CACHE_IN_USE , &oldFlags);
        while (cacheEntry == 0)
        {
            clearCacheEntries(1);
            cacheEntry = getCacheEntry(dev, i,CACHE_READ_PENDING|CACHE_BLOCK_VALID|CACHE_IN_USE, CACHE_BLOCK_VALID|CACHE_IN_USE, &oldFlags);
            // if after attempting to delete entries the cache is still full, we should yield and wait
            // for some entries to free up.
            if (cacheEntry == 0)
            {
                yield();
            }
        }

        // If the block was VALID, then it was not created new. So no need to issue a read request.
        // since the block contains the data already.
        if (!(oldFlags&CACHE_BLOCK_VALID)) 
        {
            // this function will look in the caches to find pending blocks and will issue
            // a read command if the device is not busy. If it is busy, it won't do anything
            // but at the end of the IRQ, schedule will be called again.
            schedule_io(dev);
        }

        // block until the sector we are requesting is available.
        // If the block was not created new, these flags wont be set so ignore that.
        // but if the block was created new, then we need to wait for it to be filled up.
        while ((cacheEntry->flags&(CACHE_READ_PENDING|CACHE_FILL_PENDING)))
        {
            yield();
        }

        //Note: If the block is write_pending, it doesn't matter. we are going to get
        // the latest info. We dont want the block on disk since it is not fresh because
        // there is a pending rewrite.
        memcpy64(cacheEntry->data,(char*)&buffer[512*(i-blockNumber)],512);
        cacheEntry->flags &= ~CACHE_IN_USE;
    }

    // to debug
    //for (i=0;i<16;i++) pf("%x ",(unsigned char)buffer[i]&0xFF);

}


void schedule_io(int dev)
{
//    unsigned long interruptsFlags;

    // Need to clear interrupts because we must make sure that an IRQ does not occur
    // when a thread calls this function. Otherwise there is a chance that we initiate
    // 2 disk operations
//    CLI(interruptsFlags);

    // if device is already busy, leave the request enqueued and it will be picked up 
    // after next IRQ. but if the device is not busy, then we need to schedule it now.
    if (!ata_isBusy(dev))
    {
//TODO: Dangerous! If two CPU fall here at the same time, 2 read/write requests will
//   be sent to ATA driver.

        // We prioritize read requests over writes. write requests will be delayed until
        // no more read requests. Again, this is not optimal, so it should be reworked

        //TO IMPROVE: getting the first pending request is not a fair algorithm at all
        struct block_cache_entry* entry = getFirstPendingReadCacheEntry(dev);
        if (entry != 0)
        {
            ata_read(dev, entry->block, entry->data, 1);
        }
        else
        {
            struct block_cache_entry* wentry = getFirstPendingWriteCacheEntry(dev);
            if (wentry != 0)
            {
                ata_write(dev, wentry->block, wentry->data, 1);
            }
        }
    }
//    STI(interruptsFlags);
}


int block_cache_write(unsigned long blockNumber, int dev, char* buffer, unsigned int numberOfBlocks)
{
    unsigned long i;
    unsigned int oldFlags;
    struct block_cache_entry* cacheEntry = 0;

    for (i = blockNumber; i< blockNumber+numberOfBlocks; i++)
    {
        while (1)
        {
            // Get a cache entry. either a valid entry or a new empty entry will be returned.
            // If zero is returned, it means the cache is full. We will then delete some old entries.
            // We set the block as FILL_PENDING initially because a read request could come it from 
            // another thread between the time that we have allocated the block and the time where we 
            // copy the data in the buffer
            cacheEntry = getCacheEntry(dev, i, CACHE_FILL_PENDING|CACHE_BLOCK_VALID|CACHE_IN_USE,CACHE_FILL_PENDING|CACHE_BLOCK_VALID|CACHE_IN_USE, &oldFlags);
            while (cacheEntry == 0)
            {
                clearCacheEntries(1);
                cacheEntry = getCacheEntry(dev, i, CACHE_FILL_PENDING|CACHE_BLOCK_VALID|CACHE_IN_USE,CACHE_FILL_PENDING|CACHE_BLOCK_VALID|CACHE_IN_USE,  &oldFlags);
                // if after attempting to delete entries the cache is still full, we should yield and wait
                // for some entries to free up.
                if (cacheEntry == 0)
                {
                    yield();
                }
            }

            // if the cache entry is already write_pending, then wait until it is written
            // yield and try again until it is fully written. 
            // Then overwrite it again. If it is read pending, then do the same thing
            // and wait for the disk operation to be completed.
            if (!(oldFlags&(CACHE_FILL_PENDING|CACHE_WRITE_PENDING|CACHE_READ_PENDING))) break;
            yield();
        }

        //Note: there is no chance that between the last check and here that
        //  the block be marked as read pending by another thread.
        //  a read at this point would not initiate a read request since the block
        //  exists and is pending fill.    
        // No chances for write_pending either because the block will be set as CACHE_FILL_PENDING
        // and we are checking that earlier.
        // so we are basically guaranteed that we are the only owner of the block in write-mode if we are here.
        memcpy64((char*)&buffer[512*(i-blockNumber)],cacheEntry->data,512);
        cacheEntry->flags |= (CACHE_WRITE_PENDING);
        cacheEntry->flags &= ~(CACHE_FILL_PENDING|CACHE_IN_USE);

        schedule_io(dev);
    }
}


