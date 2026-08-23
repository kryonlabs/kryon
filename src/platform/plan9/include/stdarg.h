/* Plan 9 native shim: ANSI varargs come from the per-architecture u.h.
 * u.h is include-once by convention; only pull it when not already in.
 * Hosted syntax-check builds (clang/gcc) fall through to the compiler's
 * builtin stdarg for the hosted headers pulled in alongside these shims. */
#ifndef KRYON_PLAN9_SHIM_STDARG_H
#define KRYON_PLAN9_SHIM_STDARG_H

#ifndef nil
#include <u.h>
#endif
#if defined(__clang__) || defined(__GNUC__)
#include_next <stdarg.h>
#endif

#endif
