# How to Implement Memory Alignment

This tutorial teaches you how to align sizes and addresses to powers of 2 using bit manipulation.

## The Formula

To round a value **up** to the nearest multiple of N (where N is a power of 2):

```c
aligned = (value + (N - 1)) & ~(N - 1);
```

For 8-byte alignment:
```c
aligned = (value + 7) & ~7;
```

---

## Understanding the Bit Mask

### Step 1: Why `N - 1`?

For any power of 2, subtracting 1 gives a number with all lower bits set:

| N (decimal) | N (binary) | N - 1 (binary) |
|-------------|------------|----------------|
| 2           | `00000010` | `00000001`     |
| 4           | `00000100` | `00000011`     |
| 8           | `00001000` | `00000111`     |
| 16          | `00010000` | `00001111`     |
| 64          | `01000000` | `00111111`     |

### Step 2: Why `~(N - 1)`?

The `~` operator inverts all bits, creating a mask that **clears** the lower bits:

| N   | N - 1      | ~(N - 1)   | Effect                    |
|-----|------------|------------|---------------------------|
| 8   | `00000111` | `11111000` | Clears bottom 3 bits      |
| 16  | `00001111` | `11110000` | Clears bottom 4 bits      |
| 64  | `00111111` | `11000000` | Clears bottom 6 bits      |

When you AND a value with this mask, the result is always a multiple of N.

### Step 3: Why Add `N - 1` First?

The mask rounds **down**. Adding `N - 1` first ensures we round **up**:

```
Without adding first (rounds DOWN):
  13 & ~7 = 13 & 0xF8 = 8    (wrong - we lost data)

With adding first (rounds UP):
  (13 + 7) & ~7 = 20 & 0xF8 = 16    (correct)
```

---

## Worked Examples

### Example 1: Align 13 to 8 bytes

```
Step 1: Add (N - 1)
        13 + 7 = 20

Step 2: Create mask
        ~7 = ~(00000111) = 11111000

Step 3: Apply mask
        20      = 00010100
        ~7      = 11111000
        -------------------- AND
        result  = 00010000 = 16
```

### Example 2: Align 16 to 8 bytes (already aligned)

```
Step 1: 16 + 7 = 23

Step 2: ~7 = 11111000

Step 3: 23      = 00010111
        ~7      = 11111000
        -------------------- AND
        result  = 00010000 = 16

Already aligned values stay unchanged.
```

### Example 3: Align 1 to 8 bytes

```
Step 1: 1 + 7 = 8

Step 2: ~7 = 11111000

Step 3: 8       = 00001000
        ~7      = 11111000
        -------------------- AND
        result  = 00001000 = 8
```

---

## Quick Reference Table (8-byte alignment)

| Input | + 7 | & ~7 | Result |
|-------|-----|------|--------|
| 1     | 8   | 8    | 8      |
| 5     | 12  | 8    | 8      |
| 8     | 15  | 8    | 8      |
| 9     | 16  | 16   | 16     |
| 13    | 20  | 16   | 16     |
| 16    | 23  | 16   | 16     |
| 17    | 24  | 24   | 24     |
| 100   | 107 | 104  | 104    |

---

## Code Implementations

### Basic Function

```c
size_t align_up(size_t value, size_t alignment) {
    return (value + (alignment - 1)) & ~(alignment - 1);
}
```

### With Validation

```c
#include <assert.h>

size_t align_up(size_t value, size_t alignment) {
    // Alignment must be a power of 2
    assert(alignment > 0 && (alignment & (alignment - 1)) == 0);

    return (value + (alignment - 1)) & ~(alignment - 1);
}
```

### Macro Version

```c
#define ALIGN_UP(value, alignment) \
    (((value) + ((alignment) - 1)) & ~((alignment) - 1))
```

### Aligning Pointers

```c
void *align_pointer(void *ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + (alignment - 1)) & ~(alignment - 1);
    return (void *)aligned;
}
```

---

## Checking if a Value is Aligned

To check if a value is already aligned to N:

```c
int is_aligned(size_t value, size_t alignment) {
    return (value & (alignment - 1)) == 0;
}
```

This works because aligned values have zeros in the lower bits:

```
8  = 00001000  →  8 & 7 = 00001000 & 00000111 = 0  (aligned)
13 = 00001101  → 13 & 7 = 00001101 & 00000111 = 5  (not aligned)
16 = 00010000  → 16 & 7 = 00010000 & 00000111 = 0  (aligned)
```

---

## Common Alignment Values

| Type / Use Case          | Typical Alignment |
|--------------------------|-------------------|
| `char`                   | 1 byte            |
| `short`                  | 2 bytes           |
| `int`, `float`           | 4 bytes           |
| `long`, `double`, pointer| 8 bytes           |
| SIMD (SSE)               | 16 bytes          |
| SIMD (AVX)               | 32 bytes          |
| Cache line               | 64 bytes          |
| Memory page              | 4096 bytes        |

---

## Exercises

1. **Calculate by hand**: Align 25 to 16 bytes
2. **Calculate by hand**: Align 64 to 64 bytes
3. **Write code**: Create a function that aligns down (rounds to previous multiple)
4. **Write code**: Calculate how many bytes are wasted when aligning 13 to 8

### Solutions

<details>
<summary>Click to reveal</summary>

1. `(25 + 15) & ~15 = 40 & 0xF0 = 32`

2. `(64 + 63) & ~63 = 127 & 0xC0 = 64`

3. Align down (remove the addition):
   ```c
   size_t align_down(size_t value, size_t alignment) {
       return value & ~(alignment - 1);
   }
   ```

4. Wasted bytes:
   ```c
   size_t wasted = align_up(13, 8) - 13;  // 16 - 13 = 3 bytes
   ```

</details>

---

## Key Takeaways

1. **The formula**: `(value + (N-1)) & ~(N-1)` rounds up to multiple of N
2. **N must be a power of 2** for bit manipulation to work
3. **`~(N-1)` creates a mask** that clears the lower bits
4. **Adding `N-1` first** converts round-down to round-up
5. **To check alignment**: `(value & (N-1)) == 0`
