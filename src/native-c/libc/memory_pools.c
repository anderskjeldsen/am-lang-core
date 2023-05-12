
#include <libc/memory_pools.h>
/*

Let some classes have its own mem pool since their object size is the same for each object. For example HashMapEntry. 

memory_pool
  - bank 1
    - node 1
    - node 2
  - bank 2
    -node 3
    -node 4
*/

memory_pool * create_memory_pool(size_t unit_size) {
//    printf("create memory_pool with unit_size %lu \n", unit_size);
    memory_pool *pool = calloc(1, sizeof(memory_pool));
    pool->unit_size = unit_size;

    pool_bank *bank = create_pool_bank(pool, 256); // calloc(1, sizeof(pool_bank));

    return pool;
}

pool_bank *create_pool_bank(memory_pool *pool, size_t units) {
//    printf("create pool_bank with units %lu \n", units);

    size_t extra_size = (sizeof(pool_node) + pool->unit_size) * units;
    pool_bank *bank = malloc(sizeof(pool_bank) + extra_size);
    bank->units = units;
    bank->unit_position = 0;
    bank->used_units = 0;
    bank->next = pool->first_bank;
    pool->first_bank = bank;
    return bank;
}

void free_pool_bank(memory_pool *pool, pool_bank *bank) {
    if (bank->prev != NULL) {
        bank->prev->next = bank->next;
    } else {
        pool->first_bank = bank->next;
    }
    if (bank->next != NULL) {
        bank->next->prev = bank->prev;
    }

    free(bank);
}

void *alloc_from_pool(memory_pool *pool) {
    if (pool->first_free_node != NULL) {
        pool_node *node = pool->first_free_node;
        pool->first_free_node = node->next;

        node->next = pool->first_used_node;
        pool->first_used_node = node;
        node->bank->used_units++;
        return (void *) (node + 1);
    } else {
        pool_bank * bank = pool->first_bank;
        if (bank->unit_position >= bank->units) {
            pool_bank * new_bank = create_pool_bank(pool, bank->units * 4);
            pool->first_bank = new_bank;
            new_bank->next = bank;
            bank->prev = new_bank;
            bank = new_bank;
        }

        unsigned char * bank_data = (unsigned char *) (bank + 1);
        pool_node * node = (pool_node *) &bank_data[bank->unit_position * (pool->unit_size + sizeof(pool_node))];
        bank->unit_position++;
        pool_node * next_node = pool->first_used_node;
        pool->first_used_node = node;
        node->next = next_node;
        node->prev = NULL;
        if (next_node != NULL) {
            next_node->prev = node;
        }

        return (void *) (node + 1);
    }
}

void free_from_pool(memory_pool *pool, void *data) {
    if (data == NULL) return;
    pool_node *node = data - sizeof(pool_node);

    node->bank->used_units--;
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        pool->first_used_node = node->next;
        node->next->prev = NULL;  
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    node->prev = NULL;
    node->next = pool->first_free_node;
    node->next->prev = node;

    if (node->bank->used_units == 0) {
        free_pool_bank(pool, node->bank);
    }
}