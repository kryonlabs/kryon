#ifndef KIR_DIAGNOSTIC_H
#define KIR_DIAGNOSTIC_H

#include "kir.h"
#include <stdarg.h>

int KirSetDiagnosticFormat(const char *format);
void KirDiagnostic(KirSourceSpan span, const char *code, const char *format, ...);
void KirDiagnosticV(KirSourceSpan span, const char *code, const char *format, va_list args);

#endif
