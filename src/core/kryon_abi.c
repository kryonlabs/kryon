/* ABI guard implementation — see the KRYON_ABI_VERSION comment in
 * kryon.h. Compiled into libkryon.a so consumers can compare the value
 * baked into the archive against the one in their freshly compiled
 * headers; a mismatch means the archive predates the headers. */

#include "kryon.h"

int
KryonAbiVersion(void)
{
    return KRYON_ABI_VERSION;
}
