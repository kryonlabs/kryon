#ifndef REACT_COMMON_H
#define REACT_COMMON_H

#include "../ir/ir_core.h"
#include "../ir/third_party/cJSON/cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * React Code Generation Common Utilities
 * Shared logic for generating both TypeScript (.tsx) and JavaScript (.jsx)
 */

// =============================================================================
// Types
// =============================================================================

typedef enum {
    REACT_MODE_TYPESCRIPT,  // Generate TypeScript with types (.tsx)
    REACT_MODE_JAVASCRIPT   // Generate JavaScript without types (.jsx)
} ReactOutputMode;

typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
} StringBuilder;

typedef struct {
    ReactOutputMode mode;
    cJSON* logic_functions;      // From KIR logic.functions
    cJSON* event_bindings;       // From KIR logic.event_bindings
    int indent_level;
} ReactContext;

// =============================================================================
// String Builder Utilities
// =============================================================================

/**
 * Create a new string builder
 */
StringBuilder* sb_create(size_t initial_capacity);

/**
 * Append string to builder
 */
bool sb_append(StringBuilder* sb, const char* str);

/**
 * Append formatted string to builder
 */
bool sb_append_fmt(StringBuilder* sb, const char* fmt, ...);

/**
 * Get the final string (caller must NOT free - owned by StringBuilder)
 */
const char* sb_get(StringBuilder* sb);

/**
 * Free string builder
 */
void sb_free(StringBuilder* sb);

// =============================================================================
// Mode-Aware Helper Functions
// =============================================================================

/**
 * Get type annotation for TypeScript, empty for JavaScript
 */
const char* react_type_annotation(ReactOutputMode mode, const char* typ);

/**
 * Check if interfaces should be generated (TypeScript only)
 */
bool react_should_generate_interface(ReactOutputMode mode);

/**
 * Get file extension based on mode (.tsx or .jsx)
 */
const char* react_file_extension(ReactOutputMode mode);

/**
 * Generate React imports
 */
const char* react_generate_imports(ReactOutputMode mode);

// =============================================================================
// Property Mapping
// =============================================================================

/**
 * Map KIR property names to React property names
 */
const char* react_map_kir_property(const char* key);

/**
 * Format a JSON value as React property value
 * Returns allocated string - caller must free
 */
char* react_format_value(const char* key, cJSON* val);

/**
 * Check if a value is a default that should be omitted
 */
bool react_is_default_value(const char* key, cJSON* val);

// =============================================================================
// Expression Generation
// =============================================================================

/**
 * Convert a universal expression AST to React/JavaScript code
 * Returns allocated string - caller must free
 */
char* react_generate_expression(cJSON* expr);

/**
 * Generate the body of a click handler
 * Returns allocated string - caller must free
 */
char* react_generate_handler_body(cJSON* functions, const char* handler_id);

// =============================================================================
// Element Generation
// =============================================================================

/**
 * Generate React properties for a component
 * Returns StringBuilder with properties - caller must free with sb_free
 */
StringBuilder* react_generate_props(cJSON* node, ReactContext* ctx, bool has_text_expression);

/**
 * Generate a single React/JSX element
 * Returns allocated string - caller must free
 */
char* react_generate_element(cJSON* node, ReactContext* ctx, int indent);

// =============================================================================
// Window Configuration
// =============================================================================

typedef struct {
    int width;
    int height;
    char* title;
    char* background;
} WindowConfig;

/**
 * Extract window configuration from root component
 * Returns WindowConfig - caller must free title and background
 */
WindowConfig react_extract_window_config(cJSON* component);

/**
 * Free window config
 */
void react_free_window_config(WindowConfig* config);

// =============================================================================
// State Hooks Generation
// =============================================================================

/**
 * Generate useState hooks from reactive manifest
 * Returns allocated string - caller must free
 */
char* react_generate_state_hooks(cJSON* manifest, ReactContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // REACT_COMMON_H
