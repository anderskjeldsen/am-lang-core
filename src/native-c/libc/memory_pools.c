
#include <memory_pools.h>
/*
memory_pool
  - bank 1
    - node 1
    - node 2
  - bank 2
    -node 3
    -node 4
*/

memory_pool * create_memory_pool(size_t unit_size) {
    memory_pool *pool = calloc(1, sizeof(memory_pool));
    pool_bank *bank = calloc(1, sizeof(pool_bank));
    pool->first_bank = bank;
    bank->size = bank_size;
    return pool;
}

pool_bank *create_pool_bank(memory_pool *pool, size_t size) {
    size_t alloc_size = 
}

void *alloc_from_pool(memory_pool *pool, size_t size) {
    pool_bank *bank = pool->current_bank;
    if (bank->position + size + sizeof(pool_node) < bank->size) {

    } else {

    }
}

void free_from_pool(memory_pool *pool, void *data) {
    if (data == NULL) return;
    pool_node *node = data - sizeof(pool_node);
    if (node->prev_in_bank != NULL && !node->prev_in_bank->used) {

    }
}