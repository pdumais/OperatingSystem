all: hash

hash: hash.c ../kernel/hashtable.c
	gcc -DUNIT_TEST hash.c ../kernel/hashtable.c -I ../kernel -I ../kernel/includes/kernel -g

bcache: bcache.c ../kernel/block_cache.c
	gcc -DUNIT_TEST bcache.c ../kernel/block_cache.c -I ../kernel -g
