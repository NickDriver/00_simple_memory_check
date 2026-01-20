/*
 * allocators.h - Collection of memory allocator implementations
 *
 * This header contains complete implementations of various allocator types.
 * Include this file and use whichever allocator suits your needs.
 */

#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * BUMP ALLOCATOR (Arena)
 * ============================================================================
 * Fastest allocator. No individual frees - reset all at once.
 */

typedef struct {
    uint8_t *memory;
    size_t size;
    size_t used;
} BumpAllocator;

static inline void bump_init(BumpAllocator *a, void *memory, size_t size) {
    a->memory = (uint8_t *)memory;
    a->size = size;
    a->used = 0;
}

static inline void *bump_alloc(BumpAllocator *a, size_t size) {
    size_t aligned = (size + 7) & ~((size_t)7);

    if (a->used + aligned > a->size) {
        return NULL;
    }

    void *ptr = a->memory + a->used;
    a->used += aligned;
    return ptr;
}

static inline void bump_reset(BumpAllocator *a) {
    a->used = 0;
}

static inline size_t bump_used(BumpAllocator *a) {
    return a->used;
}

static inline size_t bump_remaining(BumpAllocator *a) {
    return a->size - a->used;
}

/* ============================================================================
 * POOL ALLOCATOR
 * ============================================================================
 * Fixed-size blocks. O(1) alloc and free.
 */

typedef struct PoolFreeBlock {
    struct PoolFreeBlock *next;
} PoolFreeBlock;

typedef struct {
    uint8_t *memory;
    size_t block_size;
    size_t block_count;
    size_t used_count;
    PoolFreeBlock *free_list;
} PoolAllocator;

static inline void pool_init(PoolAllocator *p, void *memory, size_t block_size, size_t block_count) {
    p->memory = (uint8_t *)memory;
    p->block_size = block_size < sizeof(PoolFreeBlock) ? sizeof(PoolFreeBlock) : block_size;
    p->block_count = block_count;
    p->used_count = 0;

    // Build free list (link all blocks)
    p->free_list = NULL;
    for (size_t i = 0; i < block_count; i++) {
        PoolFreeBlock *block = (PoolFreeBlock *)(p->memory + i * p->block_size);
        block->next = p->free_list;
        p->free_list = block;
    }
}

static inline void *pool_alloc(PoolAllocator *p) {
    if (p->free_list == NULL) {
        return NULL;
    }

    PoolFreeBlock *block = p->free_list;
    p->free_list = block->next;
    p->used_count++;

    return block;
}

static inline void pool_free(PoolAllocator *p, void *ptr) {
    if (ptr == NULL) return;

    PoolFreeBlock *block = (PoolFreeBlock *)ptr;
    block->next = p->free_list;
    p->free_list = block;
    p->used_count--;
}

static inline size_t pool_used(PoolAllocator *p) {
    return p->used_count;
}

static inline size_t pool_available(PoolAllocator *p) {
    return p->block_count - p->used_count;
}

/* ============================================================================
 * STACK ALLOCATOR
 * ============================================================================
 * LIFO allocations with markers for batch freeing.
 */

typedef size_t StackMarker;

typedef struct {
    uint8_t *memory;
    size_t size;
    size_t top;
} StackAllocator;

static inline void stack_init(StackAllocator *s, void *memory, size_t size) {
    s->memory = (uint8_t *)memory;
    s->size = size;
    s->top = 0;
}

static inline void *stack_alloc(StackAllocator *s, size_t size) {
    size_t aligned = (size + 7) & ~((size_t)7);

    if (s->top + aligned > s->size) {
        return NULL;
    }

    void *ptr = s->memory + s->top;
    s->top += aligned;
    return ptr;
}

static inline StackMarker stack_get_marker(StackAllocator *s) {
    return s->top;
}

static inline void stack_free_to_marker(StackAllocator *s, StackMarker marker) {
    if (marker <= s->top) {
        s->top = marker;
    }
}

static inline void stack_reset(StackAllocator *s) {
    s->top = 0;
}

