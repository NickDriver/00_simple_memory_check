/*
 * allocator_examples.c - Demonstrations of each allocator type
 *
 * Compile: gcc -o allocator_examples allocator_examples.c -I../src
 * Run: ./allocator_examples
 */

#include <stdio.h>
#include <string.h>
#include "allocators.h"

/* ============================================================================
 * Example 1: Bump Allocator - Per-frame game allocations
 * ============================================================================
 */

typedef struct {
    float x, y, z;
    float radius;
} Particle;

void example_bump_allocator(void) {
    printf("\n=== BUMP ALLOCATOR EXAMPLE ===\n");
    printf("Use case: Per-frame temporary allocations\n\n");

    // Create memory pool (in real game, this might be much larger)
    uint8_t frame_memory[4096];
    BumpAllocator frame_alloc;
    bump_init(&frame_alloc, frame_memory, sizeof(frame_memory));

    // Simulate 3 game frames
    for (int frame = 0; frame < 3; frame++) {
        printf("Frame %d:\n", frame);

        // Allocate particles for this frame
        int particle_count = 10 + frame * 5;
        Particle *particles = bump_alloc(&frame_alloc, sizeof(Particle) * particle_count);

        if (particles) {
            printf("  Allocated %d particles (%zu bytes)\n",
                   particle_count, sizeof(Particle) * particle_count);

            // Initialize particles
            for (int i = 0; i < particle_count; i++) {
                particles[i].x = (float)i;
                particles[i].y = (float)(i * 2);
                particles[i].z = 0;
                particles[i].radius = 1.0f;
            }
        }

        // Allocate temporary string buffer
        char *debug_msg = bump_alloc(&frame_alloc, 256);
        if (debug_msg) {
            snprintf(debug_msg, 256, "Frame %d: %d particles active", frame, particle_count);
            printf("  Debug: %s\n", debug_msg);
        }

        printf("  Memory used: %zu / %zu bytes\n", bump_used(&frame_alloc), sizeof(frame_memory));

        // Reset for next frame - instant, O(1)
        bump_reset(&frame_alloc);
        printf("  Reset! Memory used: %zu bytes\n\n", bump_used(&frame_alloc));
    }
}

/* ============================================================================
 * Example 2: Pool Allocator - Entity management
 * ============================================================================
 */

typedef struct {
    int id;
    float x, y;
    int health;
    int active;
} Enemy;

void example_pool_allocator(void) {
    printf("\n=== POOL ALLOCATOR EXAMPLE ===\n");
    printf("Use case: Fixed-size game entities (spawn/despawn)\n\n");

    // Create pool for up to 5 enemies
    uint8_t enemy_memory[sizeof(Enemy) * 5];
    PoolAllocator enemy_pool;
    pool_init(&enemy_pool, enemy_memory, sizeof(Enemy), 5);

    printf("Pool created: %zu enemies max, %zu available\n\n",
           (size_t)5, pool_available(&enemy_pool));

    // Spawn some enemies
    Enemy *enemies[5] = {NULL};

    for (int i = 0; i < 3; i++) {
        enemies[i] = pool_alloc(&enemy_pool);
        if (enemies[i]) {
            enemies[i]->id = i;
            enemies[i]->x = (float)(i * 100);
            enemies[i]->y = 50.0f;
            enemies[i]->health = 100;
            enemies[i]->active = 1;
            printf("Spawned enemy %d at (%.0f, %.0f)\n",
                   enemies[i]->id, enemies[i]->x, enemies[i]->y);
        }
    }
    printf("Pool: %zu used, %zu available\n\n", pool_used(&enemy_pool), pool_available(&enemy_pool));

    // Kill enemy 1
    printf("Enemy 1 killed!\n");
    pool_free(&enemy_pool, enemies[1]);
    enemies[1] = NULL;
    printf("Pool: %zu used, %zu available\n\n", pool_used(&enemy_pool), pool_available(&enemy_pool));

    // Spawn 3 more enemies (should succeed - one slot freed + 2 unused)
    for (int i = 0; i < 3; i++) {
        Enemy *e = pool_alloc(&enemy_pool);
        if (e) {
            e->id = 10 + i;
            e->health = 100;
            printf("Spawned enemy %d\n", e->id);
        } else {
            printf("Failed to spawn enemy (pool full)\n");
        }
    }
    printf("Pool: %zu used, %zu available\n", pool_used(&enemy_pool), pool_available(&enemy_pool));
}

/* ============================================================================
 * Example 3: Stack Allocator - Recursive processing
 * ============================================================================
 */

void process_level(StackAllocator *stack, int depth, int max_depth) {
    if (depth > max_depth) return;

    // Save marker before allocating
    StackMarker marker = stack_get_marker(stack);

    // Allocate working memory for this recursion level
    size_t buffer_size = 64;
    char *buffer = stack_alloc(stack, buffer_size);

    if (buffer) {
        snprintf(buffer, buffer_size, "Level %d working data", depth);
        printf("%*sEnter level %d: \"%s\" (stack used: %zu)\n",
               depth * 2, "", depth, buffer, stack_used(stack));

        // Recurse deeper
        process_level(stack, depth + 1, max_depth);

        printf("%*sExit level %d (stack used: %zu)\n",
               depth * 2, "", depth, stack_used(stack));
    }

    // Free everything allocated at this level
    stack_free_to_marker(stack, marker);
}

void example_stack_allocator(void) {
    printf("\n=== STACK ALLOCATOR EXAMPLE ===\n");
    printf("Use case: Recursive algorithms with scoped allocations\n\n");

    uint8_t stack_memory[1024];
    StackAllocator stack;
    stack_init(&stack, stack_memory, sizeof(stack_memory));

    printf("Processing with recursion depth 4:\n\n");
    process_level(&stack, 0, 4);

    printf("\nAfter recursion, stack used: %zu (all freed!)\n", stack_used(&stack));
}

