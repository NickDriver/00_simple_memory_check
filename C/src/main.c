#include <stdio.h>
#include "simple_memory_allocator.h"

int main(void) {
    SimpleMemoryAllocator allocator;

    // Initialize and create allocator with 1KB pool
    simple_memory_allocator_init(&allocator);

    printf("=== Simple Memory Allocator Demo ===\n\n");

    if (simple_memory_allocator_create(&allocator, 1024) != 0) {
        printf("Failed to create allocator!\n");
        return 1;
    }

    printf("Allocator created with 1024 bytes pool\n\n");
    simple_memory_allocator_print_status(&allocator);

    // Allocate some memory
    printf("\nAllocating 100 bytes...\n");
    void *ptr1 = simple_memory_allocator_alloc(&allocator, 100);
    if (ptr1) {
        printf("Allocated at: %p\n\n", ptr1);
    }
    simple_memory_allocator_print_status(&allocator);

    // Allocate more memory
    printf("\nAllocating 256 bytes...\n");
    void *ptr2 = simple_memory_allocator_alloc(&allocator, 256);
    if (ptr2) {
        printf("Allocated at: %p\n\n", ptr2);
    }
    simple_memory_allocator_print_status(&allocator);

    // Allocate even more
    printf("\nAllocating 400 bytes...\n");
    void *ptr3 = simple_memory_allocator_alloc(&allocator, 400);
    if (ptr3) {
        printf("Allocated at: %p\n\n", ptr3);
    }
    simple_memory_allocator_print_status(&allocator);

    // Reset allocator
    printf("\nResetting allocator...\n\n");
    simple_memory_allocator_reset(&allocator);
    simple_memory_allocator_print_status(&allocator);

    // Cleanup
    printf("\nDestroying allocator...\n");
    simple_memory_allocator_destroy(&allocator);

    printf("Done!\n");
    return 0;
}
