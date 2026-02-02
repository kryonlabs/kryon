/**
 * @file wir_builder.h
 * @brief WIR Builder - KIR to WIR Transformation
 *
 * The WIR Builder transforms KIR (Kryon Intermediate Representation)
 * into WIR (Toolkit Intermediate Representation).
 *
 * Responsibilities:
 * - Widget type mapping (KIR → Tk widgets)
 * - Property normalization (sizes, colors, fonts, borders)
 * - Layout resolution (pack/grid/place detection and option computation)
 * - Background inheritance propagation
 * - Event handler extraction and deduplication
 * - Data binding extraction
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#ifndef WIR_BUILDER_H
#define WIR_BUILDER_H

#include "wir.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
// CodegenHandlerTracker is defined in codegen_common.h
// We use an incomplete type here to avoid circular includes
typedef struct CodegenHandlerTracker CodegenHandlerTracker;

// ============================================================================
// Builder Context
// ============================================================================

/**
 * Context for building WIR from KIR.
 * Tracks state during the transformation process.
 */
typedef struct {
    WIRRoot* root;                    /**< WIR root being built */
    int widget_id_counter;             /**< Counter for generating widget IDs */
    int handler_id_counter;            /**< Counter for generating handler IDs */
    const char* root_background;       /**< Root window background color for inheritance */
    CodegenHandlerTracker* handlers;   /**< Handler deduplication tracker */
    bool verbose;                      /**< Enable verbose output */
} WIRBuilderContext;

/**
 * Create a new WIR builder context.
 *
 * @param root WIRRoot to build (must be created first)
 * @return Newly allocated context, or NULL on error
 */
WIRBuilderContext* wir_builder_create(WIRRoot* root);

/**
 * Free a WIR builder context.
 *
 * @param ctx Context to free
 */
void wir_builder_free(WIRBuilderContext* ctx);

// ============================================================================
// Main Build Function
// ============================================================================

/**
 * Build complete WIR from KIR JSON.
 * This is the main entry point for KIR → WIR transformation.
 *
 * @param kir_json KIR JSON string
 * @param verbose Enable verbose output for debugging
 * @return Newly allocated WIRRoot, or NULL on error
 */
WIRRoot* wir_build_from_kir(const char* kir_json, bool verbose);

/**
 * Build WIR from parsed KIR cJSON.
 *
 * OWNERSHIP TRANSFER: This function takes ownership of the kir_root parameter.
 * The caller MUST NOT free kir_root after calling this function.
 * The WIRRoot will take responsibility for freeing it when wir_root_free is called.
 *
 * If you need to retain ownership of kir_root, use wir_root_from_cJSON instead
 * and manage the JSON lifetime yourself.
 *
 * @param kir_root KIR JSON root object (ownership is transferred)
 * @param verbose Enable verbose output for debugging
 * @return WIRRoot (caller must free with wir_root_free), or NULL on error
 */
WIRRoot* wir_build_from_cJSON(cJSON* kir_root, bool verbose);

/**
 * Transform KIR root component to WIR widget tree.
 *
 * @param ctx Builder context
 * @param kir_root KIR JSON root object
 * @return true on success, false on error
 */
bool wir_builder_transform_root(WIRBuilderContext* ctx, cJSON* kir_root);

/**
 * Transform a single KIR component to WIR widget.
 *
 * @param ctx Builder context
 * @param component KIR component JSON object
 * @param parent_id Parent widget ID (NULL for root)
 * @param parent_bg Parent background color (for inheritance)
 * @param parent_component Parent KIR component JSON object (NULL for root)
 * @param child_index Index of this child in parent (0-based)
 * @param total_children Total number of children in parent
 * @return Newly allocated WIRWidget, or NULL on error
 */
WIRWidget* wir_builder_transform_component(WIRBuilderContext* ctx,
                                             cJSON* component,
                                             const char* parent_id,
                                             const char* parent_bg,
                                             cJSON* parent_component,
                                             int child_index,
                                             int total_children);

// ============================================================================
// Property Normalization
// ============================================================================

/**
 * Normalize a size value from KIR to WIRSize.
 * Handles "200px", "100%", numbers, etc.
 *
 * @param value Size value string or number
 * @return Newly allocated WIRSize, or NULL if not specified
 */
WIRSize* wir_normalize_size(cJSON* value);

/**
 * Normalize a font property from KIR to WIRFont.
 *
 * @param component KIR component JSON object
 * @return Newly allocated WIRFont, or NULL if not specified
 */
WIRFont* wir_normalize_font(cJSON* component);

/**
 * Normalize a border property from KIR to WIRBorder.
 *
 * @param component KIR component JSON object
 * @return Newly allocated WIRBorder, or NULL if not specified
 */
WIRBorder* wir_normalize_border(cJSON* component);

