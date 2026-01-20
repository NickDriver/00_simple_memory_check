/*
 * Benchmark suite for simple_memory_allocator
 *
 * Measures allocation throughput and compares against malloc.
 */

#define _POSIX_C_SOURCE 199309L  // Required for clock_gettime

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "../src/simple_memory_allocator.h"

// Benchmark configuration
#define ITERATIONS      1000000
#define WARMUP_ITERS    10000
#define POOL_SIZE       (64 * 1024 * 1024)  // 64MB pool

// High-resolution timer
typedef struct {
    struct timespec start;
    struct timespec end;
} BenchTimer;

static inline void bench_start(BenchTimer *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

static inline void bench_end(BenchTimer *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->end);
}

static inline double bench_elapsed_ns(const BenchTimer *t) {
    double start_ns = t->start.tv_sec * 1e9 + t->start.tv_nsec;
    double end_ns = t->end.tv_sec * 1e9 + t->end.tv_nsec;
    return end_ns - start_ns;
}

static inline double bench_ops_per_sec(const BenchTimer *t, size_t ops) {
    double elapsed_s = bench_elapsed_ns(t) / 1e9;
    return (double)ops / elapsed_s;
}

// Prevent compiler from optimizing away allocations
static void *volatile sink;

// Benchmark bump allocator allocation throughput
static double bench_bump_alloc(size_t alloc_size, size_t iterations) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, POOL_SIZE);

    // Calculate how many allocs fit in pool
    size_t aligned_size = (alloc_size + 7) & ~((size_t)7);
    size_t allocs_per_pool = POOL_SIZE / aligned_size;

    BenchTimer timer;
    size_t total_allocs = 0;

    bench_start(&timer);

    while (total_allocs < iterations) {
        for (size_t i = 0; i < allocs_per_pool && total_allocs < iterations; i++) {
            sink = simple_memory_allocator_alloc(&alloc, alloc_size);
            total_allocs++;
        }
        simple_memory_allocator_reset(&alloc);
    }

    bench_end(&timer);

    simple_memory_allocator_destroy(&alloc);
    return bench_ops_per_sec(&timer, total_allocs);
}

// Benchmark malloc allocation throughput
static double bench_malloc_alloc(size_t alloc_size, size_t iterations) {
    // Pre-allocate array to store pointers for freeing
    void **ptrs = malloc(iterations * sizeof(void *));
    if (!ptrs) return 0;

    BenchTimer timer;

    bench_start(&timer);

    for (size_t i = 0; i < iterations; i++) {
        ptrs[i] = malloc(alloc_size);
        sink = ptrs[i];
    }

    bench_end(&timer);

    double ops = bench_ops_per_sec(&timer, iterations);

    // Free all allocations
    for (size_t i = 0; i < iterations; i++) {
        free(ptrs[i]);
    }
    free(ptrs);

    return ops;
}

// Benchmark reset performance
static void bench_reset_vs_recreate(void) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, POOL_SIZE);

    size_t iterations = 100000;
    BenchTimer timer;

    // Benchmark reset()
    bench_start(&timer);
    for (size_t i = 0; i < iterations; i++) {
        simple_memory_allocator_alloc(&alloc, 1024);
        simple_memory_allocator_reset(&alloc);
    }
    bench_end(&timer);
    double reset_ns = bench_elapsed_ns(&timer) / iterations;

    simple_memory_allocator_destroy(&alloc);

    // Benchmark destroy() + create()
    bench_start(&timer);
    for (size_t i = 0; i < iterations; i++) {
        simple_memory_allocator_create(&alloc, POOL_SIZE);
        simple_memory_allocator_alloc(&alloc, 1024);
        simple_memory_allocator_destroy(&alloc);
    }
    bench_end(&timer);
    double recreate_ns = bench_elapsed_ns(&timer) / iterations;

    printf("\n  Reset Performance (%zu iterations)\n", iterations);
    printf("  %-30s %12.1f ns\n", "reset()", reset_ns);
    printf("  %-30s %12.1f ns\n", "destroy() + create()", recreate_ns);
    printf("  %-30s %12.1fx faster\n", "Speedup", recreate_ns / reset_ns);
}

