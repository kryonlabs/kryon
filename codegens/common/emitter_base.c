/**
 * @file emitter_base.c
 * @brief Shared Emitter Utilities Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "emitter_base.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MAX_INDENT 128
#define DEFAULT_INDENT_SIZE 4

/* ============================================================================
 * Language Data Structure
 * ============================================================================ */

typedef struct {
    EmitterCommentStyle comment_style;
    void (*cleanup)(void*);
} LanguageData;

/* ============================================================================
 * Context Lifecycle
 * ============================================================================ */

bool emitter_init(CodegenEmitterContext* ctx, CodegenOptionsBase* opts) {
    if (!ctx) return false;

    memset(ctx, 0, sizeof(CodegenEmitterContext));

    // Create output string builder
    ctx->output = sb_create(4096);
    if (!ctx->output) {
        return false;
    }

    // Set defaults
    ctx->indent_level = 0;
    ctx->at_line_start = true;

    // Apply options or defaults
    if (opts) {
        ctx->opts = opts;
        ctx->indent_size = opts->indent_size > 0 ? opts->indent_size : DEFAULT_INDENT_SIZE;
        if (opts->indent_string) {
            ctx->indent_string = strdup(opts->indent_string);
        }
    } else {
        ctx->indent_size = DEFAULT_INDENT_SIZE;
    }

    // Create default language data
    LanguageData* lang_data = calloc(1, sizeof(LanguageData));
    if (!lang_data) {
        sb_free(ctx->output);
        if (ctx->indent_string) free(ctx->indent_string);
        return false;
    }
    lang_data->comment_style = EMITTER_COMMENT_C;
    ctx->language_data = lang_data;

    return true;
}

void emitter_cleanup(CodegenEmitterContext* ctx) {
    if (!ctx) return;

    if (ctx->output) {
        sb_free(ctx->output);
        ctx->output = NULL;
    }

    if (ctx->indent_string) {
        free(ctx->indent_string);
        ctx->indent_string = NULL;
    }

    if (ctx->language_data) {
        LanguageData* lang_data = (LanguageData*)ctx->language_data;
        if (lang_data->cleanup) {
            lang_data->cleanup(ctx->language_data);
        }
        free(ctx->language_data);
        ctx->language_data = NULL;
    }
}

/* ============================================================================
 * Indentation Management
 * ============================================================================ */

void emitter_indent(CodegenEmitterContext* ctx) {
    if (!ctx) return;
    ctx->indent_level++;
}

void emitter_dedent(CodegenEmitterContext* ctx) {
    if (!ctx) return;
    if (ctx->indent_level > 0) {
        ctx->indent_level--;
    }
}

const char* emitter_get_indent(CodegenEmitterContext* ctx) {
    if (!ctx) return "";

    static char indent_buffer[MAX_INDENT];
    int total_indent = ctx->indent_level * ctx->indent_size;

    // Use custom indent string if provided
    if (ctx->indent_string) {
        int indent_len = strlen(ctx->indent_string);
        int max_repeats = MAX_INDENT / indent_len;
        int repeats = (total_indent / indent_len);
        if (repeats > max_repeats) repeats = max_repeats;

        char* p = indent_buffer;
        for (int i = 0; i < repeats; i++) {
            strcpy(p, ctx->indent_string);
            p += indent_len;
        }
        *p = '\0';
    } else {
        // Use spaces
        int spaces = total_indent;
        if (spaces >= MAX_INDENT) spaces = MAX_INDENT - 1;
        memset(indent_buffer, ' ', spaces);
        indent_buffer[spaces] = '\0';
    }

    return indent_buffer;
}

/* ============================================================================
 * Output Functions
 * ============================================================================ */

void emitter_write_line(CodegenEmitterContext* ctx, const char* text) {
    if (!ctx || !text) return;

    // Add indent if at line start
    if (ctx->at_line_start) {
        sb_append(ctx->output, emitter_get_indent(ctx));
        ctx->at_line_start = false;
    }

    sb_append(ctx->output, text);
    sb_append(ctx->output, "\n");
    ctx->at_line_start = true;
}

void emitter_write(CodegenEmitterContext* ctx, const char* text) {
    if (!ctx || !text) return;

    // Add indent if at line start
    if (ctx->at_line_start) {
        sb_append(ctx->output, emitter_get_indent(ctx));
        ctx->at_line_start = false;
    }

    sb_append(ctx->output, text);
}

void emitter_write_line_fmt(CodegenEmitterContext* ctx, const char* fmt, ...) {
    if (!ctx || !fmt) return;

    // Add indent if at line start
    if (ctx->at_line_start) {
        sb_append(ctx->output, emitter_get_indent(ctx));
        ctx->at_line_start = false;
    }

    va_list args;
    va_start(args, fmt);
    sb_append_fmt(ctx->output, fmt, args);
    va_end(args);

    sb_append(ctx->output, "\n");
    ctx->at_line_start = true;
}

