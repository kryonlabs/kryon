#ifndef KIR_EXPR_H
#define KIR_EXPR_H
#include "kir.h"

/* Parse with token precedence; unsupported syntax is an explicit unknown node. */
int KirParseExpr(KirFunction *fn, const KirModule *module, const char *text,
                 KirSourceSpan span);
void KirStructureFunction(KirFunction *fn, const KirModule *module);
#endif
