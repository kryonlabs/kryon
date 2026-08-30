#ifndef KRY_ALLOC_H
#define KRY_ALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

static inline int
kry_size_add(size_t a, size_t b, size_t *out)
{
    if(a > SIZE_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static inline int
kry_size_mul(size_t a, size_t b, size_t *out)
{
    if(a != 0 && b > SIZE_MAX / a)
        return 0;
    *out = a * b;
    return 1;
}

static inline int
kry_size_grow_max(size_t current, size_t needed, size_t initial, size_t max,
                  size_t *out)
{
    size_t cap = current;

    if(needed > max || current > max)
        return 0;
    if(cap >= needed) {
        *out = cap;
        return 1;
    }
    if(cap == 0)
        cap = initial != 0 && initial <= max ? initial : 1;
    while(cap < needed) {
        if(cap > max / 2) {
            cap = needed;
            break;
        }
        cap *= 2;
    }
    *out = cap;
    return 1;
}

static inline void *
kry_realloc_array(void *ptr, size_t count, size_t elem_size)
{
    size_t bytes;

    if(!kry_size_mul(count, elem_size, &bytes))
        return NULL;
    return realloc(ptr, bytes);
}

static inline int
kry_reserve_bytes_max(void **ptr, size_t *cap, size_t needed, size_t initial,
                      size_t max)
{
    size_t next_cap;
    void *next;

    if(needed <= *cap)
        return 1;
    if(!kry_size_grow_max(*cap, needed, initial, max, &next_cap))
        return 0;
    next = realloc(*ptr, next_cap);
    if(next == NULL)
        return 0;
    *ptr = next;
    *cap = next_cap;
    return 1;
}

#endif /* KRY_ALLOC_H */
