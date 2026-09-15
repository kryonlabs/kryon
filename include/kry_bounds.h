#ifndef KRY_BOUNDS_H
#define KRY_BOUNDS_H

#include <stddef.h>

/* Debug bounds checking for .kry fixed-array and string indexing. Builds
 * that define KRYON_BOUNDS_CHECK trap on out-of-range indexes with the
 * offending expression; release builds compile to plain indexing. */
#ifdef KRYON_BOUNDS_CHECK
#include <stdio.h>
#include <stdlib.h>
static inline size_t KryonBoundsCheck(size_t length, size_t index,
                                       const char *what)
{
    if(index >= length) {
        fprintf(stderr, "kryon: index %zu out of bounds for %s (length %zu)\n",
                index, what, length);
        abort();
    }
    return index;
}
#define KRYON_INDEX(base, length, index) \
    ((base)[KryonBoundsCheck((size_t)(length), (size_t)(index), #base)])
#else
#define KRYON_INDEX(base, length, index) ((base)[(index)])
#endif

#endif /* KRY_BOUNDS_H */
