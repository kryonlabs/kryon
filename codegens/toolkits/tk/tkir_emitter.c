/**
 * @file tkir_emitter.c
 * @brief TKIR Emitter Implementation
 *
 * Implementation of the emitter interface and registry.
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L

#include "tkir_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Emitter Registry
// ============================================================================

#define MAX_EMITTERS 16

static struct {
    TKIREmitterType type;
    const TKIREmitterVTable* vtable;
} emitter_registry[MAX_EMITTERS];

static int emitter_count = 0;

bool tkir_emitter_register(TKIREmitterType type, const TKIREmitterVTable* vtable) {
    if (!vtable) return false;

    // Check if already registered
    for (int i = 0; i < emitter_count; i++) {
        if (emitter_registry[i].type == type) {
            // Update existing registration
            emitter_registry[i].vtable = vtable;
            return true;
        }
    }

    // Add new registration
    if (emitter_count >= MAX_EMITTERS) {
        return false;  // Registry full
    }

    emitter_registry[emitter_count].type = type;
    emitter_registry[emitter_count].vtable = vtable;
    emitter_count++;

    return true;
}

void tkir_emitter_unregister(TKIREmitterType type) {
    for (int i = 0; i < emitter_count; i++) {
        if (emitter_registry[i].type == type) {
            // Shift remaining entries
            for (int j = i; j < emitter_count - 1; j++) {
                emitter_registry[j] = emitter_registry[j + 1];
            }
            emitter_count--;
            return;
        }
    }
}

TKIREmitterContext* tkir_emitter_create(TKIREmitterType type,
                                         const TKIREmitterOptions* options) {
    // Find registered emitter
    const TKIREmitterVTable* vtable = NULL;
    for (int i = 0; i < emitter_count; i++) {
        if (emitter_registry[i].type == type) {
            vtable = emitter_registry[i].vtable;
            break;
        }
    }

    if (!vtable) {
        tkir_error("Emitter type %d not registered", type);
        return NULL;
    }

    return tkir_emitter_context_create(vtable, options);
}

// ============================================================================
// Emitter Context
// ============================================================================

TKIREmitterContext* tkir_emitter_context_create(const TKIREmitterVTable* vtable,
                                                 const TKIREmitterOptions* options) {
    if (!vtable) {
        tkir_error("NULL vtable provided to context_create");
        return NULL;
    }

    TKIREmitterContext* ctx = calloc(1, sizeof(TKIREmitterContext));
    if (!ctx) {
        tkir_error("Failed to allocate emitter context");
        return NULL;
    }

    ctx->vtable = vtable;
    ctx->target_data = NULL;

    // Set options
    if (options) {
        ctx->options = *options;
    } else {
        // Defaults
        ctx->options.include_comments = true;
        ctx->options.verbose = false;
        ctx->options.indent_string = "  ";
        ctx->options.indent_level = 0;
    }

    return ctx;
}

void tkir_emitter_context_free(TKIREmitterContext* ctx) {
    if (!ctx) return;

    // Call target-specific cleanup
    if (ctx->vtable && ctx->vtable->free_context) {
        ctx->vtable->free_context(ctx);
    }

    free(ctx);
}

void tkir_emitter_set_target_data(TKIREmitterContext* ctx, void* data) {
    if (ctx) {
        ctx->target_data = data;
    }
}

void* tkir_emitter_get_target_data(TKIREmitterContext* ctx) {
    return ctx ? ctx->target_data : NULL;
}

// ============================================================================
// Main Emit Function
// ============================================================================

char* tkir_emit(TKIREmitterContext* ctx, TKIRRoot* root) {
    if (!ctx || !root) {
        tkir_error("Invalid arguments to tkir_emit");
        return NULL;
    }

    if (!ctx->vtable || !ctx->vtable->emit_full) {
        tkir_error("Emitter does not implement emit_full");
        return NULL;
    }

    return ctx->vtable->emit_full(ctx, root);
}

// ============================================================================
// Convenience Functions
// ============================================================================

extern const TKIREmitterVTable tcltk_emitter_vtable;
extern const TKIREmitterVTable limbo_emitter_vtable;
extern const TKIREmitterVTable c_emitter_vtable;

TKIREmitterContext* tkir_emitter_create_tcltk(const TKIREmitterOptions* options) {
    return tkir_emitter_create(TKIR_EMITTER_TCLTK, options);
}

TKIREmitterContext* tkir_emitter_create_limbo(const TKIREmitterOptions* options) {
    return tkir_emitter_create(TKIR_EMITTER_LIMBO, options);
}

TKIREmitterContext* tkir_emitter_create_c(const TKIREmitterOptions* options) {
    return tkir_emitter_create(TKIR_EMITTER_C, options);
}
