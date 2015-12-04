#include <stdio.h>
#include "hashtable.h"
#include <stdlib.h>

#define KEY_SIZE 10

typedef struct 
{
    hashtable_node node;
    int val;
    char* key;
} test;


void memclear64(void* dst, uint64_t size)
{
    uint64_t i;
    for (i=0;i<size;i++) ((char*)dst)[i]=0;
}

void rwlockWriteLock(uint64_t* l)
{
}

void rwlockWriteUnlock(uint64_t* l)
{
}

void rwlockReadLock(uint64_t* l)
{
}

void rwlockReadUnlock(uint64_t* l)
{
}

uint64_t gethash(hashtable* ht, test* t)
{
    return ht->hash_function(ht,t->node.keysize, t->node.key);
}

void addItem(hashtable* ht, int val,char* key)
{
    test* t = (test*)malloc(sizeof(test));
    t->key = (char*)malloc(8);
    memcpy(t->key,key,8);
    t->val = val;
    t->node.data = t;

    hashtable_add(ht,1,t->key, &t->node);
    printf("hash for item: %x (%x %x)\r\n",gethash(ht,t),t->node.key, t->key);
}

int main(int argc, char** argv)
{
    uint64_t size = hashtable_getrequiredsize(KEY_SIZE);
    printf("creating table of size %xh\r\n",size);
    hashtable* ht = (hashtable*)malloc(size);
    hashtable_init(ht,KEY_SIZE,HASH_AND);


    addItem(ht,1,"00000001");
    addItem(ht,2,"00000002");
    addItem(ht,3,"00000003");
    addItem(ht,4,"00000004");
    addItem(ht,5,"10000001");

    if (ht->buckets[ht->hash_function(ht,1,"00000001")].node == 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000001"))->val != 1) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000002"))->val != 2) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000003"))->val != 3) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000004"))->val != 4) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"10000001"))->val != 5) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"20000000")) != 0) printf("ERROR %i\r\n",__LINE__);

    hashtable_remove(ht,1,"00000002");
    if (ht->buckets[ht->hash_function(ht,1,"00000001")].node == 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000001"))->val != 1) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000002")) != 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000003"))->val != 3) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000004"))->val != 4) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"10000001"))->val != 5) printf("ERROR %i\r\n",__LINE__);

    hashtable_remove(ht,1,"00000001");
    if (ht->buckets[ht->hash_function(ht,1,"00000001")].node == 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000001")) != 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000002")) != 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000003"))->val != 3) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000004"))->val != 4) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"10000001"))->val != 5) printf("ERROR %i\r\n",__LINE__);

    hashtable_remove(ht,1,"00000004");
    if (ht->buckets[ht->hash_function(ht,1,"00000001")].node == 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000001")) != 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000002")) != 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000003"))->val != 3) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"00000004")) != 0) printf("ERROR %i\r\n",__LINE__);
    if (((test*)hashtable_get(ht,1,"10000001"))->val != 5) printf("ERROR %i\r\n",__LINE__);

    hashtable_remove(ht,1,"00000003");
    if (ht->buckets[ht->hash_function(ht,1,"00000001")].node != 0) printf("ERROR %i\r\n",__LINE__);


}
