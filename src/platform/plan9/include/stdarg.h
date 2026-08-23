/* Plan 9 native shim: ANSI varargs come from the per-architecture u.h,
 * which defines va_list/va_start/va_arg/va_end. Pulling the full libc shim
 * here also brings libc.h in before Kryon's compatibility header, so libc
 * owns the definitions (PI, offsetof, ...) and Kryon's #ifndef guards skip
 * theirs. Hosted compilers never see this header - the native mkfile puts
 * it first on the include path. */
#ifndef KRYON_PLAN9_SHIM_STDARG_H
#define KRYON_PLAN9_SHIM_STDARG_H

#include "kryon_plan9_libc.h"

#ifndef va_copy
#define va_copy(dst, src) ((dst) = (src))
#endif

#ifndef __va_copy
#define __va_copy(dst, src) va_copy(dst, src)
#endif

#endif
