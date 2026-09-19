#ifndef KRY_SLICE_H
#define KRY_SLICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* Compiler-owned descriptor. Element types and storage lifetimes are checked
 * before emission; generated accesses cast data to the declared element type. */
typedef struct Slice {
    void *data;
    int32_t length;
} Slice;

static inline int32_t
SliceIndex(Slice source, int64_t index)
{
    if(index < 0 || index >= source.length) {
        fprintf(stderr, "kryon: slice index out of bounds\n");
        abort();
    }
    return (int32_t)index;
}

static inline Slice
SliceRange(Slice source, int64_t low, int64_t high, size_t element_size)
{
    if(low < 0 || high < low || high > source.length) {
        fprintf(stderr, "kryon: slice range out of bounds\n");
        abort();
    }
    Slice result;
    result.data = source.data != NULL ? (char *)source.data + (size_t)low * element_size : NULL;
    result.length = (int32_t)(high - low);
    return result;
}

#endif
