all: bcache

bcache: bcache.c ../kernel/block_cache.c
	gcc -DUNIT_TEST bcache.c ../kernel/block_cache.c -I ../kernel -g
