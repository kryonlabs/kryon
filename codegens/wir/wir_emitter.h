/**
 * @file wir_emitter.h
 * @brief WIR Emitter Interface
 *
 * Emitter interface for generating code from WIR.
 * Each target platform (Tcl/Tk, Limbo, C, etc.) implements this interface.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#ifndef WIR_EMITTER_H
#define WIR_EMITTER_H

#include "wir.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Emitter Context
// ============================================================================

/**
 * Forward declaration
 */
typedef struct WIREmitterContext WIREmitterContext;

/**
 * Emitter configuration options.
 */
typedef struct {
    bool include_comments;        /**< Add comments to generated code */
    bool verbose;                 /**< Enable verbose output */
    const char* indent_string;    /**< String used for indentation (e.g., "  ") */
    int indent_level;             /**< Current indentation level */
} WIREmitterOptions;

/**
 * Emitter virtual function table.
 * Each emitter implementation must provide these functions.
 */
typedef struct {
    /**
     * Emit code for a widget.
     *
     * @param ctx Emitter context
     * @param widget WIRWidget to emit
     * @return true on success, false on error
     */
    bool (*emit_widget)(WIREmitterContext* ctx, WIRWidget* widget);

    /**
     * Emit code for a handler.
     *
     * @param ctx Emitter context
     * @param handler WIRHandler to emit
     * @return true on success, false on error
     */
    bool (*emit_handler)(WIREmitterContext* ctx, WIRHandler* handler);

    /**
     * Emit layout command for a widget.
     *
     * @param ctx Emitter context
     * @param widget WIRWidget with layout options
     * @return true on success, false on error
     */
    bool (*emit_layout)(WIREmitterContext* ctx, WIRWidget* widget);

    /**
     * Emit property assignment.
     *
     * @param ctx Emitter context
     * @param widget_id Widget ID
     * @param property_name Property name
     * @param value Property value (as cJSON)
     * @return true on success, false on error
     */
    bool (*emit_property)(WIREmitterContext* ctx,
                          const char* widget_id,
                          const char* property_name,
                          cJSON* value);

    /**
     * Emit the complete output file.
     *
     * @param ctx Emitter context
     * @param root WIRRoot to emit
     * @return Generated code string (caller must free), or NULL on error
     */
    char* (*emit_full)(WIREmitterContext* ctx, WIRRoot* root);

    /**
     * Free emitter context.
     *
     * @param ctx Context to free
     */
    void (*free_context)(WIREmitterContext* ctx);

} WIREmitterVTable;

/**
 * Emitter context structure.
 */
struct WIREmitterContext {
    const WIREmitterVTable* vtable;  /**< Virtual function table */
    WIREmitterOptions options;        /**< Emitter options */
    void* target_data;                /**< Target-specific data */
};

/**
 * Create a generic emitter context.
 *
 * @param vtable Virtual function table (must be allocated statically by emitter)
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
WIREmitterContext* wir_emitter_context_create(const WIREmitterVTable* vtable,
                                                 const WIREmitterOptions* options);

/**
 * Free an emitter context.
 *
 * @param ctx Context to free
 */
void wir_emitter_context_free(WIREmitterContext* ctx);

/**
 * Set target-specific data.
 *
 * @param ctx Emitter context
 * @param data Target-specific data pointer
 */
void wir_emitter_set_target_data(WIREmitterContext* ctx, void* data);

/**
 * Get target-specific data.
 *
 * @param ctx Emitter context
 * @return Target-specific data pointer
 */
void* wir_emitter_get_target_data(WIREmitterContext* ctx);

/**
 * Emit code from WIR using the specified emitter.
 *
 * @param ctx Emitter context
 * @param root WIRRoot to emit
 * @return Generated code string (caller must free), or NULL on error
 */
char* wir_emit(WIREmitterContext* ctx, WIRRoot* root);

// ============================================================================
// Emitter Registry
// ============================================================================

/**
 * Emitter type enumeration.
 */
typedef enum {
    WIR_EMITTER_TCLTK,      /**< Tcl/Tk emitter */
    WIR_EMITTER_LIMBO,      /**< Limbo emitter */
    WIR_EMITTER_C,          /**< C emitter */
    WIR_EMITTER_ANDROID,    /**< Android emitter */
    WIR_EMITTER_CUSTOM      /**< Custom emitter */
} WIREmitterType;

/**
 * Register an emitter implementation.
 *
 * @param type Emitter type
 * @param vtable Virtual function table (must be statically allocated)
 * @return true on success, false on error
 */
bool wir_emitter_register(WIREmitterType type, const WIREmitterVTable* vtable);

/**
 * Create an emitter context for a specific type.
 *
 * @param type Emitter type
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL if type not registered
 */
WIREmitterContext* wir_emitter_create(WIREmitterType type,
                                         const WIREmitterOptions* options);

/**
 * Unregister an emitter.
 *
 * @param type Emitter type
 */
void wir_emitter_unregister(WIREmitterType type);

// ============================================================================
// Convenience Functions for Common Emitters
// ============================================================================

/**
 * Create a Tcl/Tk emitter context.
 *
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
WIREmitterContext* wir_emitter_create_tcltk(const WIREmitterOptions* options);

/**
 * Create a Limbo emitter context.
 *
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
WIREmitterContext* wir_emitter_create_limbo(const WIREmitterOptions* options);

/**
 * Create a C emitter context.
 *
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated context, or NULL on error
 */
WIREmitterContext* wir_emitter_create_c(const WIREmitterOptions* options);

#ifdef __cplusplus
}
#endif

#endif // WIR_EMITTER_H
