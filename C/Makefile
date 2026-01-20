CC = clang
CFLAGS_COMMON = -Wall -Wextra -Wpedantic -std=c17 -Iinclude

# Debug build with sanitizers
CFLAGS_DEBUG = $(CFLAGS_COMMON) -g -O1 -fno-omit-frame-pointer \
               -fsanitize=address,undefined \
               -fno-sanitize-recover=all

# Release build for benchmarks
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O3 -flto -march=native -DNDEBUG

# Memory sanitizer (requires clean build, no uninstrumented libs)
CFLAGS_MSAN = $(CFLAGS_COMMON) -g -O1 -fno-omit-frame-pointer \
              -fsanitize=memory -fsanitize-memory-track-origins

# Uncomment the following line to build the simple memory allocator
SRC = # src/simple_memory_allocator.c
MAIN = src/main.c
TEST = tests/test_simple_memory_allocator.c
BENCH = bench/bench_simple_memory_allocator.c

.PHONY: all debug release msan test bench clean

all: debug release msan

bin:
	mkdir -p bin

debug:
	$(CC) $(CFLAGS_DEBUG) $(SRC) $(MAIN) -o bin/simple_memory_allocator_debug

release:
	$(CC) $(CFLAGS_RELEASE) $(SRC) $(MAIN) -o bin/simple_memory_allocator_release

msan:
	$(CC) $(CFLAGS_MSAN) $(SRC) $(MAIN) -o bin/simple_memory_allocator_msan

test: debug
	$(CC) $(CFLAGS_DEBUG) $(SRC) $(TEST) -o bin/test_simple_memory_allocator
	./bin/test_simple_memory_allocator

bench: release
	$(CC) $(CFLAGS_RELEASE) $(SRC) $(BENCH) -o bin/bench_simple_memory_allocator
	./bin/bench_simple_memory_allocator

clean:
	rm -rf bin/*