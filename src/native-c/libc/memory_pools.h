#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct _memory_pool memory_pool;
typedef struct _pool_node pool_node;
typedef struct _pool_bank pool_bank;
typedef struct _node_list node_list;

/*
create memory_pool_list
*/

struct _memory_pool {
    node_list *node_lists;
    size_t list_size;
    pool_bank *first_bank;
}; // node_lists starts here


struct _node_list {
    pool_node *first_free;
    pool_node *first_used;
    size_t data_size;
};

struct _pool_bank {
    size_t size;
    size_t position;
    pool_bank *prev;
    pool_bank *next;    
};

struct _pool_node {
    size_t size;
    pool_node *prev;
    pool_node *next;
    pool_node *prev_in_bank;
    pool_node *next_in_bank;
    bool used;
};