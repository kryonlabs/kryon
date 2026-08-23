/* Plan 9 native shim: common definitions. u.h/libc.h (via the shared libc
 * shim) provide size_t-compatible types, NULL, and offsetof. */
#ifndef KRYON_PLAN9_SHIM_STDDEF_H
#define KRYON_PLAN9_SHIM_STDDEF_H

#include "kryon_plan9_libc.h"

typedef long ptrdiff_t;

#endif
