#include "includes/kernel/hashtable.h"

extern void memclear64(void* dst, uint64_t size);
extern void rwlockWriteLock(uint64_t*);
extern void rwlockWriteUnlock(uint64_t*);
extern void rwlockReadLock(uint64_t*);
extern void rwlockReadUnlock(uint64_t*);

uint64_t andhash(hashtable* ht, uint64_t keysize, uint64_t* key)
{
    uint64_t mask = ((1<<ht->keysize)-1);
    uint64_t k = 0;
    uint64_t i = 0;
    for (i = 0; i<keysize; i++)
    {
        k+=key[i];    
    }
    return k&mask;
}

uint64_t cumulativehash(hashtable* ht, uint64_t keysize, uint64_t* key)
{
    uint64_t k = 0;
    uint64_t i = 0;
    for (i = 0; i<keysize; i++) k+=key[i];    

    uint64_t mask = ((1<<ht->keysize)-1);
    uint64_t high = k >> ht->keysize;
    uint64_t low = k&mask;
    while (high != 0)
    {
        low += high;
        high = low >> ht->keysize;
        low = low&mask;
    }
    return low;
}

uint64_t hashtable_getrequiredsize(unsigned char keysize)
{
    uint64_t listsize = (1 << keysize)*sizeof(hashtable_bucket);
    uint64_t tablesize = listsize + sizeof(hashtable);

    return tablesize;
}

void hashtable_init(hashtable* ht,unsigned char keysize,unsigned char hash_function)
{
    memclear64(ht, hashtable_getrequiredsize(keysize));
    if (hash_function == HASH_CUMULATIVE)
    {
        ht->hash_function = &cumulativehash;
    }
    else if (hash_function == HASH_AND)
    {
        ht->hash_function = &andhash;
    }
    ht->keysize = keysize;
}

void hashtable_add(hashtable* ht,uint64_t keysize, uint64_t* key, hashtable_node* node)
{
    if (ht->hash_function == 0) return;
    uint64_t h = ht->hash_function(ht,keysize,key);
    node->key = key;
    node->keysize = keysize;
    node->next = 0;

    hashtable_bucket* bucket = &ht->buckets[h];
    rwlockWriteLock(&bucket->lock);
    hashtable_node* n = bucket->node;
    if (n==0)
    {
        bucket->node = node;
    }
    else
    {
        while (n->next != 0) n = n->next;
        n->next = node;
    }
    rwlockWriteUnlock(&bucket->lock);
}

inline bool comparekey(uint64_t* src, uint64_t* dst, uint64_t size)
{
    uint64_t i;
    for (i=0; i< size; i++) if (src[i]!=dst[i]) return false;
    return true;
}

void hashtable_remove(hashtable* ht,uint64_t keysize, uint64_t* key)
{
    if (ht->hash_function == 0) return;
    uint64_t h = ht->hash_function(ht,keysize,key);

    hashtable_bucket* bucket = &ht->buckets[h];
    rwlockWriteLock(&bucket->lock);
    hashtable_node* n = bucket->node;
    hashtable_node* previous = 0;
    while (n != 0)
    {
        if (comparekey(n->key,key,keysize))
        {
            if (previous == 0)
            {
                bucket->node = n->next;
            }
            else
            {
                previous->next = n->next;
            }
            n->next = 0;
            rwlockWriteUnlock(&bucket->lock);
            return;
        }
        previous = n;
        n = n->next;
    }
    rwlockWriteUnlock(&bucket->lock);
}

void* hashtable_get(hashtable* ht,uint64_t keysize, uint64_t* key)
{
    if (ht->hash_function == 0) return 0;

    void* ret = 0;

    uint64_t h = ht->hash_function(ht,keysize,key);
    hashtable_bucket* bucket = &ht->buckets[h];
    rwlockReadLock(&bucket->lock);
    hashtable_node* n = bucket->node;
    while (n != 0)
    {   
        if (comparekey(n->key,key,keysize))
        {
            ret = n->data;
            break;
        }
        n = n->next;
    }
    rwlockReadUnlock(&bucket->lock);

    return ret;
}

// This function will go through all nodes, locking the current bucket 
// for write, and will invoke the functor on each node. If the functor returns
// true, then the node will be removed from the list. This is usefull
// when wanting to safely remove items based on some other critaria than the key.
void hashtable_scan_and_clean(hashtable* ht, hashtable_clean_visitor visitor, void* meta)
{
    uint64_t tablesize = 1<<ht->keysize;
    uint64_t i;
    for (i=0;i<tablesize;i++)
    {
        hashtable_bucket* bucket = &ht->buckets[i];

        rwlockWriteLock(&bucket->lock);
        hashtable_node* n = bucket->node;
        hashtable_node* previous = 0;
        while (n != 0)
        {
            if (visitor(n->data,meta))
            {
                if (previous == 0)
                {
                    bucket->node = n->next;
                }
                else
                {
                    previous->next = n->next;
                }
                n = n->next;
            }
            else
            {
                previous = n;
                n = n->next;
            }
        }        
        rwlockWriteUnlock(&bucket->lock);
    }
}

bool hashtable_visit(hashtable* ht,uint64_t keysize, uint64_t* key,hashtable_visitor visitor, void* meta)
{
    if (ht->hash_function == 0) return false;

    uint64_t h = ht->hash_function(ht,keysize,key);
    hashtable_bucket* bucket = &ht->buckets[h];
    rwlockReadLock(&bucket->lock);
    hashtable_node* n = bucket->node;
    while (n != 0)
    {
        if (comparekey(n->key,key,keysize))
        {
            visitor(n->data,meta);
            rwlockReadUnlock(&bucket->lock);
            return true;
        }
        n = n->next;
    }
    rwlockReadUnlock(&bucket->lock);
    return false;
}

