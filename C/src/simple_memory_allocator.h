#ifndef SIMPLE_MEMORY_ALLOCATOR_H
#define SIMPLE_MEMORY_ALLOCATOR_H

#include <stddef.h>

typedef struct {
    void *memory;
    size_t size;
    size_t used;
} SimpleMemoryAllocator;

// Initialize allocator struct to zero state
void simple_memory_allocator_init(SimpleMemoryAllocator *allocator);

// Create allocator with a memory pool of given size
int simple_memory_allocator_create(SimpleMemoryAllocator *allocator, size_t pool_size);

// Allocate memory from the pool (returns NULL if not enough space)
void *simple_memory_allocator_alloc(SimpleMemoryAllocator *allocator, size_t size);

// Reset allocator (keeps pool, resets used counter to 0)
void simple_memory_allocator_reset(SimpleMemoryAllocator *allocator);

// Destroy allocator and free the memory pool
void simple_memory_allocator_destroy(SimpleMemoryAllocator *allocator);

// Print allocator status with formatted output
void simple_memory_allocator_print_status(const SimpleMemoryAllocator *allocator);

#endif
