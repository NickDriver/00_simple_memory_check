/*
 * Test suite for simple_memory_allocator
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/simple_memory_allocator.h"

static int tests_run = 0;
static int tests_passed = 0;

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  " COLOR_YELLOW "[TEST]" COLOR_RESET " %-40s ", #name); \
    if (test_##name()) { \
        tests_passed++; \
        printf(COLOR_GREEN "[PASS]" COLOR_RESET "\n"); \
    } else { \
        printf(COLOR_RED "[FAIL]" COLOR_RESET "\n"); \
    } \
} while(0)

#define ASSERT(cond) do { if (!(cond)) return 0; } while(0)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(p) ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

// Test: init sets all fields to zero
TEST(init_zeros_struct) {
    SimpleMemoryAllocator alloc;
    alloc.memory = (void*)0xDEADBEEF;
    alloc.size = 999;
    alloc.used = 999;

    simple_memory_allocator_init(&alloc);

    ASSERT_NULL(alloc.memory);
    ASSERT_EQ(alloc.size, 0);
    ASSERT_EQ(alloc.used, 0);
    return 1;
}

// Test: create allocates memory pool
TEST(create_allocates_pool) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);

    int result = simple_memory_allocator_create(&alloc, 1024);

    ASSERT_EQ(result, 0);
    ASSERT_NOT_NULL(alloc.memory);
    ASSERT_EQ(alloc.size, 1024);
    ASSERT_EQ(alloc.used, 0);

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: create fails with NULL allocator
TEST(create_fails_null_allocator) {
    int result = simple_memory_allocator_create(NULL, 1024);
    ASSERT_EQ(result, -1);
    return 1;
}

// Test: create fails with zero size
TEST(create_fails_zero_size) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);

    int result = simple_memory_allocator_create(&alloc, 0);

    ASSERT_EQ(result, -1);
    return 1;
}

// Test: alloc returns valid pointer
TEST(alloc_returns_valid_pointer) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    void *ptr = simple_memory_allocator_alloc(&alloc, 100);

    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(ptr, alloc.memory);

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: alloc updates used counter with alignment
TEST(alloc_updates_used_with_alignment) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    simple_memory_allocator_alloc(&alloc, 100);

    // 100 aligned to 8 = 104
    ASSERT_EQ(alloc.used, 104);

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: multiple allocs return sequential pointers
TEST(multiple_allocs_sequential) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    void *ptr1 = simple_memory_allocator_alloc(&alloc, 32);
    void *ptr2 = simple_memory_allocator_alloc(&alloc, 64);
    void *ptr3 = simple_memory_allocator_alloc(&alloc, 16);

    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_NOT_NULL(ptr3);

    // Pointers should be sequential (with 8-byte alignment)
    ASSERT_EQ((uint8_t*)ptr2, (uint8_t*)ptr1 + 32);
    ASSERT_EQ((uint8_t*)ptr3, (uint8_t*)ptr2 + 64);

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: alloc fails when pool exhausted
TEST(alloc_fails_when_exhausted) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 100);

    void *ptr1 = simple_memory_allocator_alloc(&alloc, 50);
    void *ptr2 = simple_memory_allocator_alloc(&alloc, 50);

    ASSERT_NOT_NULL(ptr1);
    ASSERT_NULL(ptr2);  // Should fail - not enough space (50 aligned = 56)

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: alloc fails with NULL allocator
TEST(alloc_fails_null_allocator) {
    void *ptr = simple_memory_allocator_alloc(NULL, 100);
    ASSERT_NULL(ptr);
    return 1;
}

// Test: alloc fails with zero size
TEST(alloc_fails_zero_size) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    void *ptr = simple_memory_allocator_alloc(&alloc, 0);

    ASSERT_NULL(ptr);

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: alloc fails on uninitialized allocator
TEST(alloc_fails_uninitialized) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);

    void *ptr = simple_memory_allocator_alloc(&alloc, 100);

    ASSERT_NULL(ptr);
    return 1;
}

// Test: reset clears used counter
TEST(reset_clears_used) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    simple_memory_allocator_alloc(&alloc, 256);
    simple_memory_allocator_alloc(&alloc, 128);
    ASSERT_NE(alloc.used, 0);

    simple_memory_allocator_reset(&alloc);

    ASSERT_EQ(alloc.used, 0);
    ASSERT_NOT_NULL(alloc.memory);  // Memory pool should still exist
    ASSERT_EQ(alloc.size, 1024);    // Size unchanged

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: reset allows reallocation
TEST(reset_allows_realloc) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 256);

    void *ptr1 = simple_memory_allocator_alloc(&alloc, 200);
    ASSERT_NOT_NULL(ptr1);

    void *ptr2 = simple_memory_allocator_alloc(&alloc, 100);
    ASSERT_NULL(ptr2);  // Should fail

    simple_memory_allocator_reset(&alloc);

    void *ptr3 = simple_memory_allocator_alloc(&alloc, 200);
    ASSERT_NOT_NULL(ptr3);
    ASSERT_EQ(ptr3, ptr1);  // Same address as first allocation

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: reset handles NULL allocator
TEST(reset_handles_null) {
    simple_memory_allocator_reset(NULL);  // Should not crash
    return 1;
}

// Test: destroy frees memory and zeros struct
TEST(destroy_frees_and_zeros) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    ASSERT_NOT_NULL(alloc.memory);

    simple_memory_allocator_destroy(&alloc);

    ASSERT_NULL(alloc.memory);
    ASSERT_EQ(alloc.size, 0);
    ASSERT_EQ(alloc.used, 0);
    return 1;
}

// Test: destroy handles NULL allocator
TEST(destroy_handles_null) {
    simple_memory_allocator_destroy(NULL);  // Should not crash
    return 1;
}

// Test: allocated memory is writable
TEST(allocated_memory_writable) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    char *buf = simple_memory_allocator_alloc(&alloc, 100);
    ASSERT_NOT_NULL(buf);

    // Write pattern
    memset(buf, 0xAB, 100);

    // Verify pattern
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ((unsigned char)buf[i], 0xAB);
    }

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: alignment is correct (8-byte boundary)
TEST(alignment_8_byte) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024);

    void *ptr1 = simple_memory_allocator_alloc(&alloc, 1);   // 1 -> 8
    void *ptr2 = simple_memory_allocator_alloc(&alloc, 5);   // 5 -> 8
    void *ptr3 = simple_memory_allocator_alloc(&alloc, 13);  // 13 -> 16

    ASSERT_EQ((uintptr_t)ptr1 % 8, 0);
    ASSERT_EQ((uintptr_t)ptr2 % 8, 0);
    ASSERT_EQ((uintptr_t)ptr3 % 8, 0);

    ASSERT_EQ((uint8_t*)ptr2 - (uint8_t*)ptr1, 8);
    ASSERT_EQ((uint8_t*)ptr3 - (uint8_t*)ptr2, 8);

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: exact pool size allocation
TEST(exact_pool_size_alloc) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 64);

    void *ptr = simple_memory_allocator_alloc(&alloc, 64);

    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(alloc.used, 64);

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

// Test: alloc just over pool size fails
TEST(alloc_over_pool_size_fails) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 64);

    void *ptr = simple_memory_allocator_alloc(&alloc, 65);

    ASSERT_NULL(ptr);  // 65 aligned to 72 > 64

    simple_memory_allocator_destroy(&alloc);
    return 1;
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║     Simple Memory Allocator Test Suite             ║\n");
    printf("╚════════════════════════════════════════════════════╝\n\n");

    printf("▸ Initialization Tests\n");
    RUN_TEST(init_zeros_struct);

    printf("\n▸ Creation Tests\n");
    RUN_TEST(create_allocates_pool);
    RUN_TEST(create_fails_null_allocator);
    RUN_TEST(create_fails_zero_size);

    printf("\n▸ Allocation Tests\n");
    RUN_TEST(alloc_returns_valid_pointer);
    RUN_TEST(alloc_updates_used_with_alignment);
    RUN_TEST(multiple_allocs_sequential);
    RUN_TEST(alloc_fails_when_exhausted);
    RUN_TEST(alloc_fails_null_allocator);
    RUN_TEST(alloc_fails_zero_size);
    RUN_TEST(alloc_fails_uninitialized);

    printf("\n▸ Reset Tests\n");
    RUN_TEST(reset_clears_used);
    RUN_TEST(reset_allows_realloc);
    RUN_TEST(reset_handles_null);

    printf("\n▸ Destroy Tests\n");
    RUN_TEST(destroy_frees_and_zeros);
    RUN_TEST(destroy_handles_null);

    printf("\n▸ Memory Tests\n");
    RUN_TEST(allocated_memory_writable);
    RUN_TEST(alignment_8_byte);
    RUN_TEST(exact_pool_size_alloc);
    RUN_TEST(alloc_over_pool_size_fails);

    printf("\n────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf(" - ALL TESTS PASSED!\n");
    } else {
        printf(" - %d FAILED\n", tests_run - tests_passed);
    }
    printf("────────────────────────────────────────────────────\n\n");

    return tests_passed == tests_run ? 0 : 1;
}