void emitter_write_fmt(CodegenEmitterContext* ctx, const char* fmt, ...) {
    if (!ctx || !fmt) return;

    // Add indent if at line start
    if (ctx->at_line_start) {
        sb_append(ctx->output, emitter_get_indent(ctx));
        ctx->at_line_start = false;
    }

    va_list args;
    va_start(args, fmt);
    sb_append_fmt(ctx->output, fmt, args);
    va_end(args);
}

void emitter_newline(CodegenEmitterContext* ctx) {
    if (!ctx) return;
    sb_append(ctx->output, "\n");
    ctx->at_line_start = true;
}

void emitter_blank_lines(CodegenEmitterContext* ctx, int count) {
    if (!ctx || count <= 0) return;
    for (int i = 0; i < count; i++) {
        emitter_newline(ctx);
    }
}

/* ============================================================================
 * Code Generation Helpers
 * ============================================================================ */

void emitter_write_comment(CodegenEmitterContext* ctx, const char* text) {
    if (!ctx || !text) return;

    LanguageData* lang_data = (LanguageData*)ctx->language_data;
    if (!lang_data) {
        // Default: C-style comment
        emitter_write_line_fmt(ctx, "// %s", text);
        return;
    }

    switch (lang_data->comment_style) {
        case EMITTER_COMMENT_C:
        case EMITTER_COMMENT_CPP:
            emitter_write_line_fmt(ctx, "// %s", text);
            break;

        case EMITTER_COMMENT_C_BLOCK:
            emitter_write_line_fmt(ctx, "/* %s */", text);
            break;

        case EMITTER_COMMENT_SHELL:
        case EMITTER_COMMENT_LIMBO:
            emitter_write_line_fmt(ctx, "# %s", text);
            break;

        case EMITTER_COMMENT_HTML:
        case EMITTER_COMMENT_MARKDOWN:
            emitter_write_line_fmt(ctx, "<!-- %s -->", text);
            break;

        case EMITTER_COMMENT_LUA:
            emitter_write_line_fmt(ctx, "-- %s", text);
            break;

        default:
            emitter_write_line_fmt(ctx, "// %s", text);
            break;
    }
}

void emitter_write_block_comment(CodegenEmitterContext* ctx, const char* text) {
    if (!ctx || !text) return;

    LanguageData* lang_data = (LanguageData*)ctx->language_data;

    // Check if text contains newlines
    bool multiline = strchr(text, '\n') != NULL;

    if (multiline) {
        // Multi-line comment
        if (lang_data && lang_data->comment_style == EMITTER_COMMENT_HTML) {
            emitter_write_line(ctx, "<!--");
            emitter_indent(ctx);
            emitter_write_line(ctx, text);
            emitter_dedent(ctx);
            emitter_write_line(ctx, "-->");
        } else {
            // Default: C-style block comment
            emitter_write_line(ctx, "/*");
            emitter_indent(ctx);
            emitter_write_line(ctx, text);
            emitter_dedent(ctx);
            emitter_write_line(ctx, "*/");
        }
    } else {
        // Single line - use single-line comment
        emitter_write_comment(ctx, text);
    }
}

void emitter_start_block(CodegenEmitterContext* ctx) {
    if (!ctx) return;
    emitter_write_line(ctx, "{");
    emitter_indent(ctx);
}

void emitter_end_block(CodegenEmitterContext* ctx) {
    if (!ctx) return;
    emitter_dedent(ctx);
    emitter_write_line(ctx, "}");
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

char* emitter_get_output(CodegenEmitterContext* ctx) {
    if (!ctx || !ctx->output) return NULL;
    return sb_get(ctx->output);
}

void emitter_clear_output(CodegenEmitterContext* ctx) {
    if (!ctx || !ctx->output) return;
    sb_clear(ctx->output);
    ctx->at_line_start = true;
}

void emitter_reset(CodegenEmitterContext* ctx) {
    if (!ctx) return;
    emitter_clear_output(ctx);
    ctx->indent_level = 0;
    ctx->at_line_start = true;
}

/* ============================================================================
 * Language-Specific Extensions
 * ============================================================================ */

void emitter_set_language_data(CodegenEmitterContext* ctx, void* data, void (*cleanup)(void*)) {
    if (!ctx) return;

    // Clean up existing language data
    if (ctx->language_data) {
        LanguageData* lang_data = (LanguageData*)ctx->language_data;
        if (lang_data->cleanup) {
            lang_data->cleanup(ctx->language_data);
        }
        free(ctx->language_data);
    }

    // Create new language data wrapper
    if (data) {
        LanguageData* lang_data = calloc(1, sizeof(LanguageData));
        if (lang_data) {
            lang_data->comment_style = EMITTER_COMMENT_C;  // Default
            lang_data->cleanup = cleanup;
            // Store user data after our structure
            // Note: This is a simple implementation. For more complex needs,
            // users should provide their own LanguageData structure.
        }
        ctx->language_data = lang_data;
    } else {
        ctx->language_data = NULL;
    }
}

/* ============================================================================
 * Comment Style Handlers
 * ============================================================================ */

void emitter_set_comment_style(CodegenEmitterContext* ctx, EmitterCommentStyle style) {
    if (!ctx || !ctx->language_data) return;

    LanguageData* lang_data = (LanguageData*)ctx->language_data;
    lang_data->comment_style = style;
}
