/**
 * @file tkir.h
 * @brief TKIR - Toolkit Intermediate Representation
 *
 * TKIR is an intermediate representation between KIR and toolkit-based codegens.
 * It provides a unified representation for widget-based UI toolkits (Tk, Limbo, etc.)
 *
 * Pipeline: KRY → KIR → TKIR → [tcltk | limbo | c | ...]
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#ifndef TKIR_H
#define TKIR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// cJSON forward declaration
typedef struct cJSON cJSON;

// ============================================================================
// TKIR Version
// ============================================================================

#define TKIR_VERSION_MAJOR 1
#define TKIR_VERSION_MINOR 0
#define TKIR_VERSION_PATCH 0

// ============================================================================
// TKIR Format Constants
// ============================================================================

#define TKIR_FORMAT_STRING "tkir"
#define TKIR_GENERATOR_NAME "kryon-tkir-generator"

// ============================================================================
// TKIR Layout Types
// ============================================================================

/**
 * Layout type enumeration for TKIR widgets.
 * All layout types are pre-computed during KIR → TKIR transformation.
 */
typedef enum {
    TKIR_LAYOUT_PACK,      /**< Pack layout (default for Row/Column) */
    TKIR_LAYOUT_GRID,      /**< Grid layout (explicit row/column) */
    TKIR_LAYOUT_PLACE,     /**< Place layout (absolute positioning) */
    TKIR_LAYOUT_AUTO       /**< Automatic layout detection */
} TKIRLayoutType;

/**
 * Convert TKIRLayoutType to string.
 */
const char* tkir_layout_type_to_string(TKIRLayoutType type);

/**
 * Convert string to TKIRLayoutType.
 */
TKIRLayoutType tkir_layout_type_from_string(const char* str);

// ============================================================================
// TKIR Property Types (Normalized)
// ============================================================================

/**
 * Normalized size value with unit.
 * All sizes in TKIR are normalized to this structure.
 */
typedef struct {
    double value;      /**< Numeric value */
    const char* unit;  /**< Unit: "px", "%", "em", etc. */
} TKIRSize;

/**
 * Border property structure.
 */
typedef struct {
    int width;              /**< Border width in pixels */
    const char* color;      /**< Border color (hex string) */
    const char* style;      /**< Border style: "solid", "dashed", etc. */
} TKIRBorder;

/**
 * Font property structure.
 */
typedef struct {
    const char* family;     /**< Font family: "Arial", "Helvetica", etc. */
    double size;            /**< Font size */
    const char* size_unit;  /**< Size unit: "px", "pt", etc. */
    const char* weight;     /**< Font weight: "normal", "bold", etc. */
    const char* style;      /**< Font style: "normal", "italic", etc. */
} TKIRFont;

/**
 * Alignment enumeration.
 */
typedef enum {
    TKIR_ALIGN_START,
    TKIR_ALIGN_CENTER,
    TKIR_ALIGN_END,
    TKIR_ALIGN_STRETCH,
    TKIR_ALIGN_SPACE_BETWEEN,
    TKIR_ALIGN_SPACE_AROUND,
    TKIR_ALIGN_SPACE_EVENLY
} TKIRAlignment;

const char* tkir_alignment_to_string(TKIRAlignment align);

// ============================================================================
// TKIR Layout Options
// ============================================================================

/**
 * Pack layout options.
 */
typedef struct {
    const char* side;       /**< "top", "bottom", "left", "right" */
    const char* fill;       /**< "none", "x", "y", "both" */
    bool expand;            /**< Expand to fill space */
    const char* anchor;     /**< "n", "s", "e", "w", "center", etc. */
    int padx;               /**< Horizontal padding */
    int pady;               /**< Vertical padding */
} TKIRPackOptions;

/**
 * Grid layout options.
 */
typedef struct {
    int row;                /**< Row number (0-indexed) */
    int column;             /**< Column number (0-indexed) */
    int rowspan;            /**< Row span */
    int columnspan;         /**< Column span */
    const char* sticky;     /**< Sticky positioning: "n", "s", "e", "w", combinations */
} TKIRGridOptions;

/**
 * Place layout options (absolute positioning).
 */
typedef struct {
    int x;                  /**< X position */
    int y;                  /**< Y position */
    int width;              /**< Width */
    int height;             /**< Height */
    const char* anchor;     /**< Anchor point */
} TKIRPlaceOptions;

/**
 * Union of all layout options.
 */
typedef struct {
    TKIRLayoutType type;
    union {
        TKIRPackOptions pack;
        TKIRGridOptions grid;
        TKIRPlaceOptions place;
    };
} TKIRLayoutOptions;

// ============================================================================
// TKIR Widget Structure
// ============================================================================

/**
 * TKIR Widget structure.
 * Represents a single widget in the TKIR tree.
 *
 * All fields are borrowed references to the underlying cJSON object
 * except where noted. Do not free individual fields.
 */
