/**
 * @file tcl_wir_emitter.c
 * @brief Tcl Language WIR Emitter Implementation
 *
 * Implementation of the WIRLanguageEmitter interface for Tcl.
 */

#define _POSIX_C_SOURCE 200809L

#include "tcl_wir_emitter.h"
#include "tcl_codegen.h"
#include "../codegen_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Static Emitter Instance
// ============================================================================

/**
 * Tcl language emitter instance.
 * Wraps the vtable and any context data.
 */
typedef struct {
    WIRLanguageEmitter base;  /**< Base emitter interface */
    TclCodegenContext* ctx;   /**< Tcl codegen context (optional) */
} TclWIREmitter;

// ============================================================================
// Emitter VTable Implementation
// ============================================================================

// Note: tcl_escape_string and tcl_quote_string are defined in tcl_codegen.c
// and declared in tcl_codegen.h (included above)

/**
 * Emit a procedure definition.
 */
static bool tcl_emit_proc_definition(StringBuilder* sb, const char* name,
                                      const char* args, const char* body) {
    if (!sb || !name || !body) {
        return false;
    }

    // Write proc header
    sb_append_fmt(sb, "proc %s {%s} {\n", name, args ? args : "");

    // Write body (simple approach - caller handles indentation)
    if (body) {
        sb_append(sb, body);
    }

    // Close proc
    sb_append(sb, "}\n");

    return true;
}

/**
 * Emit a variable assignment.
 */
static bool tcl_emit_variable(StringBuilder* sb, const char* name, const char* value) {
    if (!sb || !name) {
        return false;
    }

    if (value) {
        char* quoted = tcl_quote_string(value);
        if (quoted) {
            sb_append_fmt(sb, "set %s %s\n", name, quoted);
            free(quoted);
        } else {
            sb_append_fmt(sb, "set %s\n", name);
        }
    } else {
        sb_append_fmt(sb, "set %s\n", name);
    }

    return true;
}

/**
 * Emit a comment.
 */
static bool tcl_emit_comment(StringBuilder* sb, const char* text) {
    if (!sb || !text) {
        return false;
    }

    sb_append_fmt(sb, "# %s\n", text);
    return true;
}

/**
 * Emit a function call.
 */
static bool tcl_emit_call(StringBuilder* sb, const char* proc, const char* args) {
    if (!sb || !proc) {
        return false;
    }

    sb_append_fmt(sb, "%s%s%s\n", proc, args ? " " : "", args ? args : "");
    return true;
}

/**
 * Free the emitter.
 */
static void tcl_emitter_free(WIRLanguageEmitter* emitter) {
    if (!emitter) {
        return;
    }

    TclWIREmitter* tcl_emitter = (TclWIREmitter*)emitter;

    // Free context if allocated
    if (tcl_emitter->ctx) {
        tcl_codegen_cleanup(tcl_emitter->ctx);
        free(tcl_emitter->ctx);
    }

    free(tcl_emitter);
}

// ============================================================================
// Public API
// ============================================================================

WIRLanguageEmitter* tcl_wir_language_emitter_create(void) {
    TclWIREmitter* emitter = calloc(1, sizeof(TclWIREmitter));
    if (!emitter) {
        return NULL;
    }

    // Set up vtable
    emitter->base.name = "tcl";
    emitter->base.emit_proc_definition = tcl_emit_proc_definition;
    emitter->base.emit_variable = tcl_emit_variable;
    emitter->base.escape_string = tcl_escape_string;
    emitter->base.quote_string = tcl_quote_string;
    emitter->base.emit_comment = tcl_emit_comment;
    emitter->base.emit_call = tcl_emit_call;
    emitter->base.free = tcl_emitter_free;

    // Create context (optional, for future use)
    // emitter->ctx = NULL;

    return (WIRLanguageEmitter*)emitter;
}

void tcl_wir_language_emitter_free(WIRLanguageEmitter* emitter) {
    if (!emitter) {
        return;
    }

    if (emitter->free) {
        emitter->free(emitter);
    } else {
        free(emitter);
    }
}

// ============================================================================
// Registration
// ============================================================================

// Static instance for registration
static WIRLanguageEmitter* g_tcl_language_emitter = NULL;

void tcl_wir_emitter_init(void) {
    if (g_tcl_language_emitter) {
        return;  // Already initialized
    }

    g_tcl_language_emitter = tcl_wir_language_emitter_create();
    if (g_tcl_language_emitter) {
        wir_composer_register_language("tcl", g_tcl_language_emitter);
    }
}

void tcl_wir_emitter_cleanup(void) {
    if (g_tcl_language_emitter) {
        tcl_wir_language_emitter_free(g_tcl_language_emitter);
        g_tcl_language_emitter = NULL;
    }
}
