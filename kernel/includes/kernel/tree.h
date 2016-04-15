#pragma once
#include "types.h"

typedef struct _tree_node
{
    void* data;
    struct _tree_node *left;
    struct _tree_node *right;
} tree_node;

typedef struct _tree
{
    tree_node* root_node;
    // memory pool

} tree;

void tree_insert(tree* tree, void* data, uint64_t key);
void tree_remove(tree* tree, uint64_t key);
void* tree_get(tree* tree, uint64_t key);