typedef struct {
    const char* id;             /**< Widget ID (unique identifier) */
    const char* tk_type;        /**< Tk widget type: "button", "label", etc. */
    const char* kir_type;       /**< Original KIR component type */

    // Properties (all optional, NULL if not present)
    const char* text;           /**< Text content */
    TKIRSize* width;            /**< Width (NULL if not specified) */
    TKIRSize* height;           /**< Height */
    const char* background;     /**< Background color (resolved, inherited) */
    const char* foreground;     /**< Foreground color */
    TKIRFont* font;             /**< Font properties */
    TKIRBorder* border;         /**< Border properties */
    const char* image;          /**< Image path */

    // Layout
    TKIRLayoutOptions* layout;  /**< Layout options (NULL for root) */
    const char* parent_id;      /**< Parent widget ID (NULL for root) */

    // Children
    cJSON* children_ids;        /**< Array of child widget IDs */

    // Events
    cJSON* events;              /**< Array of event handlers */

    // Original JSON (borrowed reference, do not free)
    cJSON* json;                /**< Original KIR JSON for this widget */
} TKIRWidget;

/**
 * Create a TKIRWidget from a KIR component JSON object.
 * The returned widget and all its fields are owned by the cJSON object.
 *
 * @param component KIR component JSON object
 * @param parent_id Parent widget ID (NULL for root)
 * @param id_counter Pointer to ID counter (incremented on call)
 * @return Newly allocated TKIRWidget, or NULL on error
 */
TKIRWidget* tkir_widget_create(cJSON* component, const char* parent_id, int* id_counter);

/**
 * Free a TKIRWidget.
 * Note: This does not free the underlying cJSON object.
 *
 * @param widget Widget to free
 */
void tkir_widget_free(TKIRWidget* widget);

// ============================================================================
// TKIR Handler Structure
// ============================================================================

/**
 * TKIR Event Handler structure.
 */
typedef struct {
    const char* id;             /**< Handler ID (unique) */
    const char* event_type;     /**< Event type: "click", "change", etc. */
    const char* widget_id;      /**< Associated widget ID */

    // Multi-language implementations
    cJSON* implementations;     /**< Object mapping language to code */

    // Original JSON (borrowed reference)
    cJSON* json;
} TKIRHandler;

/**
 * Create a TKIRHandler from JSON.
 */
TKIRHandler* tkir_handler_create(cJSON* json, const char* widget_id, int* id_counter);

/**
 * Free a TKIRHandler.
 */
void tkir_handler_free(TKIRHandler* handler);

// ============================================================================
// TKIR Root Structure
// ============================================================================

/**
 * TKIR Root structure.
 * Represents the complete TKIR document.
 */
typedef struct {
    // Metadata
    const char* format;         /**< Always "tkir" */
    const char* version;        /**< Version string */
    const char* source_kir;     /**< Source KIR file path */
    const char* generator;      /**< Generator name */

    // Window configuration
    const char* title;          /**< Window title */
    int width;                  /**< Window width */
    int height;                 /**< Window height */
    bool resizable;             /**< Resizable flag */
    const char* background;     /**< Window background color */

    // Widgets
    cJSON* widgets;             /**< Array of TKIRWidget objects (by ID) */

    // Handlers
    cJSON* handlers;            /**< Array of TKIRHandler objects */

    // Data bindings
    cJSON* data_bindings;       /**< Array of data binding definitions */

    // Original JSON root (borrowed reference)
    cJSON* json;
} TKIRRoot;

/**
 * Create a TKIRRoot from KIR JSON.
 *
 * @param kir_json KIR JSON string or parsed cJSON object
 * @return Newly allocated TKIRRoot, or NULL on error
 */
TKIRRoot* tkir_root_create(const char* kir_json);

/**
 * Create a TKIRRoot from parsed cJSON.
 *
 * @param kir_root Parsed KIR JSON root object
 * @return Newly allocated TKIRRoot, or NULL on error
 */
TKIRRoot* tkir_root_from_cJSON(cJSON* kir_root);

/**
 * Free a TKIRRoot.
 *
 * @param root TKIRRoot to free
 */
void tkir_root_free(TKIRRoot* root);

/**
 * Convert TKIRRoot to JSON string.
 * Caller must free the returned string.
 *
 * @param root TKIRRoot to convert
 * @return JSON string, or NULL on error
 */
char* tkir_root_to_json(TKIRRoot* root);

/**
 * Find a widget by ID in TKIRRoot.
 *
 * @param root TKIRRoot to search
 * @param id Widget ID to find
 * @return TKIRWidget if found, NULL otherwise
 */
TKIRWidget* tkir_root_find_widget(TKIRRoot* root, const char* id);

/**
 * Find a handler by ID in TKIRRoot.
 *
 * @param root TKIRRoot to search
 * @param id Handler ID to find
 * @return TKIRHandler if found, NULL otherwise
 */
TKIRHandler* tkir_root_find_handler(TKIRRoot* root, const char* id);

// ============================================================================
// TKIR Error Handling
// ============================================================================

/**
 * Set the error prefix for TKIR error messages.
 */
void tkir_set_error_prefix(const char* prefix);

/**
 * Get the last error message.
 * Returns a pointer to a static buffer.
 */
const char* tkir_get_error(void);

/**
 * Clear the last error message.
 */
void tkir_clear_error(void);

/**
 * Report a TKIR error with consistent formatting.
 */
void tkir_error(const char* fmt, ...);

/**
 * Report a TKIR warning with consistent formatting.
 */
void tkir_warn(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // TKIR_H
