/*
 * Simple memory allocator
 *
 * This is a bump/arena allocator that allocates memory from a pre-allocated pool.
 * Allocations are fast (O(1)) but individual frees are not supported.
 * Use reset() to free all allocations at once, or destroy() to free the pool.
 */

#include "simple_memory_allocator.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

// Initialize the memory allocator to zero state
void simple_memory_allocator_init(SimpleMemoryAllocator *allocator) {
    allocator->memory = NULL;
    allocator->size = 0;
    allocator->used = 0;
}

// Create allocator with a memory pool of given size
// Returns 0 on success, -1 on failure
int simple_memory_allocator_create(SimpleMemoryAllocator *allocator, size_t pool_size) {
    if (allocator == NULL || pool_size == 0) {
        return -1;
    }

    void *memory = malloc(pool_size);
    if (memory == NULL) {
        return -1;
    }

    allocator->memory = memory;
    allocator->size = pool_size;
    allocator->used = 0;

    return 0;
}

// Allocate memory from the pool
// Returns pointer to allocated memory, or NULL if not enough space
void *simple_memory_allocator_alloc(SimpleMemoryAllocator *allocator, size_t size) {
    if (allocator == NULL || allocator->memory == NULL || size == 0) {
        return NULL;
    }

    // Align to 8 bytes for better performance
    size_t aligned_size = (size + 7) & ~((size_t)7);

    if (allocator->used + aligned_size > allocator->size) {
        return NULL;  // Not enough space
    }

    void *ptr = (uint8_t *)allocator->memory + allocator->used;
    allocator->used += aligned_size;

    return ptr;
}

// Reset allocator - keeps the pool but marks all memory as free
void simple_memory_allocator_reset(SimpleMemoryAllocator *allocator) {
    if (allocator != NULL) {
        allocator->used = 0;
    }
}

// Destroy allocator and free the memory pool
void simple_memory_allocator_destroy(SimpleMemoryAllocator *allocator) {
    if (allocator != NULL) {
        free(allocator->memory);
        allocator->memory = NULL;
        allocator->size = 0;
        allocator->used = 0;
    }
}

// Print allocator status with formatted output
void simple_memory_allocator_print_status(const SimpleMemoryAllocator *allocator) {
    if (allocator == NULL) {
        printf("┌─────────────────────────────────────┐\n");
        printf("│  ALLOCATOR STATUS: NULL             │\n");
        printf("└─────────────────────────────────────┘\n");
        return;
    }

    size_t free_bytes = allocator->size - allocator->used;
    double usage_percent = allocator->size > 0
        ? (double)allocator->used / allocator->size * 100.0
        : 0.0;

    // Create visual progress bar (20 chars wide)
    int bar_width = 20;
    int filled = (int)(usage_percent / 100.0 * bar_width);

    printf("┌─────────────────────────────────────┐\n");
    printf("│       MEMORY ALLOCATOR STATUS       │\n");
    printf("├─────────────────────────────────────┤\n");
    printf("│ Pool Address: %p      │\n", allocator->memory);
    printf("│ Total Size:   %10zu bytes     │\n", allocator->size);
    printf("│ Used:         %10zu bytes     │\n", allocator->used);
    printf("│ Free:         %10zu bytes     │\n", free_bytes);
    printf("├─────────────────────────────────────┤\n");
    printf("│ Usage: [");

    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }

    printf("] %5.1f%%  │\n", usage_percent);
    printf("└─────────────────────────────────────┘\n");
}
