/**
 * @file tcl_codegen.c
 * @brief Tcl Language Code Generator Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "tcl_codegen.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Context Management
// ============================================================================

bool tcl_codegen_init(TclCodegenContext* ctx, FILE* output) {
    if (!ctx || !output) return false;

    memset(ctx, 0, sizeof(TclCodegenContext));
    ctx->output = output;
    ctx->indent_level = 0;
    ctx->indent_spaces = 4;
    ctx->use_tabs = false;
    ctx->line_length = 0;
    ctx->max_line_width = 0;
    ctx->in_proc = false;
    ctx->needs_brace = false;

    return true;
}

void tcl_codegen_cleanup(TclCodegenContext* ctx) {
    if (!ctx) return;
    // Nothing to clean up currently (no dynamic allocations in context)
}

// ============================================================================
// Indentation
// ============================================================================

static void write_indent(TclCodegenContext* ctx) {
    if (!ctx) return;

    if (ctx->use_tabs) {
        for (int i = 0; i < ctx->indent_level; i++) {
            fprintf(ctx->output, "\t");
        }
    } else {
        int spaces = ctx->indent_level * ctx->indent_spaces;
        for (int i = 0; i < spaces; i++) {
            fprintf(ctx->output, " ");
        }
    }
}

void tcl_indent(TclCodegenContext* ctx) {
    if (ctx) ctx->indent_level++;
}

void tcl_dedent(TclCodegenContext* ctx) {
    if (ctx && ctx->indent_level > 0) ctx->indent_level--;
}

// ============================================================================
// Output Functions
// ============================================================================

bool tcl_write_line(TclCodegenContext* ctx, const char* text) {
    if (!ctx || !text) return false;

    write_indent(ctx);
    fputs(text, ctx->output);
    fputs("\n", ctx->output);
    ctx->line_length = 0;

    return true;
}

bool tcl_gen_newline(TclCodegenContext* ctx) {
    if (!ctx) return false;
    fputs("\n", ctx->output);
    ctx->line_length = 0;
    return true;
}

// ============================================================================
// String Handling
// ============================================================================

char* tcl_escape_string(const char* input) {
    if (!input) return strdup("");

    // Calculate escaped size
    size_t len = strlen(input);
    size_t escaped_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\\' || input[i] == '$' || input[i] == '[' || input[i] == ']' ||
            input[i] == '"' || input[i] == '{' || input[i] == '}') {
            escaped_len += 2;
        } else {
            escaped_len++;
        }
    }

    // Allocate and escape
    char* result = malloc(escaped_len + 1);
    if (!result) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '\\': case '$': case '[': case ']': case '"': case '{': case '}':
                result[j++] = '\\';
                result[j++] = input[i];
                break;
            default:
                result[j++] = input[i];
                break;
        }
    }
    result[j] = '\0';

    return result;
}

char* tcl_quote_string(const char* input) {
    if (!input) return strdup("{}");

    // Check if we need braces (contains special chars)
    bool needs_braces = false;
    for (size_t i = 0; i < strlen(input); i++) {
        if (input[i] == '$' || input[i] == '[' || input[i] == ']' ||
            input[i] == '\\' || input[i] == '"' || input[i] == ' ' ||
            input[i] == '\t' || input[i] == '\n') {
            needs_braces = true;
            break;
        }
    }

    // If simple, return as-is
    if (!needs_braces) {
        return strdup(input);
    }

    // Escape and wrap in braces
    char* escaped = tcl_escape_string(input);
    if (!escaped) return NULL;

    size_t len = strlen(escaped);
    char* result = malloc(len + 3);  // braces + null
    if (!result) {
        free(escaped);
        return NULL;
    }

    result[0] = '{';
    strcpy(result + 1, escaped);
    result[len + 1] = '}';
    result[len + 2] = '\0';

    free(escaped);
    return result;
}

bool tcl_gen_string_literal(TclCodegenContext* ctx, const char* str) {
    if (!ctx || !str) return false;

    char* quoted = tcl_quote_string(str);
    if (!quoted) return false;

    fputs(quoted, ctx->output);
    free(quoted);

    return true;
}

// ============================================================================
// Variable Operations
// ============================================================================

bool tcl_gen_variable(TclCodegenContext* ctx, const char* name, const char* value) {
    if (!ctx || !name) return false;

    if (value) {
        fprintf(ctx->output, "set %s ", name);
        tcl_gen_string_literal(ctx, value);
    } else {
        fprintf(ctx->output, "set %s", name);
    }
    fputs("\n", ctx->output);

    return true;
}

bool tcl_gen_set(TclCodegenContext* ctx, const char* name, const char* value) {
    return tcl_gen_variable(ctx, name, value);
}

bool tcl_gen_get(TclCodegenContext* ctx, const char* name) {
    if (!ctx || !name) return false;
    fprintf(ctx->output, "$%s", name);
    return true;
}

// ============================================================================
// Control Structures
// ============================================================================

bool tcl_gen_proc(TclCodegenContext* ctx, const char* name, const char* args, const char* body) {
    if (!ctx || !name || !body) return false;

    // Write proc header
    write_indent(ctx);
    fprintf(ctx->output, "proc %s {%s} {\n", name, args ? args : "");

    ctx->in_proc = true;
    tcl_indent(ctx);

    // Write body
    if (body) {
        // Simple approach: write body as-is (caller handles indentation)
        fputs(body, ctx->output);
    }

    tcl_dedent(ctx);
    ctx->in_proc = false;

    // Close proc
    write_indent(ctx);
    fputs("}\n", ctx->output);

    return true;
}

bool tcl_gen_call(TclCodegenContext* ctx, const char* proc, const char* args) {
    if (!ctx || !proc) return false;

    fprintf(ctx->output, "%s%s%s\n", proc, args ? " " : "", args ? args : "");
    return true;
}

bool tcl_gen_if(TclCodegenContext* ctx, const char* condition, const char* then_body, const char* else_body) {
    if (!ctx || !condition || !then_body) return false;

    // Write if
    write_indent(ctx);
    fprintf(ctx->output, "if %s {\n", condition);

    tcl_indent(ctx);
    fputs(then_body, ctx->output);
    tcl_dedent(ctx);

    // Write else if present
    if (else_body) {
        write_indent(ctx);
        fputs("} else {\n", ctx->output);

        tcl_indent(ctx);
        fputs(else_body, ctx->output);
        tcl_dedent(ctx);
    }

    // Close if
    write_indent(ctx);
    fputs("}\n", ctx->output);

    return true;
}

bool tcl_gen_for(TclCodegenContext* ctx, const char* var, const char* list, const char* body) {
    if (!ctx || !var || !list || !body) return false;

    write_indent(ctx);
    fprintf(ctx->output, "foreach %s %s {\n", var, list);

    tcl_indent(ctx);
    fputs(body, ctx->output);
    tcl_dedent(ctx);

    write_indent(ctx);
    fputs("}\n", ctx->output);

    return true;
}

bool tcl_gen_while(TclCodegenContext* ctx, const char* condition, const char* body) {
    if (!ctx || !condition || !body) return false;

    write_indent(ctx);
    fprintf(ctx->output, "while %s {\n", condition);

    tcl_indent(ctx);
    fputs(body, ctx->output);
    tcl_dedent(ctx);

    write_indent(ctx);
    fputs("}\n", ctx->output);

    return true;
}

// ============================================================================
// Comments
// ============================================================================

bool tcl_gen_comment(TclCodegenContext* ctx, const char* text) {
    if (!ctx || !text) return false;

    write_indent(ctx);
    fprintf(ctx->output, "# %s\n", text);

    return true;
}

// ============================================================================
// Data Structures
// ============================================================================

bool tcl_gen_list(TclCodegenContext* ctx, const char** elements, size_t count) {
    if (!ctx || !elements || count == 0) return false;

    fputs("{", ctx->output);
    for (size_t i = 0; i < count; i++) {
        if (i > 0) fputs(" ", ctx->output);
        char* quoted = tcl_quote_string(elements[i]);
        if (quoted) {
            fputs(quoted, ctx->output);
            free(quoted);
        }
    }
    fputs("}", ctx->output);

    return true;
}

bool tcl_gen_dict(TclCodegenContext* ctx, const char** keys, const char** values, size_t count) {
    if (!ctx || !keys || !values || count == 0) return false;

    fputs("[dict create", ctx->output);
    for (size_t i = 0; i < count; i++) {
        fprintf(ctx->output, " ");
        char* key_quoted = tcl_quote_string(keys[i]);
        char* value_quoted = tcl_quote_string(values[i]);
        if (key_quoted && value_quoted) {
            fprintf(ctx->output, "%s %s", key_quoted, value_quoted);
        }
        free(key_quoted);
        free(value_quoted);
    }
    fputs("]", ctx->output);

    return true;
}

bool tcl_gen_subst(TclCodegenContext* ctx, const char* command) {
    if (!ctx || !command) return false;

    fprintf(ctx->output, "[%s]", command);
    return true;
}
