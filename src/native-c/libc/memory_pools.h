#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct _memory_pool memory_pool;
typedef struct _pool_node pool_node;
typedef struct _pool_bank pool_bank;
//typedef struct _node_list node_list;

/*
create memory_pool_list
*/

struct _memory_pool {
    size_t unit_size;
    pool_bank *first_bank;
    pool_node *first_used_node;
    pool_node *first_free_node;

}; // node_lists starts here

/*
struct _node_list {
    pool_node *first_free;
    pool_node *first_used;
    size_t data_size;
};
*/

struct _pool_bank {
    size_t units;
    size_t unit_position;
    size_t used_units; // when 0, release bank?
    pool_bank *prev;
    pool_bank *next;
};

struct _pool_node {
    pool_node *prev;
    pool_node *next;
    pool_bank *bank;
//    pool_node *prev_in_bank;
//    pool_node *next_in_bank;
    bool used;
};

memory_pool * create_memory_pool(size_t unit_size);
void free_memory_pool(memory_pool *pool);
pool_bank *create_pool_bank(memory_pool *pool, size_t units);
void free_pool_bank(memory_pool *pool, pool_bank *bank);
void *alloc_from_pool(memory_pool *pool);
void free_from_pool(memory_pool *pool, void *data);