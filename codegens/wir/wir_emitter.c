/**
 * @file wir_emitter.c
 * @brief WIR Emitter Implementation
 *
 * Implementation of the emitter interface and registry.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#define _POSIX_C_SOURCE 200809L

#include "wir_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Emitter Registry
// ============================================================================

#define MAX_EMITTERS 16

static struct {
    WIREmitterType type;
    const WIREmitterVTable* vtable;
} emitter_registry[MAX_EMITTERS];

static int emitter_count = 0;

bool wir_emitter_register(WIREmitterType type, const WIREmitterVTable* vtable) {
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

void wir_emitter_unregister(WIREmitterType type) {
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

WIREmitterContext* wir_emitter_create(WIREmitterType type,
                                         const WIREmitterOptions* options) {
    // Find registered emitter
    const WIREmitterVTable* vtable = NULL;
    for (int i = 0; i < emitter_count; i++) {
        if (emitter_registry[i].type == type) {
            vtable = emitter_registry[i].vtable;
            break;
        }
    }

    if (!vtable) {
        wir_error("Emitter type %d not registered", type);
        return NULL;
    }

    return wir_emitter_context_create(vtable, options);
}

// ============================================================================
// Emitter Context
// ============================================================================

WIREmitterContext* wir_emitter_context_create(const WIREmitterVTable* vtable,
                                                 const WIREmitterOptions* options) {
    if (!vtable) {
        wir_error("NULL vtable provided to context_create");
        return NULL;
    }

    WIREmitterContext* ctx = calloc(1, sizeof(WIREmitterContext));
    if (!ctx) {
        wir_error("Failed to allocate emitter context");
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

void wir_emitter_context_free(WIREmitterContext* ctx) {
    if (!ctx) return;

    // Call target-specific cleanup
    if (ctx->vtable && ctx->vtable->free_context) {
        ctx->vtable->free_context(ctx);
    }

    free(ctx);
}

void wir_emitter_set_target_data(WIREmitterContext* ctx, void* data) {
    if (ctx) {
        ctx->target_data = data;
    }
}

void* wir_emitter_get_target_data(WIREmitterContext* ctx) {
    return ctx ? ctx->target_data : NULL;
}

// ============================================================================
// Main Emit Function
// ============================================================================

char* wir_emit(WIREmitterContext* ctx, WIRRoot* root) {
    if (!ctx || !root) {
        wir_error("Invalid arguments to wir_emit");
        return NULL;
    }

    if (!ctx->vtable || !ctx->vtable->emit_full) {
        wir_error("Emitter does not implement emit_full");
        return NULL;
    }

    return ctx->vtable->emit_full(ctx, root);
}

// ============================================================================
// Convenience Functions
// ============================================================================

extern const WIREmitterVTable tcltk_emitter_vtable;
extern const WIREmitterVTable limbo_emitter_vtable;
extern const WIREmitterVTable c_emitter_vtable;

WIREmitterContext* wir_emitter_create_tcltk(const WIREmitterOptions* options) {
    return wir_emitter_create(WIR_EMITTER_TCLTK, options);
}

WIREmitterContext* wir_emitter_create_limbo(const WIREmitterOptions* options) {
    return wir_emitter_create(WIR_EMITTER_LIMBO, options);
}

WIREmitterContext* wir_emitter_create_c(const WIREmitterOptions* options) {
    return wir_emitter_create(WIR_EMITTER_C, options);
}
