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

#define UINT8_MAX  255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295U
#define INT8_MIN   (-128)
#define INT16_MIN  (-32768)
#define INT32_MIN  (-2147483647 - 1)
#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647
#define INT64_MAX  ((int64_t)0x7fffffffffffffff)
#define INT64_MIN  (-INT64_MAX - 1)
#define UINT64_MAX ((uint64_t)0xffffffffffffffff)
#define UINT64_C(c) ((uint64_t)(c))
#define INT64_C(c) ((int64_t)(c))
#define UINT32_C(c) ((uint32_t)(c))

#endif
