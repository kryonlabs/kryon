/**
 * Kryon Codegen - Common Utilities
 *
 * Shared utilities for all code generators to reduce code duplication.
 * Provides file I/O, JSON parsing helpers, and common property parsing.
 */

#ifndef CODEGEN_COMMON_H
#define CODEGEN_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// cJSON forward declaration
typedef struct cJSON cJSON;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// File I/O Utilities
// ============================================================================

/**
 * Read a KIR file into a null-terminated string.
 * The caller is responsible for freeing the returned pointer.
 *
 * @param path Path to the KIR file
 * @param out_size Optional pointer to receive the file size
 * @return Null-terminated string containing the file contents, or NULL on error
 */
char* codegen_read_kir_file(const char* path, size_t* out_size);

/**
 * Write content to an output file.
 *
 * @param path Path to the output file
 * @param content Content to write
 * @return true on success, false on failure
 */
bool codegen_write_output_file(const char* path, const char* content);

// ============================================================================
// JSON Parsing Utilities
// ============================================================================

/**
 * Parse KIR JSON string into a cJSON object.
 * The caller is responsible for freeing the returned cJSON object with cJSON_Delete().
 *
 * @param kir_json Null-terminated JSON string
 * @return cJSON root object, or NULL on parse error
 */
cJSON* codegen_parse_kir_json(const char* kir_json);

/**
 * Extract the root component from parsed KIR JSON.
 * Returns a borrowed reference - do not free.
 *
 * @param root Root cJSON object
 * @return Root component cJSON object, or NULL if not found
 */
cJSON* codegen_extract_root_component(cJSON* root);

/**
 * Extract the logic_block from parsed KIR JSON.
 * Returns a borrowed reference - do not free.
 *
 * @param root Root cJSON object
 * @return Logic block cJSON object, or NULL if not found
 */
cJSON* codegen_extract_logic_block(cJSON* root);

/**
 * Extract the component type from a component JSON object.
 * Returns a borrowed reference - do not free.
 *
 * @param component Component cJSON object
 * @return Type string (e.g., "Column", "Row"), or NULL if not found
 */
const char* codegen_get_component_type(cJSON* component);

/**
 * Get a property value from a component JSON object.
 * Returns a borrowed reference - do not free.
 *
 * @param component Component cJSON object
 * @param key Property name
 * @return Property value cJSON object, or NULL if not found
 */
cJSON* codegen_get_property(cJSON* component, const char* key);

// ============================================================================
// Property Parsing Utilities
// ============================================================================

/**
 * Parse a pixel value from a string like "200.0px" or "200.0".
 *
 * @param str String containing the value (may include "px" suffix)
 * @return Parsed float value, or 0.0f if str is NULL
 */
float codegen_parse_px_value(const char* str);

/**
 * Parse a percentage value from a string like "50%".
 *
 * @param str String containing the percentage
 * @param out_value Output pointer for the parsed value
 * @return true if string contained a percentage, false otherwise
 */
bool codegen_parse_percentage(const char* str, float* out_value);

/**
 * Check if a color string represents a transparent color.
 * Recognizes "#00000000" and "transparent" as transparent.
 *
 * @param color Color string (hex format or named)
 * @return true if color is transparent, false otherwise
 */
bool codegen_is_transparent_color(const char* color);

/**
 * Parse a hex color string to RGBA components.
 * Supports formats: "#RRGGBB", "#RRGGBBAA", "0xRRGGBBAA".
 *
 * @param color Color string
 * @param out_r Output for red component (0-255)
 * @param out_g Output for green component (0-255)
 * @param out_b Output for blue component (0-255)
 * @param out_a Output for alpha component (0-255, defaults to 255 if not specified)
 * @return true on success, false on parse error
 */
bool codegen_parse_color_rgba(const char* color, uint8_t* out_r, uint8_t* out_g,
                              uint8_t* out_b, uint8_t* out_a);

/**
 * Format RGBA components to a hex color string.
 * The returned string is statically allocated and must not be freed.
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 * @param include_alpha Whether to include alpha in output (#RRGGBBAA vs #RRGGBB)
 * @return Static buffer containing hex color string
 */
const char* codegen_format_rgba_hex(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                     bool include_alpha);

// ============================================================================
// Error Reporting Utilities
// ============================================================================

/**
 * Set the error prefix for codegen error messages.
 * This is used to identify which codegen is reporting an error.
 *
 * @param prefix Error prefix (e.g., "Lua", "Kotlin")
 */
void codegen_set_error_prefix(const char* prefix);

