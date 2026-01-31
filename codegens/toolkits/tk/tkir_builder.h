/**
 * @file tkir_builder.h
 * @brief TKIR Builder - KIR to TKIR Transformation
 *
 * The TKIR Builder transforms KIR (Kryon Intermediate Representation)
 * into TKIR (Toolkit Intermediate Representation).
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
 * @version 1.0.0
 */

#ifndef TKIR_BUILDER_H
#define TKIR_BUILDER_H

#include "tkir.h"

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
 * Context for building TKIR from KIR.
 * Tracks state during the transformation process.
 */
typedef struct {
    TKIRRoot* root;                    /**< TKIR root being built */
    int widget_id_counter;             /**< Counter for generating widget IDs */
    int handler_id_counter;            /**< Counter for generating handler IDs */
    const char* root_background;       /**< Root window background color for inheritance */
    CodegenHandlerTracker* handlers;   /**< Handler deduplication tracker */
    bool verbose;                      /**< Enable verbose output */
} TKIRBuilderContext;

/**
 * Create a new TKIR builder context.
 *
 * @param root TKIRRoot to build (must be created first)
 * @return Newly allocated context, or NULL on error
 */
TKIRBuilderContext* tkir_builder_create(TKIRRoot* root);

/**
 * Free a TKIR builder context.
 *
 * @param ctx Context to free
 */
void tkir_builder_free(TKIRBuilderContext* ctx);

// ============================================================================
// Main Build Function
// ============================================================================

/**
 * Build complete TKIR from KIR JSON.
 * This is the main entry point for KIR → TKIR transformation.
 *
 * @param kir_json KIR JSON string
 * @param verbose Enable verbose output for debugging
 * @return Newly allocated TKIRRoot, or NULL on error
 */
TKIRRoot* tkir_build_from_kir(const char* kir_json, bool verbose);

/**
 * Build TKIR from parsed KIR cJSON.
 *
 * @param kir_root Parsed KIR JSON root
 * @param verbose Enable verbose output for debugging
 * @return Newly allocated TKIRRoot, or NULL on error
 */
TKIRRoot* tkir_build_from_cJSON(cJSON* kir_root, bool verbose);

/**
 * Transform KIR root component to TKIR widget tree.
 *
 * @param ctx Builder context
 * @param kir_root KIR JSON root object
 * @return true on success, false on error
 */
bool tkir_builder_transform_root(TKIRBuilderContext* ctx, cJSON* kir_root);

/**
 * Transform a single KIR component to TKIR widget.
 *
 * @param ctx Builder context
 * @param component KIR component JSON object
 * @param parent_id Parent widget ID (NULL for root)
 * @param parent_bg Parent background color (for inheritance)
 * @param parent_component Parent KIR component JSON object (NULL for root)
 * @param child_index Index of this child in parent (0-based)
 * @param total_children Total number of children in parent
 * @return Newly allocated TKIRWidget, or NULL on error
 */