/**
 * Resolve and propagate background color.
 * Handles inheritance from parent if not specified.
 *
 * @param component KIR component JSON object
 * @param parent_bg Parent background color (NULL for root)
 * @return Resolved background color string, or NULL
 */
const char* wir_resolve_background(cJSON* component, const char* parent_bg);

// ============================================================================
// Layout Resolution
// ============================================================================

/**
 * Detect and compute layout options for a widget.
 * Determines pack/grid/place and computes all options.
 *
 * @param ctx Builder context
 * @param widget WIRWidget being processed
 * @param component KIR component JSON object
 * @param parent_type Parent component type (for layout defaults)
 * @param child_index Index of this child in parent (for spacing)
 * @param total_children Total number of children in parent
 * @return Newly allocated WIRLayoutOptions, or NULL for root
 */
WIRLayoutOptions* wir_builder_compute_layout(WIRBuilderContext* ctx,
                                               WIRWidget* widget,
                                               cJSON* component,
                                               const char* parent_type,
                                               int child_index,
                                               int total_children);

/**
 * Compute pack layout options.
 *
 * @param component KIR component JSON object
 * @param parent_type Parent component type
 * @param child_index Index of this child
 * @param total_children Total number of children
 * @return Newly allocated WIRPackOptions
 */
WIRPackOptions* wir_compute_pack_options(cJSON* component,
                                           const char* parent_type,
                                           int child_index,
                                           int total_children);

/**
 * Compute grid layout options.
 *
 * @param component KIR component JSON object
 * @return Newly allocated WIRGridOptions
 */
WIRGridOptions* wir_compute_grid_options(cJSON* component);

/**
 * Compute place layout options (absolute positioning).
 *
 * @param component KIR component JSON object
 * @return Newly allocated WIRPlaceOptions
 */
WIRPlaceOptions* wir_compute_place_options(cJSON* component);

/**
 * Compute layout options with alignment support.
 *
 * @param ctx Builder context
 * @param widget WIRWidget being processed
 * @param component KIR component JSON object
 * @param parent_type Parent component type
 * @param main_align Main axis alignment (justifyContent)
 * @param cross_align Cross axis alignment (alignItems)
 * @param child_index Index of this child in parent
 * @param total_children Total number of children in parent
 * @return Newly allocated WIRLayoutOptions
 */
WIRLayoutOptions* wir_builder_compute_layout_with_alignment(
    WIRBuilderContext* ctx,
    WIRWidget* widget,
    cJSON* component,
    const char* parent_type,
    const char* main_align,
    const char* cross_align,
    int child_index,
    int total_children);

// ============================================================================
// Event Handler Extraction
// ============================================================================

/**
 * Extract and deduplicate event handlers from a component.
 *
 * @param ctx Builder context
 * @param widget WIRWidget being processed
 * @param component KIR component JSON object
 * @return true on success, false on error
 */
bool wir_builder_extract_handlers(WIRBuilderContext* ctx,
                                    WIRWidget* widget,
                                    cJSON* component);

/**
 * Generate handler implementation for a specific language.
 * This is a placeholder - actual implementations will be in emitter code.
 *
 * @param handler_json Handler JSON from KIR
 * @param language Target language ("tcl", "limbo", etc.)
 * @return Generated code string, or NULL on error
 */
char* wir_generate_handler_code(cJSON* handler_json, const char* language);

// ============================================================================
// Data Binding Extraction
// ============================================================================

/**
 * Extract data bindings from KIR logic block.
 *
 * @param ctx Builder context
 * @param logic_block KIR logic block JSON object
 * @return true on success, false on error
 */
bool wir_builder_extract_data_bindings(WIRBuilderContext* ctx, cJSON* logic_block);

// ============================================================================
// Utilities
// ============================================================================

/**
 * Serialize a WIRWidget to JSON.
 * Returns a cJSON object that should be added to the widgets array.
 *
 * @param widget WIRWidget to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* wir_widget_to_json(WIRWidget* widget);

/**
 * Serialize a WIRHandler to JSON.
 *
 * @param handler WIRHandler to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* wir_handler_to_json(WIRHandler* handler);

/**
 * Serialize WIRSize to JSON.
 *
 * @param size WIRSize to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* wir_size_to_json(WIRSize* size);

/**
 * Serialize WIRFont to JSON.
 *
 * @param font WIRFont to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* wir_font_to_json(WIRFont* font);

/**
 * Serialize WIRBorder to JSON.
 *
 * @param border WIRBorder to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* wir_border_to_json(WIRBorder* border);

/**
 * Serialize WIRLayoutOptions to JSON.
 *
 * @param layout WIRLayoutOptions to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* wir_layout_to_json(WIRLayoutOptions* layout);

#ifdef __cplusplus
}
#endif

#endif // WIR_BUILDER_H