/**
 * Report a codegen error with consistent formatting.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
void codegen_error(const char* fmt, ...);

/**
 * Report a codegen warning with consistent formatting.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
void codegen_warn(const char* fmt, ...);

// ============================================================================
// StringBuilder Utility
// ============================================================================

typedef struct StringBuilder StringBuilder;

/**
 * Create a new string builder with initial capacity.
 */
StringBuilder* sb_create(size_t initial_capacity);

/**
 * Append a string to the builder.
 */
void sb_append(StringBuilder* sb, const char* str);

/**
 * Append a formatted string to the builder.
 */
void sb_append_fmt(StringBuilder* sb, const char* fmt, ...);

/**
 * Get the built string (caller must free).
 */
char* sb_get(StringBuilder* sb);

/**
 * Free the string builder.
 */
void sb_free(StringBuilder* sb);

// ============================================================================
// TSX/React Helper Types and Functions
// ============================================================================

#define REACT_MODE_TYPESCRIPT 1

typedef struct {
    int width;
    int height;
    char* title;
    char* background;
} WindowConfig;

typedef struct ReactContext {
    int mode;
    cJSON* logic_functions;
    cJSON* event_bindings;
    int indent_level;
} ReactContext;

/**
 * Extract window configuration from KIR JSON.
 * Returns a WindowConfig struct that must be freed with react_free_window_config().
 */
WindowConfig react_extract_window_config(cJSON* root);

/**
 * Free window configuration resources.
 */
void react_free_window_config(WindowConfig* config);

/**
 * Generate React import statements.
 */
char* react_generate_imports(int mode);

/**
 * Generate React state hooks from reactive manifest.
 */
char* react_generate_state_hooks(cJSON* manifest, ReactContext* ctx);

/**
 * Generate a React element from component JSON.
 */
char* react_generate_element(cJSON* component, ReactContext* ctx, int indent);

// ============================================================================
// Tk-Based Codegen Utilities
// ============================================================================
// Shared utilities for all Tk-based codegens (Limbo, Tcl/Tk, etc.)

/**
 * Map KIR element type to Tk widget type.
 * Shared by all Tk-based targets.
 *
 * @param element_type KIR element type (e.g., "Button", "Text", "Container")
 * @return Tk widget type string (e.g., "button", "label", "frame")
 */
const char* codegen_map_kir_to_tk_widget(const char* element_type);

/**
 * Extract common widget properties from KIR component.
 * Returns pointers to borrowed strings (do not free).
 *
 * @param component KIR component JSON object
 * @param out_text Optional output for text content
 * @param out_width Optional output for width
 * @param out_height Optional output for height
 * @param out_background Optional output for background color
 * @param out_foreground Optional output for foreground color
 * @param out_font Optional output for font
 */
void codegen_extract_widget_props(cJSON* component,
                                  const char** out_text,
                                  const char** out_width,
                                  const char** out_height,
                                  const char** out_background,
                                  const char** out_foreground,
                                  const char** out_font);

/**
 * Parse size value from string (handles "200px", "100%", etc.).
 *
 * @param value Size value string
 * @return Parsed integer value, or 0 if invalid
 */
int codegen_parse_size_value(const char* value);

/**
 * Check if size value is a percentage.
 *
 * @param value Size value string
 * @return true if string contains "%", false otherwise
 */
bool codegen_is_percentage_value(const char* value);

/**
 * Check if component is a layout container.
 *
 * @param type Component type string
 * @return true if container type, false otherwise
 */
bool codegen_is_layout_container(const char* type);

/**
 * Layout type enumeration
 */
typedef enum {
    CODEGEN_LAYOUT_PACK,      // Pack layout (default)
    CODEGEN_LAYOUT_GRID,      // Grid layout
    CODEGEN_LAYOUT_PLACE,     // Place layout (absolute positioning)
    CODEGEN_LAYOUT_AUTO       // Automatic (based on properties)
} CodegenLayoutType;

/**
 * Determine layout type from component properties.
 * Checks for "row"/"column" (grid) or "left"/"top" (place) properties.
 *
 * @param component KIR component JSON object
 * @return Detected layout type
 */
CodegenLayoutType codegen_detect_layout_type(cJSON* component);

/**
 * Layout options structure
 * Used to build layout command options
 */
typedef struct {
    const char* parent_type;
    bool has_explicit_size;
    bool has_position;
    int left;
    int top;
} CodegenLayoutOptions;

/**
 * Extract layout options from component.
 *
 * @param component KIR component JSON object
 * @param parent_type Parent component type
 * @param out_options Output for layout options
 */
