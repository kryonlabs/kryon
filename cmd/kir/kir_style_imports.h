/*
 * Shared helpers for resolving .kry #style imports to KSS source.
 */
#ifndef KRYON_KIR_STYLE_IMPORTS_H
#define KRYON_KIR_STYLE_IMPORTS_H

#include "kir.h"

int KirStyleImportIsBuiltIn(const char *target);
char *KirReadStyleImportSource(const KirModule *module, const char *root,
                               const KirStyleImport *style);

#endif
