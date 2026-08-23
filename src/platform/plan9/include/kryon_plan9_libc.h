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

#endif
