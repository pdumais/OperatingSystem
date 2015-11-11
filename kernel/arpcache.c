#include "display.h"

//TODO: we should also use a second level of cache for recently used addresses. this would reduce the chances of
//      collisions if we happen to be using 5 addresses that collision together (the collision depth is 4 here)
//WARNING: entries in cache must be little-endian

// This is the number of bits in the IP address that we will use as HASH index.
#define MAC_HASH_PRECISION 1024
// if a collision occur, we will have 4 entries in that slots
#define MAC_HASH_COLLISION 4
#define CACHE_TIMEOUT (60*5)
extern unsigned long getSecondsSinceBoot();

struct ArpCacheEntry
{
    unsigned int ip;
    unsigned long timestamp;
    unsigned long mac;
};

// hash table with 4096 entries and 4 slots per entry (for collisions)
struct ArpCacheEntry arpCache[MAC_HASH_PRECISION][MAC_HASH_COLLISION]={0};

unsigned short iphash(unsigned int ip)
{
    return (ip&(MAC_HASH_PRECISION-1));
}


unsigned long arpcache_get(unsigned int ip)
{
    unsigned short hash = iphash(ip);
    unsigned short i;
    unsigned long s = getSecondsSinceBoot();
    unsigned long timeout = s - CACHE_TIMEOUT;
    if (s<CACHE_TIMEOUT) timeout = 0;
    for (i=0;i<MAC_HASH_COLLISION;i++)
    {
        if (arpCache[hash][i].ip == ip && (arpCache[hash][i].timestamp >= timeout))
        {
            return arpCache[hash][i].mac;
        }
    }
    return 0;
}

void arpcache_put(unsigned int ip, unsigned long mac)
{
    unsigned short hash = iphash(ip);
    unsigned short i;
    unsigned long s = getSecondsSinceBoot();
    unsigned long timeout = s - CACHE_TIMEOUT;
    if (s<CACHE_TIMEOUT) timeout = 0;
    unsigned short emptySlot = 0; // if not empty entry found, we will crush entry 0. but we should crush the oldest one instead.
    for (i=0;i<MAC_HASH_COLLISION;i++)
    {
        if (arpCache[hash][i].ip == ip)
        {
            emptySlot = i;
            break;
        }
        if (arpCache[hash][i].ip==0 || (arpCache[hash][i].timestamp < timeout))
        {
            emptySlot = i;
            break;
        }
    }

    arpCache[hash][emptySlot].ip = ip;
    arpCache[hash][emptySlot].mac = mac;
    arpCache[hash][emptySlot].timestamp = s;
}


