#ifndef KIR_EMIT_H
#define KIR_EMIT_H
#include "kir.h"

typedef enum KirTarget { KIR_C, KIR_CPP, KIR_GO, KIR_JS } KirTarget;
/* Resolve target symbol spelling and call ABI. Input contains identifiers and
 * captured argument names only, never a source expression to reinterpret. */
typedef void (*KirResolveTarget)(void *context, const char *text, char *out, size_t size);
const char *KirTargetType(const char *type, KirTarget target);
int KirScalarLiteral(const char *type, const char *text, KirTarget target,
                      KirSourceSpan span, char *out, size_t size);
int KirCanEmitBody(const KirFunction *fn);
void KirEmitNumbers(FILE *out, const KirModule *module, KirTarget target);
int KirEmitBody(FILE *out, const KirModule *module, const KirFunction *fn,
                KirTarget target, KirResolveTarget resolve, void *context);
#endif
