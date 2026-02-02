/**
 * @file wir_composer.h
 * @brief WIR Composer - Language and Toolkit Composition Interface
 *
 * Provides a composition pattern for separating language and toolkit emitters.
 * This enables flexible language+toolkit combinations (e.g., tcl+tk, tcl+terminal,
 * limbo+tk, c+tk) without creating N*M hardcoded implementations.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#ifndef WIR_COMPOSER_H
#define WIR_COMPOSER_H

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct cJSON;
typedef struct cJSON cJSON;
struct WIRWidget;
struct WIRHandler;
struct WIRRoot;
typedef struct WIRWidget WIRWidget;
typedef struct WIRHandler WIRHandler;
typedef struct WIRRoot WIRRoot;
struct StringBuilder;
typedef struct StringBuilder StringBuilder;

// ============================================================================
// Language Emitter Interface
// ============================================================================

/**
 * Language emitter virtual function table.
 * Language emitters handle language-specific syntax: procs, variables,
 * control flow, string escaping, etc.
 */
typedef struct WIRLanguageEmitter WIRLanguageEmitter;

struct WIRLanguageEmitter {
    const char* name;  /**< Language name (e.g., "tcl", "limbo", "c") */

    /**
     * Emit a procedure/function definition.
     * @param sb String builder for output
     * @param name Procedure name
     * @param args Argument list (language-specific format)
     * @param body Procedure body
     * @return true on success, false on error
     */
    bool (*emit_proc_definition)(StringBuilder* sb, const char* name,
                                  const char* args, const char* body);

    /**
     * Emit a variable assignment.
     * @param sb String builder for output
     * @param name Variable name
     * @param value Variable value (NULL for declaration without assignment)
     * @return true on success, false on error
     */
    bool (*emit_variable)(StringBuilder* sb, const char* name, const char* value);

    /**
     * Escape a string for this language.
     * @param input Input string
     * @return Newly allocated escaped string (caller must free), or NULL on error
     */
    char* (*escape_string)(const char* input);

    /**
     * Quote a string for this language.
     * @param input Input string
     * @return Newly allocated quoted string (caller must free), or NULL on error
     */
    char* (*quote_string)(const char* input);

    /**
     * Emit a comment.
     * @param sb String builder for output
     * @param text Comment text
     * @return true on success, false on error
     */
    bool (*emit_comment)(StringBuilder* sb, const char* text);

    /**
     * Emit a function call.
     * @param sb String builder for output
     * @param proc Procedure/function name
     * @param args Arguments (language-specific format)
     * @return true on success, false on error
     */
    bool (*emit_call)(StringBuilder* sb, const char* proc, const char* args);

    /**
     * Free language emitter context.
     * @param emitter Language emitter to free
     */
    void (*free)(WIRLanguageEmitter* emitter);
};

// ============================================================================
// Toolkit Emitter Interface
// ============================================================================

/**
 * Toolkit emitter virtual function table.
 * Toolkit emitters handle toolkit-specific widgets, properties, layout,
 * and event binding.
 */
typedef struct WIRToolkitEmitter WIRToolkitEmitter;

struct WIRToolkitEmitter {
    const char* name;  /**< Toolkit name (e.g., "tk", "terminal", "dom") */

    /**
     * Emit widget creation command.
     * @param sb String builder for output
     * @param widget WIR widget
     * @return true on success, false on error
     */
    bool (*emit_widget_creation)(StringBuilder* sb, WIRWidget* widget);

    /**
     * Emit property assignment.
     * @param sb String builder for output
     * @param widget_id Widget ID
     * @param property_name Property name
     * @param value Property value (cJSON)
     * @return true on success, false on error
     */
    bool (*emit_property_assignment)(StringBuilder* sb, const char* widget_id,
                                      const char* property_name, cJSON* value);

    /**
     * Emit layout command.
     * @param sb String builder for output
     * @param widget WIR widget with layout options
     * @return true on success, false on error
     */
    bool (*emit_layout_command)(StringBuilder* sb, WIRWidget* widget);

    /**
     * Emit event binding.
     * @param sb String builder for output
     * @param handler WIR handler
     * @return true on success, false on error
     */
    bool (*emit_event_binding)(StringBuilder* sb, WIRHandler* handler);

