/**
 * C Code Generator - Output Utilities Implementation
 */

#include "ir_c_output.h"

void c_write_indent(CCodegenContext* ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        fprintf(ctx->output, "    ");
    }
}

void c_writeln(CCodegenContext* ctx, const char* str) {
    c_write_indent(ctx);
    fprintf(ctx->output, "%s\n", str);
}

void c_write_raw(CCodegenContext* ctx, const char* str) {
    fprintf(ctx->output, "%s", str);
}