void codegen_extract_layout_options(cJSON* component,
                                    const char* parent_type,
                                    CodegenLayoutOptions* out_options);

/**
 * Handler tracker for deduplication.
 * Tracks which event handlers have already been generated.
 */
typedef struct {
    char* handlers[256];
    int count;
} CodegenHandlerTracker;

/**
 * Create a new handler tracker.
 *
 * @return Newly allocated tracker, or NULL on error
 */
CodegenHandlerTracker* codegen_handler_tracker_create(void);

/**
 * Check if handler was already generated.
 *
 * @param tracker Handler tracker
 * @param handler_name Handler name to check
 * @return true if already generated, false otherwise
 */
bool codegen_handler_tracker_contains(CodegenHandlerTracker* tracker,
                                      const char* handler_name);

/**
 * Mark handler as generated.
 *
 * @param tracker Handler tracker
 * @param handler_name Handler name to mark
 * @return true on success, false if tracker is full
 */
bool codegen_handler_tracker_mark(CodegenHandlerTracker* tracker,
                                  const char* handler_name);

/**
 * Free handler tracker.
 *
 * @param tracker Handler tracker to free
 */
void codegen_handler_tracker_free(CodegenHandlerTracker* tracker);

/**
 * Free all tracked handlers in a tracker.
 *
 * @param tracker Handler tracker
 */
void codegen_handler_tracker_clear(CodegenHandlerTracker* tracker);

// ============================================================================
// Multi-File Codegen Utilities
// ============================================================================

#define CODEGEN_MAX_PROCESSED_MODULES 256

/**
 * Tracks which modules have been processed during multi-file codegen
 * to prevent duplicate processing and circular dependencies.
 */
typedef struct {
    char* modules[CODEGEN_MAX_PROCESSED_MODULES];
    int count;
} CodegenProcessedModules;

/**
 * Check if a module has already been processed.
 *
 * @param pm ProcessedModules tracker
 * @param module_id Module identifier
 * @return true if already processed, false otherwise
 */
bool codegen_processed_modules_contains(CodegenProcessedModules* pm, const char* module_id);

/**
 * Mark a module as processed.
 *
 * @param pm ProcessedModules tracker
 * @param module_id Module identifier to mark as processed
 */
void codegen_processed_modules_add(CodegenProcessedModules* pm, const char* module_id);

/**
 * Free all memory associated with the processed modules tracker.
 *
 * @param pm ProcessedModules tracker to free
 */
void codegen_processed_modules_free(CodegenProcessedModules* pm);

/**
 * Check if a module is a Kryon internal module (not user code).
 * Internal modules include kryon/, dsl, ffi, runtime, reactive, etc.
 *
 * @param module_id Module identifier
 * @return true if internal module, false otherwise
 */
bool codegen_is_internal_module(const char* module_id);

/**
 * Load the plugin manifest from build directory.
 * Call this early in the codegen pipeline before processing imports.
 * Safe to call multiple times - subsequent calls are no-ops.
 *
 * @param build_dir Build directory path (uses "build" if NULL)
 */
void codegen_load_plugin_manifest(const char* build_dir);

/**
 * Check if a module is an external plugin (runtime dependency, not source).
 * Uses the plugin manifest loaded by codegen_load_plugin_manifest().
 * Falls back to known plugins if manifest not loaded.
 *
 * @param module_id Module identifier
 * @return true if external plugin, false otherwise
 */
bool codegen_is_external_plugin(const char* module_id);

/**
 * Get the parent directory of a file path.
 *
 * @param path Input file path
 * @param parent Output buffer for parent directory
 * @param parent_size Size of output buffer
 */
void codegen_get_parent_dir(const char* path, char* parent, size_t parent_size);

/**
 * Write content to a file, creating parent directories as needed.
 *
 * @param path Output file path
 * @param content Content to write
 * @return true on success, false on failure
 */
bool codegen_write_file_with_mkdir(const char* path, const char* content);

/**
 * Create a directory and all parent directories as needed.
 *
 * @param path Directory path to create
 * @return true on success, false on failure
 */
bool codegen_mkdir_p(const char* path);

/**
 * Check if a file exists.
 *
 * @param path File path to check
 * @return true if file exists, false otherwise
 */
bool codegen_file_exists(const char* path);

/**
 * Copy a file from src to dst.
 *
 * @param src Source file path
 * @param dst Destination file path
 * @return 0 on success, -1 on failure
 */
int codegen_copy_file(const char* src, const char* dst);

#ifdef __cplusplus
}
#endif

#endif // CODEGEN_COMMON_H
