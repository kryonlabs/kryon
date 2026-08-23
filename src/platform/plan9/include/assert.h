/* Plan 9 native shim: assert via fprint + abort. */
#ifndef KRYON_PLAN9_SHIM_ASSERT_H
#define KRYON_PLAN9_SHIM_ASSERT_H

#include <libc.h>

#define assert(e) ((e) ? (void)0 : (fprint(2, "assert failed: %s:%d: %s\n", \
                                           __FILE__, __LINE__, #e), abort()))

#endif
