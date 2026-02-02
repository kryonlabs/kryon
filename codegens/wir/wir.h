/**
 * @file wir.h
 * @brief WIR - Toolkit Intermediate Representation
 *
 * WIR is an intermediate representation between KIR and toolkit-based codegens.
 * It provides a unified representation for widget-based UI toolkits (Tk, Limbo, etc.)
 *
 * Pipeline: KRY → KIR → WIR → [tcltk | limbo | c | ...]
 *
 * @copyright Kryon UI Framework
 */

#ifndef WIR_H
#define WIR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// cJSON forward declaration
typedef struct cJSON cJSON;

// ============================================================================
// WIR Format Constants
// ============================================================================

#define WIR_FORMAT_STRING "wir"
#define WIR_GENERATOR_NAME "kryon-wir-generator"

// ============================================================================
// WIR Layout Types
// ============================================================================

/**
 * Layout type enumeration for WIR widgets.
 * All layout types are pre-computed during KIR → WIR transformation.
 */
typedef enum {
    WIR_LAYOUT_PACK,      /**< Pack layout (default for Row/Column) */
    WIR_LAYOUT_GRID,      /**< Grid layout (explicit row/column) */
    WIR_LAYOUT_PLACE,     /**< Place layout (absolute positioning) */
    WIR_LAYOUT_AUTO       /**< Automatic layout detection */
} WIRLayoutType;

/**
 * Convert WIRLayoutType to string.
 */
const char* wir_layout_type_to_string(WIRLayoutType type);

/**
 * Convert string to WIRLayoutType.
 */
WIRLayoutType wir_layout_type_from_string(const char* str);

// ============================================================================
// WIR Property Types (Normalized)
// ============================================================================

/**
 * Normalized size value with unit.
 * All sizes in WIR are normalized to this structure.
 */
typedef struct {
    double value;      /**< Numeric value */
    const char* unit;  /**< Unit: "px", "%", "em", etc. */
} WIRSize;

/**
 * Border property structure.
 */
typedef struct {
    int width;              /**< Border width in pixels */
    const char* color;      /**< Border color (hex string) */
    const char* style;      /**< Border style: "solid", "dashed", etc. */
} WIRBorder;

/**
 * Font property structure.
 */
typedef struct {
    const char* family;     /**< Font family: "Arial", "Helvetica", etc. */
    double size;            /**< Font size */
    const char* size_unit;  /**< Size unit: "px", "pt", etc. */
    const char* weight;     /**< Font weight: "normal", "bold", etc. */
    const char* style;      /**< Font style: "normal", "italic", etc. */
} WIRFont;

/**
 * Alignment enumeration.
 */
typedef enum {
    WIR_ALIGN_START,
    WIR_ALIGN_CENTER,
    WIR_ALIGN_END,
    WIR_ALIGN_STRETCH,
    WIR_ALIGN_SPACE_BETWEEN,
    WIR_ALIGN_SPACE_AROUND,
    WIR_ALIGN_SPACE_EVENLY
} WIRAlignment;

const char* wir_alignment_to_string(WIRAlignment align);

// ============================================================================
// WIR Layout Options
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
} WIRPackOptions;

/**
 * Grid layout options.
 */
typedef struct {
    int row;                /**< Row number (0-indexed) */
    int column;             /**< Column number (0-indexed) */
    int rowspan;            /**< Row span */
    int columnspan;         /**< Column span */
    const char* sticky;     /**< Sticky positioning: "n", "s", "e", "w", combinations */
} WIRGridOptions;

/**
 * Place layout options (absolute positioning).
 */
typedef struct {
    int x;                  /**< X position */
    int y;                  /**< Y position */
    int width;              /**< Width */
    int height;             /**< Height */
    const char* anchor;     /**< Anchor point */
} WIRPlaceOptions;

/**
 * Union of all layout options.
 */
typedef struct {
    WIRLayoutType type;
    union {
        WIRPackOptions pack;
        WIRGridOptions grid;
        WIRPlaceOptions place;
    };
} WIRLayoutOptions;

// ============================================================================
// WIR Widget Structure
// ============================================================================

/**
 * WIR Widget structure.
 * Represents a single widget in the WIR tree.
 *
 * All fields are borrowed references to the underlying cJSON object
 * except where noted. Do not free individual fields.
 */
struct WIRWidget {
    const char* id;             /**< Widget ID (unique identifier) */
    const char* widget_type;    /**< Widget type: "button", "label", etc. */
    const char* kir_type;       /**< Original KIR component type */

