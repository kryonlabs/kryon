/* Plan 9 native shim: math surface.
 *
 * Native libc declares double math in <libc.h>; the C99 float spellings map
 * onto those. Round-trip through double is exact for the float values Kryon
 * computes, so results match the hosted builds.
 */
#ifndef KRYON_PLAN9_SHIM_MATH_H
#define KRYON_PLAN9_SHIM_MATH_H

#include "kryon_plan9_libc.h"

#define sinf sin
#define cosf cos
#define tanf tan
#define asinf asin
#define acosf acos
#define atanf atan
#define atan2f atan2
#define sinhf sinh
#define coshf cosh
#define tanhf tanh
#define expf exp
#define logf log
#define log10f log10
#define powf pow
#define sqrtf sqrt
#define fabsf fabs
#define floorf floor
#define ceilf ceil
#define fmodf fmod
#define hypotf hypot

/* Native libc has no fmax/fmin, so the float spellings are provided
 * inline with C99 NaN semantics (raymath clamps rely on them). */
static float
kryon_plan9_fmaxf(float a, float b)
{
    if(a != a)
        return b;
    if(b != b)
        return a;
    return a > b ? a : b;
}

static float
kryon_plan9_fminf(float a, float b)
{
    if(a != a)
        return b;
    if(b != b)
        return a;
    return a < b ? a : b;
}

#define fmaxf kryon_plan9_fmaxf
#define fminf kryon_plan9_fminf

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define NAN_PLAN9 (0.0f / 0.0f)
#define INFINITY_PLAN9 (1e308f * 10.0f)
#ifndef NAN
#define NAN NAN_PLAN9
#endif
#ifndef INFINITY
#define INFINITY INFINITY_PLAN9
#endif

#endif
