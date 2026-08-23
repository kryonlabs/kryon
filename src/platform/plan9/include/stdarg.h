/* Plan 9 native shim: ANSI varargs come from the per-architecture u.h,
 * which defines va_list/va_start/va_arg/va_end. u.h is include-once by
 * convention; only pull it when not already in. Hosted compilers never see
 * this header - the native mkfile puts it first on the include path. */
#ifndef KRYON_PLAN9_SHIM_STDARG_H
#define KRYON_PLAN9_SHIM_STDARG_H

#ifndef nil
#include <u.h>
#endif

#endif
