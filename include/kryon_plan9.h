#ifndef KRYON_PLAN9_H
#define KRYON_PLAN9_H

#ifndef KRYON_NATIVE_PLAN9
#define KRYON_NATIVE_PLAN9 1
#endif

#include "kryon_plan9_libc.h"

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
