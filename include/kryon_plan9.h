#ifndef KRYON_PLAN9_H
#define KRYON_PLAN9_H

#ifndef KRYON_NATIVE_PLAN9
#define KRYON_NATIVE_PLAN9 1
#endif

#include "kryon_plan9_libc.h"

/* The shim <math.h> maps the C99 float spellings (fmodf, atan2f, ...)
   onto native libc's double math; sources skip their hosted include
   block under KRYON_NATIVE_PLAN9, so pull the surface in here. */
#include <math.h>

#ifndef NULL
#define NULL nil
#endif

#ifndef snprintf
#define snprintf snprint
#endif
#ifndef vsnprintf
#define vsnprintf vsnprint
#endif
#ifndef sqrtf
#define sqrtf(x) ((float)sqrt((double)(x)))
#endif
#ifndef fabsf
#define fabsf(x) ((float)fabs((double)(x)))
#endif
#ifndef floorf
#define floorf(x) ((float)floor((double)(x)))
#endif
#ifndef ceilf
#define ceilf(x) ((float)ceil((double)(x)))
#endif
#ifndef sinf
#define sinf(x) ((float)sin((double)(x)))
#endif
#ifndef cosf
#define cosf(x) ((float)cos((double)(x)))
#endif

#include "kryon_compat.generated.h"

#endif /* KRYON_PLAN9_H */
