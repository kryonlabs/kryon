#ifndef KIR_CLEANUP_H
#define KIR_CLEANUP_H
#include "kir.h"

/* Lower lexical cleanup once, before backend emission. Returns zero on error. */
int KirLowerCleanup(KirFunction *fn);
#endif