    // Properties (all optional, NULL if not present)
    const char* text;           /**< Text content */
    WIRSize* width;            /**< Width (NULL if not specified) */
    WIRSize* height;           /**< Height */
    const char* background;     /**< Background color (resolved, inherited) */
    const char* foreground;     /**< Foreground color */
    WIRFont* font;             /**< Font properties */
    WIRBorder* border;         /**< Border properties */
    const char* image;          /**< Image path */

    // Layout
    WIRLayoutOptions* layout;  /**< Layout options (NULL for root) */
    const char* parent_id;      /**< Parent widget ID (NULL for root) */

    // Children
    cJSON* children_ids;        /**< Array of child widget IDs */

    // Events
    cJSON* events;              /**< Array of event handlers */

    // Original JSON (borrowed reference, do not free)
    cJSON* json;                /**< Original KIR JSON for this widget */
};
typedef struct WIRWidget WIRWidget;

/**
 * Create a WIRWidget from a KIR component JSON object.
 * The returned widget and all its fields are owned by the cJSON object.
 *
 * @param component KIR component JSON object
 * @param parent_id Parent widget ID (NULL for root)
 * @param id_counter Pointer to ID counter (incremented on call)
 * @return Newly allocated WIRWidget, or NULL on error
 */
WIRWidget* wir_widget_create(cJSON* component, const char* parent_id, int* id_counter);

/**
 * Free a WIRWidget.
 * Note: This does not free the underlying cJSON object.
 *
 * @param widget Widget to free
 */
void wir_widget_free(WIRWidget* widget);

// ============================================================================
// WIR Handler Structure
// ============================================================================

/**
 * WIR Event Handler structure.
 */
struct WIRHandler {
    const char* id;             /**< Handler ID (unique) */
    const char* event_type;     /**< Event type: "click", "change", etc. */
    const char* widget_id;      /**< Associated widget ID */

    // Multi-language implementations
    cJSON* implementations;     /**< Object mapping language to code */

    // Original JSON (borrowed reference)
    cJSON* json;
};
typedef struct WIRHandler WIRHandler;

/**
 * Create a WIRHandler from JSON.
 */
WIRHandler* wir_handler_create(cJSON* json, const char* widget_id, int* id_counter);

/**
 * Free a WIRHandler.
 */
void wir_handler_free(WIRHandler* handler);

// ============================================================================
// WIR Root Structure
// ============================================================================

/**
 * WIR Root structure.
 * Represents the complete WIR document.
 */
struct WIRRoot {
    // Metadata
    const char* format;         /**< Always "wir" */
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
    cJSON* widgets;             /**< Array of WIRWidget objects (by ID) */

    // Handlers
    cJSON* handlers;            /**< Array of WIRHandler objects */

    // Data bindings
    cJSON* data_bindings;       /**< Array of data binding definitions */

    // Original JSON root (borrowed reference)
    cJSON* json;
};
typedef struct WIRRoot WIRRoot;

/**
 * Create a WIRRoot from KIR JSON.
 *
 * @param kir_json KIR JSON string or parsed cJSON object
 * @return Newly allocated WIRRoot, or NULL on error
 */
WIRRoot* wir_root_create(const char* kir_json);

/**
 * Create a WIRRoot from parsed cJSON.
 *
 * @param kir_root Parsed KIR JSON root object
 * @return Newly allocated WIRRoot, or NULL on error
 */
WIRRoot* wir_root_from_cJSON(cJSON* kir_root);

/**
 * Free a WIRRoot.
 *
 * @param root WIRRoot to free
 */
void wir_root_free(WIRRoot* root);

/**
 * Convert WIRRoot to JSON string.
 * Caller must free the returned string.
 *
 * @param root WIRRoot to convert
 * @return JSON string, or NULL on error
 */
char* wir_root_to_json(WIRRoot* root);

/**
 * Find a widget by ID in WIRRoot.
 *
 * @param root WIRRoot to search
 * @param id Widget ID to find
 * @return WIRWidget if found, NULL otherwise
 */
WIRWidget* wir_root_find_widget(WIRRoot* root, const char* id);

/**
 * Find a handler by ID in WIRRoot.
 *
 * @param root WIRRoot to search
 * @param id Handler ID to find
 * @return WIRHandler if found, NULL otherwise
 */
WIRHandler* wir_root_find_handler(WIRRoot* root, const char* id);

// ============================================================================
// WIR Error Handling
// ============================================================================

/**
 * Set the error prefix for WIR error messages.
 */
void wir_set_error_prefix(const char* prefix);

/**
 * Get the last error message.
 * Returns a pointer to a static buffer.
 */
const char* wir_get_error(void);

/**
 * Clear the last error message.
 */
void wir_clear_error(void);

/**
 * Report a WIR error with consistent formatting.
 */
void wir_error(const char* fmt, ...);

/**
 * Report a WIR warning with consistent formatting.
 */
void wir_warn(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // WIR_H