static inline size_t stack_used(StackAllocator *s) {
    return s->top;
}

/* ============================================================================
 * FREE LIST ALLOCATOR
 * ============================================================================
 * Variable-size allocations with individual free support.
 */

typedef struct FreeListBlock {
    size_t size;
    struct FreeListBlock *next;
} FreeListBlock;

typedef struct {
    uint8_t *memory;
    size_t size;
    FreeListBlock *free_list;
    size_t used;
} FreeListAllocator;

#define FREELIST_HEADER_SIZE sizeof(size_t)
#define FREELIST_MIN_BLOCK sizeof(FreeListBlock)

static inline void freelist_init(FreeListAllocator *a, void *memory, size_t size) {
    a->memory = (uint8_t *)memory;
    a->size = size;
    a->used = 0;

    // Start with one big free block
    a->free_list = (FreeListBlock *)memory;
    a->free_list->size = size;
    a->free_list->next = NULL;
}

static inline void *freelist_alloc(FreeListAllocator *a, size_t size) {
    if (size == 0) return NULL;

    size_t required = size + FREELIST_HEADER_SIZE;
    required = (required + 7) & ~((size_t)7);  // Align
    if (required < FREELIST_MIN_BLOCK) {
        required = FREELIST_MIN_BLOCK;
    }

    // First-fit search
    FreeListBlock **prev = &a->free_list;
    FreeListBlock *block = a->free_list;

    while (block != NULL) {
        if (block->size >= required) {
            // Split if enough space for another block
            if (block->size >= required + FREELIST_MIN_BLOCK) {
                FreeListBlock *new_free = (FreeListBlock *)((uint8_t *)block + required);
                new_free->size = block->size - required;
                new_free->next = block->next;
                *prev = new_free;
                block->size = required;
            } else {
                *prev = block->next;
                required = block->size;  // Use full block
            }

            a->used += required;
            return (uint8_t *)block + FREELIST_HEADER_SIZE;
        }
        prev = &block->next;
        block = block->next;
    }

    return NULL;
}

static inline void freelist_free(FreeListAllocator *a, void *ptr) {
    if (ptr == NULL) return;

    FreeListBlock *block = (FreeListBlock *)((uint8_t *)ptr - FREELIST_HEADER_SIZE);
    a->used -= block->size;

    // Insert sorted by address (enables coalescing)
    FreeListBlock **prev = &a->free_list;
    FreeListBlock *curr = a->free_list;

    while (curr != NULL && curr < block) {
        prev = &curr->next;
        curr = curr->next;
    }

    block->next = curr;
    *prev = block;

    // Coalesce with next block if adjacent
    if (block->next != NULL) {
        uint8_t *block_end = (uint8_t *)block + block->size;
        if (block_end == (uint8_t *)block->next) {
            block->size += block->next->size;
            block->next = block->next->next;
        }
    }

    // Coalesce with previous block if adjacent
    if (prev != &a->free_list) {
        // Find the block before 'block' in the free list
        FreeListBlock *scan = a->free_list;
        FreeListBlock *prev_scan = NULL;
        while (scan != NULL && scan != block) {
            prev_scan = scan;
            scan = scan->next;
        }
        if (prev_scan != NULL) {
            uint8_t *prev_end = (uint8_t *)prev_scan + prev_scan->size;
            if (prev_end == (uint8_t *)block) {
                prev_scan->size += block->size;
                prev_scan->next = block->next;
            }
        }
    }
}

static inline size_t freelist_used(FreeListAllocator *a) {
    return a->used;
}

/* ============================================================================
 * HELPER MACROS
 * ============================================================================
 */

// Calculate required memory for pool allocator
#define POOL_MEMORY_SIZE(block_size, count) ((block_size) * (count))

// Calculate aligned size
#define ALIGN_UP(value, alignment) (((value) + ((alignment) - 1)) & ~((alignment) - 1))

// Check if value is aligned
#define IS_ALIGNED(value, alignment) (((value) & ((alignment) - 1)) == 0)

#endif /* ALLOCATORS_H */
