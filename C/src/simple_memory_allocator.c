/*
 * Simple memory allocator
 *
 * This allocator is a simple allocator that allocates memory from the heap.

 */

#include "simple_memory_allocator.h"

// Define the struct for the memory allocator
typedef struct {
    void *memory;
    size_t size;
    size_t used;
} SimpleMemoryAllocator;

// Initialize the memory allocator
void simple_memory_allocator_init(SimpleMemoryAllocator *allocator) {
    allocator->memory = NULL;
    allocator->size = 0;
    allocator->used = 0;
}