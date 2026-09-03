#ifndef KRYON_KIR_PARSE_H
#define KRYON_KIR_PARSE_H

#include "kir.h"

/*
 * Parse a .kry source file into a KirProgram. Returns a heap-allocated
 * program (caller frees with KirProgramFree), or NULL on fatal error.
 * This is the shared Kir frontend used by k2kir (dump), k2c (C backend),
 * and k2b (krb backend).
 */
KirProgram *kir_parse_file(const char *path, const char *root);

#endif /* KRYON_KIR_PARSE_H */
