#ifndef KIR_LAWS_H
#define KIR_LAWS_H
#include "kir.h"

/* Enforce named .kry laws over parsed programs in every compiler mode.
 * Violations report `law <name>` diagnostics and fail before lowering.
 * Type-checking strictness cannot disable laws. Returns 1 when passing. */
int KirCheckLaws(KirProgram **programs, int count);
#endif
