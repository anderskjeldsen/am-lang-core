
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

#define MAX_BANK_UNITS (32 * 1024)
#define SMALL_OBJECT_MAX_SIZE = 128

memory_pool _small_object_memory_pool = {
    .unit_size SMALL_OBJECT_MAX_SIZE,
    .first_bank = NULL,
    .first_bank_with_free_nodes = NULL,
//    .first_free_node = NULL,
    .first_used_node = NULL,
};

memory_pool * small_object_memory_pool = &_small_object_memory_pool;

memory_pool * create_memory_pool(size_t unit_size) {
//    printf("create memory_pool with unit_size %lu \n", unit_size);
    memory_pool *pool = malloc(sizeof(memory_pool));
    pool->unit_size = unit_size;

    pool_bank *bank = create_pool_bank(pool, 256); // calloc(1, sizeof(pool_bank));
    pool->first_bank_with_free_nodes = NULL;

    return pool;
}

void free_memory_pool(memory_pool *pool) {
    free_pool_banks(pool);
    free(pool);
}

void free_pool_banks(memory_pool *pool) {
    pool_bank *current = pool->first_bank;
    while(current != NULL) {
        if (current->used_units > 0) {
            printf("Pool bank still used\n");
        }
        pool_bank *next = current->next;
        free(current);
        current = next;
    }
}

pool_bank *create_pool_bank(memory_pool *pool, size_t units) {
//    printf("create pool_bank with units %lu \n", units);

    size_t extra_size = (sizeof(pool_node) + pool->unit_size) * units;
    pool_bank *bank = malloc(sizeof(pool_bank) + extra_size);
    bank->first_free_node = NULL;
    bank->units = units;
    bank->unit_position = 0;
    bank->used_units = 0;
    pool_bank *old = pool->first_bank;
    bank->next = old;
    pool->first_bank = bank;
    if (old != NULL) {
        old->prev = bank;
    }
    return bank;
}

void free_pool_bank(memory_pool *pool, pool_bank *bank) {
    printf("free pool bank, size %lu\n", bank->units);
    if (bank->prev != NULL) {
        bank->prev->next = bank->next;
    } else {
        pool->first_bank = bank->next;
    }
    if (bank->next != NULL) {
        bank->next->prev = bank->prev;
    }
    if (pool->first_bank_with_free_nodes == bank) {
        pool->first_bank_with_free_nodes = NULL;
    }

    free(bank);
    printf("Freed\n");
}

int allocated_objects = 0;

pool_node *extract_free_node(memory_pool  *pool) {
    pool_bank *bank = pool->first_bank_with_free_nodes;
    if (bank == NULL) {
        return NULL;
    }
    pool_node *node = bank->first_free_node;
    if (node != NULL) {
        bank->first_free_node = node->next;
        bank->used_units++;
        node->next = NULL;
    }
    return node;
}

void *alloc_from_pool(memory_pool *pool) {
    allocated_objects++;
    if (allocated_objects % 1000000 == 0) {
        printf("Allocated another 1000000 objects: %d\n", allocated_objects);
    }

    pool_bank * bank = pool->first_bank;
    if (bank->unit_position >= bank->units) {
        pool_node *node2 = extract_free_node(pool);
        if ( node2 != NULL) {
            node2->next = pool->first_used_node;
            pool->first_used_node = node2;
            return (void *) (node2 + 1);
        }
        int new_bank_units = bank->units * 4;
        if (new_bank_units > MAX_BANK_UNITS) new_bank_units = MAX_BANK_UNITS;
        bank = create_pool_bank(pool, new_bank_units);
    }

    unsigned char * bank_data = (unsigned char *) (bank + 1);
    pool_node * node = (pool_node *) &bank_data[bank->unit_position * (pool->unit_size + sizeof(pool_node))];
    node->bank = bank;
    bank->used_units++;
//        node->used = true;

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

void move_bank_up(memory_pool *pool, pool_bank *bank) {
    if (bank != pool->first_bank) return;

    if (bank->first_free_node != NULL && pool->first_bank->first_free_node == NULL) {

        bank->prev->next = bank->next;
        bank->next->prev = bank->prev;

        bank->next = pool->first_bank;
        bank->prev = NULL;
        pool->first_bank = bank; 
    }
    
}

int freed_objects = 0;

void free_from_pool(memory_pool *pool, void *data) {
    if (data == NULL) return;
    freed_objects++;
    if (freed_objects % 1000000 == 0) {
        printf("Freed another 1000000 objects: %d\n", freed_objects);
    }

    pool_node *node = data - sizeof(pool_node);
    node->bank->used_units--;
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        pool->first_used_node = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    node->prev = NULL;
    node->next = node->bank->first_free_node;
    node->bank->first_free_node = node;
    if (node->next != NULL) {
        node->next->prev = node;
    }

    if (node->bank->used_units == 0) {                
        free_pool_bank(pool, node->bank);
    } else {
        move_bank_up(pool, node->bank);
    }
}