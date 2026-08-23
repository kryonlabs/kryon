/* Plan 9 native shim: libc.h already carries an assert that calls the
 * libc _assert handler; keep whichever definition arrives first. */
#ifndef KRYON_PLAN9_SHIM_ASSERT_H
#define KRYON_PLAN9_SHIM_ASSERT_H

#ifndef assert
#define assert(e) ((e) ? (void)0 : (fprint(2, "assert failed: %s:%d: %s\n", \
                                           __FILE__, __LINE__, #e), abort()))
#endif

#endif