TKIRWidget* tkir_builder_transform_component(TKIRBuilderContext* ctx,
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
 * Normalize a size value from KIR to TKIRSize.
 * Handles "200px", "100%", numbers, etc.
 *
 * @param value Size value string or number
 * @return Newly allocated TKIRSize, or NULL if not specified
 */
TKIRSize* tkir_normalize_size(cJSON* value);

/**
 * Normalize a font property from KIR to TKIRFont.
 *
 * @param component KIR component JSON object
 * @return Newly allocated TKIRFont, or NULL if not specified
 */
TKIRFont* tkir_normalize_font(cJSON* component);

/**
 * Normalize a border property from KIR to TKIRBorder.
 *
 * @param component KIR component JSON object
 * @return Newly allocated TKIRBorder, or NULL if not specified
 */
TKIRBorder* tkir_normalize_border(cJSON* component);

/**
 * Resolve and propagate background color.
 * Handles inheritance from parent if not specified.
 *
 * @param component KIR component JSON object
 * @param parent_bg Parent background color (NULL for root)
 * @return Resolved background color string, or NULL
 */
const char* tkir_resolve_background(cJSON* component, const char* parent_bg);

// ============================================================================
// Layout Resolution
// ============================================================================

/**
 * Detect and compute layout options for a widget.
 * Determines pack/grid/place and computes all options.
 *
 * @param ctx Builder context
 * @param widget TKIRWidget being processed
 * @param component KIR component JSON object
 * @param parent_type Parent component type (for layout defaults)
 * @param child_index Index of this child in parent (for spacing)
 * @param total_children Total number of children in parent
 * @return Newly allocated TKIRLayoutOptions, or NULL for root
 */
TKIRLayoutOptions* tkir_builder_compute_layout(TKIRBuilderContext* ctx,
                                               TKIRWidget* widget,
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
 * @return Newly allocated TKIRPackOptions
 */
TKIRPackOptions* tkir_compute_pack_options(cJSON* component,
                                           const char* parent_type,
                                           int child_index,
                                           int total_children);

/**
 * Compute grid layout options.
 *
 * @param component KIR component JSON object
 * @return Newly allocated TKIRGridOptions
 */
TKIRGridOptions* tkir_compute_grid_options(cJSON* component);

/**
 * Compute place layout options (absolute positioning).
 *
 * @param component KIR component JSON object
 * @return Newly allocated TKIRPlaceOptions
 */
TKIRPlaceOptions* tkir_compute_place_options(cJSON* component);

/**
 * Compute layout options with alignment support.
 *
 * @param ctx Builder context
 * @param widget TKIRWidget being processed
 * @param component KIR component JSON object
 * @param parent_type Parent component type
 * @param main_align Main axis alignment (justifyContent)
 * @param cross_align Cross axis alignment (alignItems)
 * @param child_index Index of this child in parent
 * @param total_children Total number of children in parent
 * @return Newly allocated TKIRLayoutOptions
 */
TKIRLayoutOptions* tkir_builder_compute_layout_with_alignment(
    TKIRBuilderContext* ctx,
    TKIRWidget* widget,
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
 * @param widget TKIRWidget being processed
 * @param component KIR component JSON object
 * @return true on success, false on error
 */
bool tkir_builder_extract_handlers(TKIRBuilderContext* ctx,
                                    TKIRWidget* widget,
                                    cJSON* component);

/**
 * Generate handler implementation for a specific language.
 * This is a placeholder - actual implementations will be in emitter code.
 *
 * @param handler_json Handler JSON from KIR
 * @param language Target language ("tcl", "limbo", etc.)
 * @return Generated code string, or NULL on error
 */
char* tkir_generate_handler_code(cJSON* handler_json, const char* language);

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
bool tkir_builder_extract_data_bindings(TKIRBuilderContext* ctx, cJSON* logic_block);

// ============================================================================
// Utilities
// ============================================================================

/**
 * Serialize a TKIRWidget to JSON.
 * Returns a cJSON object that should be added to the widgets array.
 *
 * @param widget TKIRWidget to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* tkir_widget_to_json(TKIRWidget* widget);

/**
 * Serialize a TKIRHandler to JSON.
 *
 * @param handler TKIRHandler to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* tkir_handler_to_json(TKIRHandler* handler);

/**
 * Serialize TKIRSize to JSON.
 *
 * @param size TKIRSize to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* tkir_size_to_json(TKIRSize* size);

/**
 * Serialize TKIRFont to JSON.
 *
 * @param font TKIRFont to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* tkir_font_to_json(TKIRFont* font);

/**
 * Serialize TKIRBorder to JSON.
 *
 * @param border TKIRBorder to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* tkir_border_to_json(TKIRBorder* border);

/**
 * Serialize TKIRLayoutOptions to JSON.
 *
 * @param layout TKIRLayoutOptions to serialize
 * @return cJSON object, or NULL on error
 */
cJSON* tkir_layout_to_json(TKIRLayoutOptions* layout);

#ifdef __cplusplus
}
#endif

#endif // TKIR_BUILDER_H