    /**
     * Map widget type from KIR to toolkit-specific type.
     * @param kir_type KIR widget type (e.g., "Button", "Text")
     * @return Toolkit-specific widget type (e.g., "button", "label")
     */
    const char* (*map_widget_type)(const char* kir_type);

    /**
     * Map event name from KIR to toolkit-specific event.
     * @param event KIR event name (e.g., "click", "keypress")
     * @return Toolkit-specific event name (e.g., "Button-1", "Key")
     */
    const char* (*map_event_name)(const char* event);

    /**
     * Free toolkit emitter context.
     * @param emitter Toolkit emitter to free
     */
    void (*free)(WIRToolkitEmitter* emitter);
};

// ============================================================================
// Composed Emitter
// ============================================================================

/**
 * Composed emitter combining a language and toolkit.
 * This is the main interface for generating code from WIR.
 */
typedef struct {
    WIRLanguageEmitter* language;  /**< Language emitter */
    WIRToolkitEmitter* toolkit;    /**< Toolkit emitter */
    void* context;                 /**< Private context data */
} WIRComposedEmitter;

/**
 * Emitter options for composed emitters.
 */
typedef struct {
    bool include_comments;     /**< Add comments to generated code */
    bool verbose;              /**< Enable verbose output */
    const char* indent_string; /**< Indentation string (e.g., "  ") */
    int indent_level;          /**< Current indentation level */
} WIRComposerOptions;

// ============================================================================
// Composition API
// ============================================================================

/**
 * Initialize the WIR composer registry.
 * Must be called before using any composer functions.
 */
void wir_composer_init(void);

/**
 * Cleanup the WIR composer registry.
 */
void wir_composer_cleanup(void);

/**
 * Register a language emitter.
 * @param name Language name (e.g., "tcl", "limbo", "c")
 * @param emitter Language emitter (must be statically allocated or managed by caller)
 * @return true on success, false on error
 */
bool wir_composer_register_language(const char* name, WIRLanguageEmitter* emitter);

/**
 * Register a toolkit emitter.
 * @param name Toolkit name (e.g., "tk", "terminal", "dom")
 * @param emitter Toolkit emitter (must be statically allocated or managed by caller)
 * @return true on success, false on error
 */
bool wir_composer_register_toolkit(const char* name, WIRToolkitEmitter* emitter);

/**
 * Create a composed emitter from language and toolkit names.
 * @param language Language name (e.g., "tcl", "limbo")
 * @param toolkit Toolkit name (e.g., "tk", "terminal")
 * @param options Emitter options (can be NULL for defaults)
 * @return Newly allocated composed emitter (caller must free with wir_composer_free()),
 *         or NULL if language or toolkit not found
 */
WIRComposedEmitter* wir_compose(const char* language, const char* toolkit,
                                const WIRComposerOptions* options);

/**
 * Free a composed emitter.
 * @param emitter Composed emitter to free (can be NULL)
 */
void wir_composer_free(WIRComposedEmitter* emitter);

/**
 * Generate code from WIR using a composed emitter.
 * @param emitter Composed emitter
 * @param root WIR root
 * @return Generated code string (caller must free), or NULL on error
 */
char* wir_composer_emit(WIRComposedEmitter* emitter, WIRRoot* root);

/**
 * Lookup a language emitter by name.
 * @param name Language name
 * @return Language emitter, or NULL if not found
 */
WIRLanguageEmitter* wir_composer_lookup_language(const char* name);

/**
 * Lookup a toolkit emitter by name.
 * @param name Toolkit name
 * @return Toolkit emitter, or NULL if not found
 */
WIRToolkitEmitter* wir_composer_lookup_toolkit(const char* name);

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Generate code from WIR with auto-composed emitter.
 * @param language Language name (e.g., "tcl")
 * @param toolkit Toolkit name (e.g., "tk")
 * @param root WIR root
 * @param options Emitter options (can be NULL for defaults)
 * @return Generated code string (caller must free), or NULL on error
 */
char* wir_compose_and_emit(const char* language, const char* toolkit,
                           WIRRoot* root, const WIRComposerOptions* options);

#ifdef __cplusplus
}
#endif

#endif // WIR_COMPOSER_H
