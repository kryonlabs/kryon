#ifndef KIR_LAWS_H
#define KIR_LAWS_H
#include "kir.h"

/* Enforce named .kry laws over parsed programs. Violations report
 * `law <name>` diagnostics and fail strict builds; lenient builds stay
 * silent, mirroring KirCheckPrograms. Returns 1 when passing. */
int KirCheckLaws(KirProgram **programs, int count, int strict);
#endif
