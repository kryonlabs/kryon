/* Plan 9 native shim: fixed-width integer types from u.h (via libc). */
#ifndef KRYON_PLAN9_SHIM_STDINT_H
#define KRYON_PLAN9_SHIM_STDINT_H

#include "kryon_plan9_libc.h"

typedef schar int8_t;
typedef uchar uint8_t;
typedef short int16_t;
typedef ushort uint16_t;
typedef int int32_t;
typedef uint uint32_t;
typedef vlong int64_t;
typedef uvlong uint64_t;
typedef long intptr_t;
typedef ulong uintptr_t;
typedef vlong intmax_t;
typedef uvlong uintmax_t;

#endif