// Benchmark fill pattern (how long to fill entire pool)
static void bench_fill_pattern(size_t alloc_size) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, POOL_SIZE);

    size_t aligned_size = (alloc_size + 7) & ~((size_t)7);
    size_t allocs_per_pool = POOL_SIZE / aligned_size;
    size_t fill_iterations = 100;

    BenchTimer timer;

    bench_start(&timer);
    for (size_t iter = 0; iter < fill_iterations; iter++) {
        for (size_t i = 0; i < allocs_per_pool; i++) {
            sink = simple_memory_allocator_alloc(&alloc, alloc_size);
        }
        simple_memory_allocator_reset(&alloc);
    }
    bench_end(&timer);

    double total_bytes = (double)POOL_SIZE * fill_iterations;
    double elapsed_s = bench_elapsed_ns(&timer) / 1e9;
    double throughput_gbps = (total_bytes / (1024.0 * 1024.0 * 1024.0)) / elapsed_s;

    printf("\n  Fill Pattern (alloc size: %zu bytes)\n", alloc_size);
    printf("  %-30s %zu\n", "Allocations per fill", allocs_per_pool);
    printf("  %-30s %.2f GB/s\n", "Throughput", throughput_gbps);

    simple_memory_allocator_destroy(&alloc);
}

// Format large numbers with commas
static void format_number(double n, char *buf, size_t buf_size) {
    if (n >= 1e9) {
        snprintf(buf, buf_size, "%.2fB", n / 1e9);
    } else if (n >= 1e6) {
        snprintf(buf, buf_size, "%.2fM", n / 1e6);
    } else if (n >= 1e3) {
        snprintf(buf, buf_size, "%.2fK", n / 1e3);
    } else {
        snprintf(buf, buf_size, "%.0f", n);
    }
}

// Warmup to stabilize CPU frequency and fill caches
static void warmup(void) {
    SimpleMemoryAllocator alloc;
    simple_memory_allocator_init(&alloc);
    simple_memory_allocator_create(&alloc, 1024 * 1024);

    for (size_t i = 0; i < WARMUP_ITERS; i++) {
        sink = simple_memory_allocator_alloc(&alloc, 64);
        if ((i % 1000) == 0) {
            simple_memory_allocator_reset(&alloc);
        }
    }

    simple_memory_allocator_destroy(&alloc);

    // Warmup malloc too
    for (size_t i = 0; i < WARMUP_ITERS; i++) {
        void *p = malloc(64);
        sink = p;
        free(p);
    }
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          Simple Memory Allocator Benchmark                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    printf("\nWarming up...\n");
    warmup();

    // Allocation sizes to benchmark
    size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 4096};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("\n▸ Allocation Throughput (%d iterations)\n", ITERATIONS);
    printf("  %-8s %14s %14s %10s\n", "Size", "Bump/s", "malloc/s", "Speedup");
    printf("  ─────────────────────────────────────────────────────\n");

    for (size_t i = 0; i < num_sizes; i++) {
        size_t size = sizes[i];

        double bump_ops = bench_bump_alloc(size, ITERATIONS);
        double malloc_ops = bench_malloc_alloc(size, ITERATIONS);
        double speedup = bump_ops / malloc_ops;

        char bump_str[32], malloc_str[32];
        format_number(bump_ops, bump_str, sizeof(bump_str));
        format_number(malloc_ops, malloc_str, sizeof(malloc_str));

        printf("  %-8zu %14s %14s %9.1fx\n", size, bump_str, malloc_str, speedup);
    }

    printf("\n▸ Reset vs Recreate\n");
    bench_reset_vs_recreate();

    printf("\n▸ Memory Throughput\n");
    bench_fill_pattern(64);
    bench_fill_pattern(1024);

    printf("\n────────────────────────────────────────────────────────────\n");
    printf("Benchmark complete.\n");
    printf("────────────────────────────────────────────────────────────\n\n");

    return 0;
}