/* ============================================================================
 * Example 4: Free List Allocator - Variable-size allocations
 * ============================================================================
 */

void example_freelist_allocator(void) {
    printf("\n=== FREE LIST ALLOCATOR EXAMPLE ===\n");
    printf("Use case: Variable-size allocations with individual frees\n\n");

    uint8_t heap_memory[1024];
    FreeListAllocator heap;
    freelist_init(&heap, heap_memory, sizeof(heap_memory));

    printf("Heap initialized: 1024 bytes\n\n");

    // Allocate various sizes
    void *ptr1 = freelist_alloc(&heap, 100);
    printf("Allocated 100 bytes at %p (used: %zu)\n", ptr1, freelist_used(&heap));

    void *ptr2 = freelist_alloc(&heap, 200);
    printf("Allocated 200 bytes at %p (used: %zu)\n", ptr2, freelist_used(&heap));

    void *ptr3 = freelist_alloc(&heap, 50);
    printf("Allocated 50 bytes at %p (used: %zu)\n", ptr3, freelist_used(&heap));

    // Free middle allocation
    printf("\nFreeing 200-byte block...\n");
    freelist_free(&heap, ptr2);
    printf("After free (used: %zu)\n", freelist_used(&heap));

    // Allocate something that fits in the freed space
    void *ptr4 = freelist_alloc(&heap, 150);
    printf("\nAllocated 150 bytes at %p (reused freed space!)\n", ptr4);
    printf("Used: %zu bytes\n", freelist_used(&heap));

    // Free all
    freelist_free(&heap, ptr1);
    freelist_free(&heap, ptr3);
    freelist_free(&heap, ptr4);
    printf("\nAfter freeing all (used: %zu)\n", freelist_used(&heap));
}

/* ============================================================================
 * Example 5: Combined usage - Real game scenario
 * ============================================================================
 */

typedef struct {
    char name[32];
    int type;
} Item;

void example_combined_usage(void) {
    printf("\n=== COMBINED ALLOCATORS EXAMPLE ===\n");
    printf("Use case: Real game with multiple allocator types\n\n");

    // Permanent allocator for long-lived data (items, config)
    uint8_t permanent_memory[2048];
    FreeListAllocator permanent;
    freelist_init(&permanent, permanent_memory, sizeof(permanent_memory));

    // Pool for fixed-size entities
    uint8_t entity_memory[sizeof(Enemy) * 10];
    PoolAllocator entities;
    pool_init(&entities, entity_memory, sizeof(Enemy), 10);

    // Frame allocator for temporary data
    uint8_t frame_memory[1024];
    BumpAllocator frame;
    bump_init(&frame, frame_memory, sizeof(frame_memory));

    printf("Memory layout:\n");
    printf("  Permanent (FreeList): 2048 bytes - long-lived data\n");
    printf("  Entities (Pool): %zu bytes - %d enemies max\n",
           sizeof(entity_memory), 10);
    printf("  Frame (Bump): 1024 bytes - per-frame temporaries\n\n");

    // === Game initialization ===
    printf("=== INITIALIZATION ===\n");

    // Load items into permanent storage
    Item *sword = freelist_alloc(&permanent, sizeof(Item));
    strcpy(sword->name, "Iron Sword");
    sword->type = 1;

    Item *potion = freelist_alloc(&permanent, sizeof(Item));
    strcpy(potion->name, "Health Potion");
    potion->type = 2;

    printf("Loaded items: %s, %s\n", sword->name, potion->name);
    printf("Permanent memory used: %zu bytes\n\n", freelist_used(&permanent));

    // === Game loop (simulated) ===
    printf("=== GAME LOOP (3 frames) ===\n");

    for (int frame_num = 0; frame_num < 3; frame_num++) {
        printf("\n--- Frame %d ---\n", frame_num);

        // Spawn enemies (persistent until killed)
        if (frame_num == 0) {
            for (int i = 0; i < 3; i++) {
                Enemy *e = pool_alloc(&entities);
                if (e) {
                    e->id = i;
                    e->health = 100;
                    printf("  Spawned enemy %d\n", e->id);
                }
            }
        }

        // Per-frame calculations (temporary)
        float *distances = bump_alloc(&frame, sizeof(float) * 10);
        char *log_buffer = bump_alloc(&frame, 256);

        if (distances && log_buffer) {
            // Calculate distances (dummy)
            for (int i = 0; i < 10; i++) {
                distances[i] = (float)(i * 10 + frame_num);
            }

            snprintf(log_buffer, 256, "Frame %d: calculated %d distances",
                     frame_num, 10);
            printf("  %s\n", log_buffer);
        }

        printf("  Frame memory used: %zu bytes\n", bump_used(&frame));
        printf("  Entities active: %zu\n", pool_used(&entities));

        // Reset frame allocator
        bump_reset(&frame);
    }

    printf("\n=== CLEANUP ===\n");
    printf("Permanent memory still holding items: %zu bytes used\n",
           freelist_used(&permanent));

    // Free items
    freelist_free(&permanent, sword);
    freelist_free(&permanent, potion);
    printf("Items freed, permanent memory used: %zu bytes\n", freelist_used(&permanent));
}

/* ============================================================================
 * Main
 * ============================================================================
 */

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           MEMORY ALLOCATOR EXAMPLES                         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    example_bump_allocator();
    example_pool_allocator();
    example_stack_allocator();
    example_freelist_allocator();
    example_combined_usage();

    printf("\n\n=== ALL EXAMPLES COMPLETE ===\n");
    return 0;
}
