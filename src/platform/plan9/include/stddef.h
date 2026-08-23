/* Plan 9 native shim: common definitions. size_t/NULL via libc types. */
#ifndef KRYON_PLAN9_SHIM_STDDEF_H
#define KRYON_PLAN9_SHIM_STDDEF_H

#include <u.h>

typedef ulong size_t;
typedef long ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(s, m) ((ulong) & (((s *)0)->m))

#endif
