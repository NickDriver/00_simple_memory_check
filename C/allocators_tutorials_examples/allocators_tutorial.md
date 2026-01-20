# Memory Allocators: A Comprehensive Guide

This tutorial covers memory allocator design, implementation, and real-world use cases.

## Table of Contents

1. [Why Custom Allocators?](#why-custom-allocators)
2. [Allocator Types Overview](#allocator-types-overview)
3. [Bump Allocator (Arena)](#1-bump-allocator-arena)
4. [Pool Allocator](#2-pool-allocator)
5. [Free List Allocator](#3-free-list-allocator)
6. [Stack Allocator](#4-stack-allocator)
7. [Buddy Allocator](#5-buddy-allocator)
8. [Comparison Table](#comparison-table)
9. [Real-World Use Cases](#real-world-use-cases)
10. [Choosing the Right Allocator](#choosing-the-right-allocator)

---

## Why Custom Allocators?

The standard `malloc`/`free` are general-purpose but have drawbacks:

| Issue | Problem |
|-------|---------|
| **Speed** | `malloc` searches for free blocks, which takes time |
| **Fragmentation** | Memory becomes scattered with holes over time |
| **Overhead** | Each allocation stores metadata (typically 8-16 bytes) |
| **Cache misses** | Related data ends up scattered in memory |
| **Unpredictable** | Allocation time varies based on heap state |

Custom allocators solve these by trading generality for performance in specific scenarios.

---

## Allocator Types Overview

```
                    ┌─────────────────────────────────────────────────┐
                    │              ALLOCATOR TYPES                     │
                    └─────────────────────────────────────────────────┘

    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │    BUMP      │  │    POOL      │  │  FREE LIST   │  │    STACK     │
    │   (Arena)    │  │              │  │              │  │              │
    ├──────────────┤  ├──────────────┤  ├──────────────┤  ├──────────────┤
    │ Fastest      │  │ Fixed-size   │  │ Variable     │  │ LIFO order   │
    │ No free      │  │ O(1) alloc   │  │ sizes        │  │ O(1) both    │
    │ Bulk reset   │  │ O(1) free    │  │ Can fragment │  │ Limited use  │
    └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘

                              ┌──────────────┐
                              │    BUDDY     │
                              │              │
                              ├──────────────┤
                              │ Power-of-2   │
                              │ Low fragment │
                              │ Complex      │
                              └──────────────┘
```

---

## 1. Bump Allocator (Arena)

The simplest and fastest allocator. Just "bump" a pointer forward.

### How It Works

```
Initial state:
┌────────────────────────────────────────────────┐
│                  FREE SPACE                     │
└────────────────────────────────────────────────┘
▲
used = 0

After alloc(16):
┌────────────────┬───────────────────────────────┐
│   ALLOCATED    │          FREE SPACE            │
│    16 bytes    │                                │
└────────────────┴───────────────────────────────┘
                 ▲
                 used = 16

After alloc(8):
┌────────────────┬────────┬──────────────────────┐
│   ALLOCATED    │ ALLOC  │      FREE SPACE       │
│    16 bytes    │ 8 bytes│                       │
└────────────────┴────────┴──────────────────────┘
                          ▲
                          used = 24

After reset():
┌────────────────────────────────────────────────┐
│                  FREE SPACE                     │
└────────────────────────────────────────────────┘
▲
used = 0  (memory reused, not freed)
```

### Implementation

```c
typedef struct {
    uint8_t *memory;
    size_t size;
    size_t used;
} BumpAllocator;

void bump_init(BumpAllocator *a, void *memory, size_t size) {
    a->memory = memory;
    a->size = size;
    a->used = 0;
}

void *bump_alloc(BumpAllocator *a, size_t size) {
    // Align to 8 bytes
    size_t aligned = (size + 7) & ~7;

    if (a->used + aligned > a->size) {
        return NULL;
    }

    void *ptr = a->memory + a->used;
    a->used += aligned;
    return ptr;
}

void bump_reset(BumpAllocator *a) {
    a->used = 0;
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Alloc time | O(1) |
| Free time | N/A (no individual free) |
| Reset time | O(1) |
| Fragmentation | None |
| Overhead per alloc | 0 bytes |

### Use Cases

- **Game frames**: Allocate per-frame data, reset at frame end
- **Request handling**: Web server allocates per-request, resets after response
- **Parsing**: Compiler allocates AST nodes, frees all after compilation
- **Temporary calculations**: Math operations needing scratch space

### Example: Game Frame Allocator

```c
BumpAllocator frame_allocator;
uint8_t frame_memory[1024 * 1024];  // 1 MB per frame

void game_init(void) {
    bump_init(&frame_allocator, frame_memory, sizeof(frame_memory));
}

void game_update(void) {
    // Allocate temporary data for this frame
    Enemy *visible_enemies = bump_alloc(&frame_allocator, sizeof(Enemy) * 100);
    Particle *particles = bump_alloc(&frame_allocator, sizeof(Particle) * 1000);

    // ... process frame ...

    // Reset for next frame - instant, no matter how many allocations
    bump_reset(&frame_allocator);
}
```

---

## 2. Pool Allocator

Pre-allocates fixed-size blocks. Perfect when all allocations are the same size.

### How It Works

```
Initial state (block size = 32 bytes):
┌────────┬────────┬────────┬────────┬────────┐
│ FREE   │ FREE   │ FREE   │ FREE   │ FREE   │
│ next:1 │ next:2 │ next:3 │ next:4 │ next:∅ │
└────────┴────────┴────────┴────────┴────────┘
▲
free_list points here

After alloc() returns block 0:
┌────────┬────────┬────────┬────────┬────────┐
│ IN USE │ FREE   │ FREE   │ FREE   │ FREE   │
│        │ next:2 │ next:3 │ next:4 │ next:∅ │
└────────┴────────┴────────┴────────┴────────┘
          ▲
          free_list points here

After alloc() returns block 1:
┌────────┬────────┬────────┬────────┬────────┐
│ IN USE │ IN USE │ FREE   │ FREE   │ FREE   │
│        │        │ next:3 │ next:4 │ next:∅ │
└────────┴────────┴────────┴────────┴────────┘
                   ▲
                   free_list

After free(block 0):
┌────────┬────────┬────────┬────────┬────────┐
│ FREE   │ IN USE │ FREE   │ FREE   │ FREE   │
│ next:2 │        │ next:3 │ next:4 │ next:∅ │
└────────┴────────┴────────┴────────┴────────┘
▲
free_list (block 0 added to front)
```

### Implementation

```c
typedef struct PoolBlock {
    struct PoolBlock *next;  // Used only when block is free
} PoolBlock;

typedef struct {
    uint8_t *memory;
    size_t block_size;
    size_t block_count;
    PoolBlock *free_list;
} PoolAllocator;

void pool_init(PoolAllocator *p, void *memory, size_t block_size, size_t block_count) {
    p->memory = memory;
    p->block_size = block_size < sizeof(PoolBlock) ? sizeof(PoolBlock) : block_size;
    p->block_count = block_count;

    // Build free list
    p->free_list = NULL;
    for (size_t i = 0; i < block_count; i++) {
        PoolBlock *block = (PoolBlock *)(p->memory + i * p->block_size);
        block->next = p->free_list;
        p->free_list = block;
    }
}

void *pool_alloc(PoolAllocator *p) {
    if (p->free_list == NULL) {
        return NULL;
    }

    PoolBlock *block = p->free_list;
    p->free_list = block->next;
    return block;
}

void pool_free(PoolAllocator *p, void *ptr) {
    if (ptr == NULL) return;

    PoolBlock *block = ptr;
    block->next = p->free_list;
    p->free_list = block;
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Alloc time | O(1) |
| Free time | O(1) |
| Fragmentation | None (fixed sizes) |
| Overhead per alloc | 0 bytes (in-place free list) |
| Limitation | Single fixed size |

### Use Cases

- **Game entities**: All enemies same size, frequent spawn/despawn
- **Network connections**: Fixed-size connection structures
- **Object pools**: Database connection pools, thread pools
- **Particles**: Fixed-size particle structures

### Example: Entity Management

```c
typedef struct {
    float x, y, z;
    float health;
    int type;
    // ... total 64 bytes
} Entity;

PoolAllocator entity_pool;
uint8_t entity_memory[64 * 1000];  // 1000 entities max

void game_init(void) {
    pool_init(&entity_pool, entity_memory, sizeof(Entity), 1000);
}

Entity *spawn_enemy(float x, float y) {
    Entity *e = pool_alloc(&entity_pool);
    if (e) {
        e->x = x;
        e->y = y;
        e->health = 100;
    }
    return e;
}

void kill_enemy(Entity *e) {
    pool_free(&entity_pool, e);
}
```

---

## 3. Free List Allocator

Maintains a list of free blocks. Supports variable-size allocations with individual frees.

### How It Works

```
Initial state:
┌──────────────────────────────────────────────────────┐
│ HDR │              FREE BLOCK (1000 bytes)            │
│ 1000│                                                 │
└──────────────────────────────────────────────────────┘

After alloc(100):
┌──────────────────┬───────────────────────────────────┐
│ HDR │ ALLOCATED  │ HDR │     FREE BLOCK (876 bytes)   │
│ 100 │  100 bytes │ 876 │                              │
└──────────────────┴───────────────────────────────────┘

After alloc(200):
┌──────────────────┬───────────────────────┬───────────────────────┐
│ HDR │ ALLOCATED  │ HDR │   ALLOCATED     │ HDR │   FREE (652)     │
│ 100 │  100 bytes │ 200 │   200 bytes     │ 652 │                  │
└──────────────────┴───────────────────────┴───────────────────────┘

After free(first block):
┌──────────────────┬───────────────────────┬───────────────────────┐
│ HDR │   FREE     │ HDR │   ALLOCATED     │ HDR │   FREE (652)     │
│ 100 │  (in list) │ 200 │   200 bytes     │ 652 │   (in list)      │
└──────────────────┴───────────────────────┴───────────────────────┘
      ▲                                           ▲
      └─────── free_list ─────────────────────────┘
```

### Implementation

```c
typedef struct FreeBlock {
    size_t size;
    struct FreeBlock *next;
} FreeBlock;

typedef struct {
    uint8_t *memory;
    size_t size;
    FreeBlock *free_list;
} FreeListAllocator;

#define HEADER_SIZE (sizeof(size_t))
#define MIN_BLOCK_SIZE (sizeof(FreeBlock))

void freelist_init(FreeListAllocator *a, void *memory, size_t size) {
    a->memory = memory;
    a->size = size;

    // Start with one big free block
    a->free_list = (FreeBlock *)memory;
    a->free_list->size = size;
    a->free_list->next = NULL;
}

void *freelist_alloc(FreeListAllocator *a, size_t size) {
    size_t required = size + HEADER_SIZE;
    required = (required + 7) & ~7;  // Align
    if (required < MIN_BLOCK_SIZE) {
        required = MIN_BLOCK_SIZE;
    }

    // First-fit search
    FreeBlock **prev = &a->free_list;
    FreeBlock *block = a->free_list;

    while (block != NULL) {
        if (block->size >= required) {
            // Can we split this block?
            if (block->size >= required + MIN_BLOCK_SIZE) {
                // Split: create new free block after allocated portion
                FreeBlock *new_free = (FreeBlock *)((uint8_t *)block + required);
                new_free->size = block->size - required;
                new_free->next = block->next;
                *prev = new_free;
                block->size = required;
            } else {
                // Use entire block
                *prev = block->next;
            }

            // Return pointer after header
            return (uint8_t *)block + HEADER_SIZE;
        }
        prev = &block->next;
        block = block->next;
    }

    return NULL;  // No suitable block found
}

void freelist_free(FreeListAllocator *a, void *ptr) {
    if (ptr == NULL) return;

    // Get block header
    FreeBlock *block = (FreeBlock *)((uint8_t *)ptr - HEADER_SIZE);

    // Add to front of free list (simple approach)
    block->next = a->free_list;
    a->free_list = block;

    // Note: A production allocator would coalesce adjacent free blocks
}
```

### Allocation Strategies

| Strategy | How It Works | Pros | Cons |
|----------|--------------|------|------|
| **First-fit** | Use first block that fits | Fast search | Fragmentation at start |
| **Best-fit** | Use smallest block that fits | Less waste | Slow (searches all), tiny fragments |
| **Worst-fit** | Use largest block | Large leftovers | Poor overall |
| **Next-fit** | First-fit from last position | Spreads allocation | Can miss better fits |

### Characteristics

| Property | Value |
|----------|-------|
| Alloc time | O(n) where n = free blocks |
| Free time | O(1) simple, O(n) with coalescing |
| Fragmentation | Can fragment over time |
| Overhead per alloc | Header size (8+ bytes) |
| Flexibility | Any size allocation |

### Use Cases

- **General-purpose replacement** for malloc with more control
- **Memory-constrained systems** where you need to track usage
- **Custom game allocators** with debugging features
- **Embedded systems** without standard library

---

## 4. Stack Allocator

LIFO (Last In, First Out) allocations. Must free in reverse order.

### How It Works

```
Initial:
┌──────────────────────────────────────────────┐
│                    FREE                       │
└──────────────────────────────────────────────┘
▲
top

After alloc(A), alloc(B), alloc(C):
┌──────────┬──────────┬──────────┬─────────────┐
│    A     │    B     │    C     │    FREE     │
└──────────┴──────────┴──────────┴─────────────┘
                                 ▲
                                 top

free(C) - OK:
┌──────────┬──────────┬────────────────────────┐
│    A     │    B     │         FREE           │
└──────────┴──────────┴────────────────────────┘
                      ▲
                      top

free(A) - ERROR! Must free B first
```

### Implementation

```c
typedef struct {
    uint8_t *memory;
    size_t size;
    size_t top;

    // For validation (optional)
    size_t *markers;
    size_t marker_count;
} StackAllocator;

void stack_init(StackAllocator *s, void *memory, size_t size) {
    s->memory = memory;
    s->size = size;
    s->top = 0;
}

void *stack_alloc(StackAllocator *s, size_t size) {
    size_t aligned = (size + 7) & ~7;

    if (s->top + aligned > s->size) {
        return NULL;
    }

    void *ptr = s->memory + s->top;
    s->top += aligned;
    return ptr;
}

// Save current position (for batch free)
size_t stack_get_marker(StackAllocator *s) {
    return s->top;
}

// Free everything after marker
void stack_free_to_marker(StackAllocator *s, size_t marker) {
    if (marker <= s->top) {
        s->top = marker;
    }
}

void stack_reset(StackAllocator *s) {
    s->top = 0;
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Alloc time | O(1) |
| Free time | O(1) (with markers) |
| Fragmentation | None |
| Overhead per alloc | 0 bytes |
| Limitation | LIFO order required |

### Use Cases

- **Recursive algorithms**: Allocate on descent, free on return
- **Scoped allocations**: Function-local allocations
- **Undo systems**: Each action allocates, undo frees in reverse
- **Expression evaluation**: Push operands, pop results

### Example: Recursive File Processing

```c
StackAllocator work_stack;

void process_directory(const char *path) {
    // Mark current position
    size_t marker = stack_get_marker(&work_stack);

    // Allocate working memory for this level
    char *full_path = stack_alloc(&work_stack, 1024);
    DirEntry *entries = stack_alloc(&work_stack, sizeof(DirEntry) * 100);

    // Read directory...
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].is_directory) {
            process_directory(entries[i].name);  // Recursive call allocates more
        }
    }

    // Free everything allocated at this level and below
    stack_free_to_marker(&work_stack, marker);
}
```

---

## 5. Buddy Allocator

Splits memory into power-of-2 sized blocks. Efficient coalescing.

### How It Works

```
Initial (64 bytes total):
Level 0: [          64 bytes          ]
Level 1: [    32    ][    32    ]
Level 2: [ 16 ][ 16 ][ 16 ][ 16 ]
Level 3: [8][8][8][8][8][8][8][8]

Request: alloc(10) → needs 16 bytes (next power of 2)

Step 1: Split 64 into two 32s
[    32    ][    32    ]
     ▲ split

Step 2: Split first 32 into two 16s
[ 16 ][ 16 ][    32    ]
  ▲ split

Step 3: Return first 16
[USED][ 16 ][    32    ]
  ▲ allocated (10 bytes in 16-byte block)

After alloc(8):
[USED][8][8][    32    ]
        ▲ 16 split, first 8 used

After free(first block):
[FREE][8][8][    32    ]

After free(second block):
[  16 ][8][    32    ]
   ▲ buddies merged!
```

### Simplified Implementation

```c
#include <stdint.h>
#include <string.h>

#define MAX_LEVELS 10
#define MIN_BLOCK_SIZE 16

typedef struct {
    uint8_t *memory;
    size_t total_size;
    // Free lists for each level (level 0 = smallest)
    uint8_t *free_lists[MAX_LEVELS];
    size_t num_levels;
} BuddyAllocator;

static size_t size_to_level(BuddyAllocator *b, size_t size) {
    size_t block_size = MIN_BLOCK_SIZE;
    size_t level = 0;
    while (block_size < size && level < b->num_levels - 1) {
        block_size *= 2;
        level++;
    }
    return level;
}

static size_t level_to_size(size_t level) {
    return MIN_BLOCK_SIZE << level;
}

void buddy_init(BuddyAllocator *b, void *memory, size_t size) {
    b->memory = memory;
    b->total_size = size;

    // Calculate number of levels
    b->num_levels = 0;
    size_t s = MIN_BLOCK_SIZE;
    while (s <= size) {
        b->free_lists[b->num_levels] = NULL;
        b->num_levels++;
        s *= 2;
    }

    // Entire memory is one free block at highest level
    b->free_lists[b->num_levels - 1] = memory;
    *(uint8_t **)memory = NULL;  // Next pointer = NULL
}

void *buddy_alloc(BuddyAllocator *b, size_t size) {
    if (size == 0) return NULL;

    size_t level = size_to_level(b, size);

    // Find a free block at this level or higher
    size_t search_level = level;
    while (search_level < b->num_levels && b->free_lists[search_level] == NULL) {
        search_level++;
    }

    if (search_level >= b->num_levels) {
        return NULL;  // No space
    }

    // Remove block from free list
    uint8_t *block = b->free_lists[search_level];
    b->free_lists[search_level] = *(uint8_t **)block;

    // Split down to required level
    while (search_level > level) {
        search_level--;
        size_t half_size = level_to_size(search_level);

        // Add buddy (second half) to free list
        uint8_t *buddy = block + half_size;
        *(uint8_t **)buddy = b->free_lists[search_level];
        b->free_lists[search_level] = buddy;
    }

    return block;
}

void buddy_free(BuddyAllocator *b, void *ptr, size_t size) {
    if (ptr == NULL) return;

    uint8_t *block = ptr;
    size_t level = size_to_level(b, size);

    // Try to merge with buddy
    while (level < b->num_levels - 1) {
        size_t block_size = level_to_size(level);
        size_t offset = block - b->memory;

        // Find buddy address
        uint8_t *buddy;
        if ((offset / block_size) % 2 == 0) {
            buddy = block + block_size;  // Buddy is after
        } else {
            buddy = block - block_size;  // Buddy is before
        }

        // Check if buddy is free (search free list)
        uint8_t **prev = &b->free_lists[level];
        uint8_t *curr = b->free_lists[level];
        int found = 0;

        while (curr != NULL) {
            if (curr == buddy) {
                // Remove buddy from free list
                *prev = *(uint8_t **)curr;
                found = 1;
                break;
            }
            prev = (uint8_t **)curr;
            curr = *(uint8_t **)curr;
        }

        if (!found) break;  // Buddy not free, can't merge

        // Merge: use lower address as new block
        if (buddy < block) {
            block = buddy;
        }
        level++;
    }

    // Add block to free list
    *(uint8_t **)block = b->free_lists[level];
    b->free_lists[level] = block;
}
```

### Characteristics

| Property | Value |
|----------|-------|
| Alloc time | O(log n) |
| Free time | O(log n) with coalescing |
| Fragmentation | Internal (power-of-2 rounding) |
| Overhead per alloc | Depends on implementation |
| Coalescing | Automatic and efficient |

### Use Cases

- **Operating system kernels**: Linux uses buddy for page allocation
- **Large memory systems**: When coalescing is important
- **Virtual memory managers**: Page-level allocation
- **Memory pools with varied sizes**: When you need efficient merging

---

## Comparison Table

| Allocator | Alloc | Free | Fragmentation | Overhead | Best For |
|-----------|-------|------|---------------|----------|----------|
| **Bump** | O(1) | N/A | None | 0 | Temporary, batch-freed data |
| **Pool** | O(1) | O(1) | None | 0 | Fixed-size objects |
| **Free List** | O(n) | O(1)* | External | 8+ bytes | Variable sizes, general use |
| **Stack** | O(1) | O(1) | None | 0 | LIFO patterns, recursion |
| **Buddy** | O(log n) | O(log n) | Internal | Varies | Large systems, kernels |

*O(n) with coalescing

---

## Real-World Use Cases

### Video Games

```
┌─────────────────────────────────────────────────────────────┐
│                    GAME MEMORY LAYOUT                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ PERMANENT    │  │ LEVEL        │  │ FRAME        │       │
│  │ (Free List)  │  │ (Bump)       │  │ (Bump)       │       │
│  ├──────────────┤  ├──────────────┤  ├──────────────┤       │
│  │ - Engine     │  │ - Geometry   │  │ - Visible    │       │
│  │ - Textures   │  │ - Enemies    │  │   objects    │       │
│  │ - Audio      │  │ - Scripts    │  │ - Particles  │       │
│  │              │  │              │  │ - UI temp    │       │
│  │ Freed on     │  │ Reset on     │  │ Reset every  │       │
│  │ shutdown     │  │ level load   │  │ frame        │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
│                                                              │
│  ┌──────────────┐                                           │
│  │ ENTITIES     │                                           │
│  │ (Pool)       │                                           │
│  ├──────────────┤                                           │
│  │ - Bullets    │  Fixed size, fast spawn/despawn           │
│  │ - Pickups    │                                           │
│  │ - Effects    │                                           │
│  └──────────────┘                                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Web Server

```c
// Per-request arena allocator
void handle_request(Request *req) {
    BumpAllocator arena;
    uint8_t request_memory[64 * 1024];  // 64 KB per request
    bump_init(&arena, request_memory, sizeof(request_memory));

    // All allocations for this request come from arena
    Headers *headers = bump_alloc(&arena, sizeof(Headers));
    char *body = bump_alloc(&arena, req->content_length);
    Response *resp = bump_alloc(&arena, sizeof(Response));

    // Process request...

    // Send response...

    // No cleanup needed! Arena memory on stack, released automatically
}
```

### Compiler / Parser

```c
// AST nodes all same size? Use pool
PoolAllocator ast_pool;

// Source file processing with stack allocator
StackAllocator parse_stack;

ASTNode *parse_expression(void) {
    size_t marker = stack_get_marker(&parse_stack);

    // Temporary parse state
    Token *tokens = stack_alloc(&parse_stack, sizeof(Token) * 100);

    // Final AST node goes in pool (persists after parsing)
    ASTNode *node = pool_alloc(&ast_pool);

    // Free temporary state
    stack_free_to_marker(&parse_stack, marker);

    return node;
}
```

### Embedded System

```c
// Static memory - no malloc available
static uint8_t sensor_memory[1024];
static uint8_t message_memory[2048];

PoolAllocator sensor_pool;   // Fixed-size sensor readings
BumpAllocator message_arena; // Variable-size messages, reset after send

void embedded_init(void) {
    pool_init(&sensor_pool, sensor_memory, sizeof(SensorReading), 32);
    bump_init(&message_arena, message_memory, sizeof(message_memory));
}
```

---

## Choosing the Right Allocator

```
                        START
                          │
                          ▼
                ┌───────────────────┐
                │ All allocations   │
                │ same size?        │
                └───────────────────┘
                    │           │
                   YES          NO
                    │           │
                    ▼           ▼
              ┌─────────┐  ┌───────────────────┐
              │  POOL   │  │ Can free all at   │
              └─────────┘  │ once (batch)?     │
                           └───────────────────┘
                               │           │
                              YES          NO
                               │           │
                               ▼           ▼
                         ┌─────────┐  ┌───────────────────┐
                         │  BUMP   │  │ Must free in      │
                         │ (Arena) │  │ reverse order?    │
                         └─────────┘  └───────────────────┘
                                          │           │
                                         YES          NO
                                          │           │
                                          ▼           ▼
                                    ┌─────────┐  ┌───────────────────┐
                                    │  STACK  │  │ Need efficient    │
                                    └─────────┘  │ coalescing?       │
                                                 └───────────────────┘
                                                     │           │
                                                    YES          NO
                                                     │           │
                                                     ▼           ▼
                                               ┌─────────┐  ┌───────────┐
                                               │  BUDDY  │  │ FREE LIST │
                                               └─────────┘  └───────────┘
```

### Quick Decision Guide

| Scenario | Recommended |
|----------|-------------|
| Temporary per-frame data | Bump |
| Game entities (spawn/despawn) | Pool |
| Unknown lifetime, variable size | Free List |
| Recursive algorithms | Stack |
| OS kernel page allocation | Buddy |
| Per-request web handling | Bump |
| Fixed message buffers | Pool |
| General embedded use | Free List or Pool |

---

## Further Reading

- **"Game Engine Architecture"** by Jason Gregory - Detailed game allocator patterns
- **Linux kernel source** - `mm/page_alloc.c` for buddy allocator
- **"Efficient Memory Management"** - Academic papers on allocator design
- **jemalloc / tcmalloc source** - Production-quality allocator implementations
