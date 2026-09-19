#include "kir_diagnostic.h"
#include "kry_json.h"

#include <stdlib.h>
#include <string.h>

static int diagnostic_json = -1;

int
KirSetDiagnosticFormat(const char *format)
{
    if(strcmp(format, "json") == 0)
        diagnostic_json = 1;
    else if(strcmp(format, "text") == 0)
        diagnostic_json = 0;
    else
        return 0;
    return 1;
}

void
KirDiagnosticV(KirSourceSpan span, const char *code, const char *format, va_list args)
{
    char message[KIR_TEXT_MAX * 2];

    vsnprintf(message, sizeof(message), format, args);
    if(diagnostic_json < 0) {
        const char *environment = getenv("KRYON_DIAGNOSTICS");

        diagnostic_json = environment != NULL && strcmp(environment, "json") == 0;
    }
    if(diagnostic_json) {
        KryJsonBuf json = {0};

        kry_json_buf_raw(&json, "{\"severity\":\"error\",\"code\":");
        kry_json_buf_str(&json, code);
        kry_json_buf_raw(&json, ",\"message\":");
        kry_json_buf_str(&json, message);
        kry_json_buf_raw(&json, ",\"path\":");
        kry_json_buf_str(&json, span.path);
        kry_json_buf_raw(&json, ",\"line\":");
        kry_json_buf_num(&json, span.line);
        kry_json_buf_raw(&json, ",\"column\":");
        kry_json_buf_num(&json, span.column);
        kry_json_buf_raw(&json, ",\"end_line\":");
        kry_json_buf_num(&json, span.end_line > 0 ? span.end_line : span.line);
        kry_json_buf_raw(&json, ",\"end_column\":");
        kry_json_buf_num(&json, span.end_column > 0 ? span.end_column : span.column);
        kry_json_buf_raw(&json, "}");
        fprintf(stderr, "%s\n", kry_json_buf_finish(&json));
        kry_json_buf_free(&json);
    } else if(span.path[0] != '\0') {
        fprintf(stderr, "%s:%d:%d: %s\n", span.path, span.line, span.column, message);
    } else {
        fprintf(stderr, "kry: %s\n", message);
    }
}

void
KirDiagnostic(KirSourceSpan span, const char *code, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    KirDiagnosticV(span, code, format, args);
    va_end(args);
}
