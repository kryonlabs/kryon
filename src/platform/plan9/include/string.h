/* Plan 9 native shim: string operations come from libc.h. */
#ifndef KRYON_PLAN9_SHIM_STRING_H
#define KRYON_PLAN9_SHIM_STRING_H

#include "kryon_plan9_libc.h"

#define strcasecmp cistrcmp
#define strncasecmp cistrncmp

#endif
