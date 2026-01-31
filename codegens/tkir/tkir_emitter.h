/**
 * @file tkir_emitter.h
 * @brief TKIR Emitter Interface
 *
 * Emitter interface for generating code from TKIR.
 * Each target platform (Tcl/Tk, Limbo, C, etc.) implements this interface.
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#ifndef TKIR_EMITTER_H
#define TKIR_EMITTER_H

#include "tkir.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Emitter Context
// ============================================================================

/**
 * Forward declaration
 */
typedef struct TKIREmitterContext TKIREmitterContext;

/**
 * Emitter configuration options.
 */
typedef struct {
    bool include_comments;        /**< Add comments to generated code */
    bool verbose;                 /**< Enable verbose output */
    const char* indent_string;    /**< String used for indentation (e.g., "  ") */
    int indent_level;             /**< Current indentation level */
} TKIREmitterOptions;

/**
 * Emitter virtual function table.
 * Each emitter implementation must provide these functions.
 */
typedef struct {
    /**
     * Emit code for a widget.
     *
     * @param ctx Emitter context
     * @param widget TKIRWidget to emit
     * @return true on success, false on error
     */
    bool (*emit_widget)(TKIREmitterContext* ctx, TKIRWidget* widget);

    /**
     * Emit code for a handler.
     *
     * @param ctx Emitter context
     * @param handler TKIRHandler to emit
     * @return true on success, false on error
     */
    bool (*emit_handler)(TKIREmitterContext* ctx, TKIRHandler* handler);

    /**
     * Emit layout command for a widget.
     *
     * @param ctx Emitter context
     * @param widget TKIRWidget with layout options
     * @return true on success, false on error
     */
    bool (*emit_layout)(TKIREmitterContext* ctx, TKIRWidget* widget);

    /**
     * Emit property assignment.
     *
     * @param ctx Emitter context
     * @param widget_id Widget ID
     * @param property_name Property name
     * @param value Property value (as cJSON)
     * @return true on success, false on error
     */
    bool (*emit_property)(TKIREmitterContext* ctx,
                          const char* widget_id,
                          const char* property_name,
                          cJSON* value);

    /**
     * Emit the complete output file.
     *
     * @param ctx Emitter context
     * @param root TKIRRoot to emit
     * @return Generated code string (caller must free), or NULL on error
     */
    char* (*emit_full)(TKIREmitterContext* ctx, TKIRRoot* root);

    /**
     * Free emitter context.
     *
     * @param ctx Context to free
     */
    void (*free_context)(TKIREmitterContext* ctx);

} TKIREmitterVTable;

/**
 * Emitter context structure.
 */
struct TKIREmitterContext {
    const TKIREmitterVTable* vtable;  /**< Virtual function table */
    TKIREmitterOptions options;        /**< Emitter options */
    void* target_data;                /**< Target-specific data */
};

/**
 * Create a generic emitter context.
 *
 * @param vtable Virtual function table (must be allocated statically by emitter)
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
TKIREmitterContext* tkir_emitter_context_create(const TKIREmitterVTable* vtable,
                                                 const TKIREmitterOptions* options);

/**
 * Free an emitter context.
 *
 * @param ctx Context to free
 */
void tkir_emitter_context_free(TKIREmitterContext* ctx);

/**
 * Set target-specific data.
 *
 * @param ctx Emitter context
 * @param data Target-specific data pointer
 */
void tkir_emitter_set_target_data(TKIREmitterContext* ctx, void* data);

/**
 * Get target-specific data.
 *
 * @param ctx Emitter context
 * @return Target-specific data pointer
 */
void* tkir_emitter_get_target_data(TKIREmitterContext* ctx);

/**
 * Emit code from TKIR using the specified emitter.
 *
 * @param ctx Emitter context
 * @param root TKIRRoot to emit
 * @return Generated code string (caller must free), or NULL on error
 */
char* tkir_emit(TKIREmitterContext* ctx, TKIRRoot* root);

// ============================================================================
// Emitter Registry
// ============================================================================

/**
 * Emitter type enumeration.
 */
typedef enum {
    TKIR_EMITTER_TCLTK,      /**< Tcl/Tk emitter */
    TKIR_EMITTER_LIMBO,      /**< Limbo emitter */
    TKIR_EMITTER_C,          /**< C emitter */
    TKIR_EMITTER_ANDROID,    /**< Android emitter */
    TKIR_EMITTER_CUSTOM      /**< Custom emitter */
} TKIREmitterType;

/**
 * Register an emitter implementation.
 *
 * @param type Emitter type
 * @param vtable Virtual function table (must be statically allocated)
 * @return true on success, false on error
 */
bool tkir_emitter_register(TKIREmitterType type, const TKIREmitterVTable* vtable);

/**
 * Create an emitter context for a specific type.
 *
 * @param type Emitter type
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL if type not registered
 */
TKIREmitterContext* tkir_emitter_create(TKIREmitterType type,
                                         const TKIREmitterOptions* options);

/**
 * Unregister an emitter.
 *
 * @param type Emitter type
 */
void tkir_emitter_unregister(TKIREmitterType type);

// ============================================================================
// Convenience Functions for Common Emitters
// ============================================================================

/**
 * Create a Tcl/Tk emitter context.
 *
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
TKIREmitterContext* tkir_emitter_create_tcltk(const TKIREmitterOptions* options);

/**
 * Create a Limbo emitter context.
 *
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
TKIREmitterContext* tkir_emitter_create_limbo(const TKIREmitterOptions* options);

/**
 * Create a C emitter context.
 *
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
TKIREmitterContext* tkir_emitter_create_c(const TKIREmitterOptions* options);

#ifdef __cplusplus
}
#endif

#endif // TKIR_EMITTER_H
