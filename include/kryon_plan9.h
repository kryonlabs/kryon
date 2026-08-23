#ifndef KRYON_PLAN9_H
#define KRYON_PLAN9_H

#ifndef KRYON_NATIVE_PLAN9
#define KRYON_NATIVE_PLAN9 1
#endif

/* Native Plan 9 core headers are include-once by convention (u.h carries no
 * include guard), so re-entry checks the nil/nelem macros they define. */
#ifndef nil
#include <u.h>
#endif
#ifndef nelem
#include <libc.h>
#endif

#ifndef KRYON_PLAN9_SIZE_T_DEFINED
#define KRYON_PLAN9_SIZE_T_DEFINED
typedef usize size_t;
#endif

#ifndef NULL
#define NULL nil
#endif

#define snprintf snprint
#define vsnprintf vsnprint
#define sqrtf(x) ((float)sqrt((double)(x)))
#define fabsf(x) ((float)fabs((double)(x)))
#define floorf(x) ((float)floor((double)(x)))
#define ceilf(x) ((float)ceil((double)(x)))
#define sinf(x) ((float)sin((double)(x)))
#define cosf(x) ((float)cos((double)(x)))

#include "kryon_compat.generated.h"

#endif /* KRYON_PLAN9_H */
