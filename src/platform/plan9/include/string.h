/* Plan 9 native shim: string operations come from libc.h. */
#ifndef KRYON_PLAN9_SHIM_STRING_H
#define KRYON_PLAN9_SHIM_STRING_H

#include "kryon_plan9_libc.h"

#define strcasecmp cistrcmp
#define strncasecmp cistrncmp

/* Native libc has no POSIX reentrant tokenizer; the portable sources use it
 * in single-threaded parse loops, where the plain tokenizer is equivalent. */
#define strtok_r(s, d, c) strtok((s), (d))

#endif
