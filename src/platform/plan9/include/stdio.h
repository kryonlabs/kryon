/* Plan 9 native shim: the pANS <stdio.h> declares varargs functions with
 * va_list, which only exists once <u.h> has been read. Native Plan 9
 * sources always include u.h first; Kryon sources reach <stdio.h> on its
 * own, so pull the core headers in first and then the native stdio by
 * absolute path (a plain <stdio.h> here would loop back to this shim). */
#ifndef KRYON_PLAN9_SHIM_STDIO_H
#define KRYON_PLAN9_SHIM_STDIO_H

#include "kryon_plan9_libc.h"
#include "/sys/include/stdio.h"

#endif
