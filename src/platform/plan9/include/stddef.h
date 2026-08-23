/* Plan 9 native shim: common definitions. size_t/NULL via libc types.
 * u.h is include-once by convention; only pull it when not already in. */
#ifndef KRYON_PLAN9_SHIM_STDDEF_H
#define KRYON_PLAN9_SHIM_STDDEF_H

#ifndef nil
#include <u.h>
#endif

typedef ulong size_t;
typedef long ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

/* libc.h defines offsetof identically; keep whichever arrives first. */
#ifndef offsetof
#define offsetof(s, m) ((ulong) & (((s *)0)->m))
#endif

#endif
