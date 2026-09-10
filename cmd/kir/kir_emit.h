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
int KirCanEmitBody(const KirModule *module, const KirFunction *fn);
/* Stream a portable record's zero value or deep copy; no output if unsupported. */
int KirEmitJsRecordValue(FILE *out, const KirModule *module, const char *type,
                         const char *source);
void KirEmitNumbers(FILE *out, const KirModule *module, KirTarget target);
void KirEmitNumberSupport(FILE *out, KirTarget target, const char *prefix);
void KirEmitStringType(FILE *out);
void KirEmitSlotWrappers(FILE *out, const KirModule *module, const KirFunction *fn,
                         KirTarget target, KirResolveTarget resolver, void *context);
void KirEmitSlotType(FILE *out, const KirType *slot, KirTarget target,
                     KirResolveTarget resolve_type, void *context);
/* instance_host is the native Go implementation receiver, or an empty string
 * for application functions and the other targets. number_support overrides
 * the module-local numeric helper prefix when a package shares one definition. */
int KirEmitBody(FILE *out, const KirModule *module, const KirFunction *fn,
                KirTarget target, KirResolveTarget resolve, void *context,
                const char *instance_host, const char *number_support);
#endif
