/* Plan 9 native build shims for the Kryon libdraw backend.
 *
 * These headers stand in for the hosted-OS headers Kryon's portable sources
 * include (<string.h>, <stdlib.h>, <math.h>, ...). On native Plan 9 the
 * declarations live in <libc.h>/<u.h> and the pANS <stdio.h>; each shim maps
 * onto those. They are only placed on the include path by the native Plan 9
 * mkfile, so hosted builds never see them.
 *
 * Float math: native libc exposes double math only, so the C99 *f spellings
 * are mapped onto the double functions. Call sites pass float arguments and
 * store into float fields; the implicit conversions keep values identical.
 */
#ifndef KRYON_PLAN9_SHIM_LIBC_H
#define KRYON_PLAN9_SHIM_LIBC_H

/* Native Plan 9 core headers are include-once by convention (u.h carries no
 * include guard), so re-entry checks the nil/nelem macros they define. */
#ifndef nil
#include <u.h>
#endif
#ifndef nelem
#include <libc.h>
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Hosted <stdlib.h> provides size_t; native libc does not. */
#ifndef KRYON_PLAN9_SIZE_T_DEFINED
#define KRYON_PLAN9_SIZE_T_DEFINED
typedef ulong size_t;
#endif

/* Hosted code commonly uses time_t and GCC-style attributes. Native Plan 9's
 * libc has time(long*) but no C library time_t typedef, and 8c has no
 * __attribute__ parser. */
#ifndef KRYON_PLAN9_TIME_T_DEFINED
#define KRYON_PLAN9_TIME_T_DEFINED
typedef long time_t;
#endif

#ifndef __attribute__
#define __attribute__(x)
#endif

/* Native libc has no rename(2); the runtime stubs implement it as a
 * same-directory name wstat. */
int rename(const char *oldpath, const char *newpath);

#endif
