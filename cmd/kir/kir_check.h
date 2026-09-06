#ifndef KIR_CHECK_H
#define KIR_CHECK_H
#include "kir.h"

/* Resolve scalar expression types and lexical bindings. Strict mode rejects
 * unresolved expressions instead of delegating them to target-language text. */
int KirCheckPrograms(KirProgram **programs, int count, int strict);
const char *KirScalarType(const char *type);
#endif
