/**
 * Kryon UI Framework - C Frontend
 *
 * This is the main C API for building Kryon UIs.
 * The API follows a runtime builder pattern that wraps the IR builder API.
 *
 * Usage:
 *   1. Call kryon_init() to initialize
 *   2. Create components with kryon_*() functions
 *   3. Set properties with kryon_set_*() functions
 *   4. Build tree with kryon_add_child()
 *   5. Call kryon_finalize() to serialize to .kir
 */

#ifndef KRYON_H
#define KRYON_H

#include "../../ir/include/ir_core.h"
#include "../../ir/include/ir_builder.h"
#include "../../ir/include/ir_serialization.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// App State Structure
// ============================================================================

// Forward declaration for state manager
struct IRStateManager;

typedef struct {
    IRContext* context;
    IRComponent* root;
    char* window_title;
    int window_width;
    int window_height;
    struct IRStateManager* state_manager;  // For cleanup
} KryonAppState;

extern KryonAppState g_app_state;

// ============================================================================
// Initialization & App Management
// ============================================================================

/**
 * Initialize the Kryon context with window configuration
 */
void kryon_init(const char* title, int width, int height);

/**
 * Get the root component
 */
IRComponent* kryon_get_root(void);

/**
 * Finalize and serialize the component tree to a .kir file
 */
bool kryon_finalize(const char* output_path);

/**
 * Clean up and destroy all resources
 */
void kryon_cleanup(void);

/**
 * Get the renderer backend from kryon.toml configuration
 * Returns the backend type enum value, or -1 if not found/error
 */
int kryon_get_renderer_backend_from_config(void);

// ============================================================================
// Component Creation
// ============================================================================

/**
 * Create a generic container component
 */
IRComponent* kryon_container(void);

/**
 * Create a row layout component (horizontal flex)
 */
IRComponent* kryon_row(void);

/**
 * Create a column layout component (vertical flex)
 */
IRComponent* kryon_column(void);

/**
 * Create a center alignment component
 */
IRComponent* kryon_center(void);

/**
 * Create a text display component
 */
IRComponent* kryon_text(const char* content);

/**
 * Create a button component with label
 */
IRComponent* kryon_button(const char* label);

/**
 * Create a text input component
 */
IRComponent* kryon_input(const char* placeholder);

/**
 * Create a checkbox component
 */
IRComponent* kryon_checkbox(const char* label, bool checked);

/**
 * Create a dropdown component
 */
IRComponent* kryon_dropdown(const char* placeholder);

/**
 * Create an image component
 */
IRComponent* kryon_image(const char* src, const char* alt);

// ============================================================================
// Tab Components
// ============================================================================

IRComponent* kryon_tab_group(void);
IRComponent* kryon_tab_bar(void);
IRComponent* kryon_tab(const char* title);
IRComponent* kryon_tab_content(void);
IRComponent* kryon_tab_panel(void);

// ============================================================================
// Table Components
// ============================================================================

IRComponent* kryon_table(void);
IRComponent* kryon_table_head(void);
IRComponent* kryon_table_body(void);
IRComponent* kryon_table_row(void);
IRComponent* kryon_table_cell(const char* content);
IRComponent* kryon_table_header_cell(const char* content);

// Table styling
void kryon_table_set_cell_padding(IRComponent* table, float padding);
void kryon_table_set_striped(IRComponent* table, bool striped);
void kryon_table_set_show_borders(IRComponent* table, bool show);
void kryon_table_set_header_background(IRComponent* table, uint32_t color);
void kryon_table_set_even_row_background(IRComponent* table, uint32_t color);
void kryon_table_set_odd_row_background(IRComponent* table, uint32_t color);
void kryon_table_set_border_color(IRComponent* table, uint32_t color);

// ============================================================================
// Component Properties - Dimensions
// ============================================================================

/**
 * Set width with unit ("px", "%", "auto", "fr")
 */
void kryon_set_width(IRComponent* c, float value, const char* unit);

/**
 * Set height with unit
 */
void kryon_set_height(IRComponent* c, float value, const char* unit);

/**
 * Set min-width with unit
 */
void kryon_set_min_width(IRComponent* c, float value, const char* unit);

/**
 * Set min-height with unit
 */
void kryon_set_min_height(IRComponent* c, float value, const char* unit);

/**
 * Set max-width with unit
 */
void kryon_set_max_width(IRComponent* c, float value, const char* unit);

/**
 * Set max-height with unit
 */
void kryon_set_max_height(IRComponent* c, float value, const char* unit);

/**
 * Set absolute positioning with coordinates
 */
void kryon_set_position_absolute(IRComponent* c, float x, float y);

// ============================================================================
// Component Properties - Colors
// ============================================================================

/**
 * Set background color (0xRRGGBB or 0xRRGGBBAA)
 */
void kryon_set_background(IRComponent* c, uint32_t color);

/**
 * Set text color (0xRRGGBB or 0xRRGGBBAA)
 */
void kryon_set_color(IRComponent* c, uint32_t color);

/**
 * Set border color (0xRRGGBB or 0xRRGGBBAA)
 */
void kryon_set_border_color(IRComponent* c, uint32_t color);

// ============================================================================
// Component Properties - Spacing
// ============================================================================

/**
 * Set uniform padding on all sides
 */
void kryon_set_padding(IRComponent* c, float value);

/**
 * Set padding for each side individually
 */
void kryon_set_padding_sides(IRComponent* c, float top, float right, float bottom, float left);

/**
 * Set uniform margin on all sides
 */
void kryon_set_margin(IRComponent* c, float value);

/**
 * Set margin for each side individually
 */
void kryon_set_margin_sides(IRComponent* c, float top, float right, float bottom, float left);

/**
 * Set gap between children (for flex layouts)
 */
void kryon_set_gap(IRComponent* c, float value);

// ============================================================================
// Component Properties - Typography
// ============================================================================

/**
 * Set font size in pixels
 */
void kryon_set_font_size(IRComponent* c, float size);

/**
 * Set font family
 */
void kryon_set_font_family(IRComponent* c, const char* family);

/**
 * Set font weight (100-900, 400=normal, 700=bold)
 */
void kryon_set_font_weight(IRComponent* c, uint16_t weight);

/**
 * Set font bold
 */
void kryon_set_font_bold(IRComponent* c, bool bold);

/**
 * Set font italic
 */
void kryon_set_font_italic(IRComponent* c, bool italic);

/**
 * Set line height multiplier
 */
void kryon_set_line_height(IRComponent* c, float line_height);

/**
 * Set letter spacing in pixels
 */
void kryon_set_letter_spacing(IRComponent* c, float spacing);

/**
 * Set text alignment
 */
void kryon_set_text_align(IRComponent* c, IRTextAlign align);

// ============================================================================
// Component Properties - Border & Radius
// ============================================================================

/**
 * Set border width in pixels
 */
void kryon_set_border_width(IRComponent* c, float width);

/**
 * Set border radius in pixels
 */
void kryon_set_border_radius(IRComponent* c, float radius);

// ============================================================================
// Component Properties - Effects
// ============================================================================

/**
 * Set opacity (0.0 = transparent, 1.0 = opaque)
 */
void kryon_set_opacity(IRComponent* c, float opacity);

/**
 * Set z-index for layering
 */
void kryon_set_z_index(IRComponent* c, uint32_t z_index);

/**
 * Set visibility
 */
void kryon_set_visible(IRComponent* c, bool visible);

// ============================================================================
// Layout Properties
// ============================================================================

/**
 * Set justify-content for flex layout
 */
void kryon_set_justify_content(IRComponent* c, IRAlignment align);

/**
 * Set align-items for flex layout
 */
void kryon_set_align_items(IRComponent* c, IRAlignment align);

/**
 * Set flex-grow (default: 0)
 */
void kryon_set_flex_grow(IRComponent* c, uint8_t grow);

/**
 * Set flex-shrink (default: 1)
 */
void kryon_set_flex_shrink(IRComponent* c, uint8_t shrink);

/**
 * Set flex-wrap
 */
void kryon_set_flex_wrap(IRComponent* c, bool wrap);

// ============================================================================
// Tree Management
// ============================================================================

/**
 * Add a child component to a parent
 */
void kryon_add_child(IRComponent* parent, IRComponent* child);

/**
 * Remove a child component from a parent
 */
void kryon_remove_child(IRComponent* parent, IRComponent* child);

/**
 * Insert a child at a specific index
 */
void kryon_insert_child(IRComponent* parent, IRComponent* child, uint32_t index);

// ============================================================================
// Event Handlers
// ============================================================================

/**
 * Event handler function type
 */
typedef void (*KryonEventHandler)(void);

/**
 * Register a click event handler
 */
void kryon_on_click(IRComponent* component, KryonEventHandler handler, const char* handler_name);

/**
 * Register a change event handler (for inputs)
 */
void kryon_on_change(IRComponent* component, KryonEventHandler handler, const char* handler_name);

/**
 * Register a hover event handler
 */
void kryon_on_hover(IRComponent* component, KryonEventHandler handler, const char* handler_name);

/**
 * Register a focus event handler
 */
void kryon_on_focus(IRComponent* component, KryonEventHandler handler, const char* handler_name);

/**
 * C event bridge - called by the renderer when events fire
 * This is exported for the C renderer to call
 */
void kryon_c_event_bridge(const char* logic_id);

// ============================================================================
// Color Utilities
// ============================================================================

/**
 * Create an RGB color value
 */
#define KRYON_COLOR_RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/**
 * Create an RGBA color value
 */
#define KRYON_COLOR_RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/**
 * Named color constants
 */
#define KRYON_COLOR_BLACK       0x000000
#define KRYON_COLOR_WHITE       0xFFFFFF
#define KRYON_COLOR_RED         0xFF0000
#define KRYON_COLOR_GREEN       0x00FF00
#define KRYON_COLOR_BLUE        0x0000FF
#define KRYON_COLOR_YELLOW      0xFFFF00
#define KRYON_COLOR_CYAN        0x00FFFF
#define KRYON_COLOR_MAGENTA     0xFF00FF
#define KRYON_COLOR_GRAY        0x808080
#define KRYON_COLOR_DARK_GRAY   0x404040
#define KRYON_COLOR_LIGHT_GRAY  0xC0C0C0
#define KRYON_COLOR_TRANSPARENT 0x00000000

// ============================================================================
// ============================================================================
// C Source Metadata API (for round-trip conversion)
// ============================================================================

void kryon_register_variable(const char* name, const char* type, const char* storage,
                              const char* initial_value, uint32_t component_id, int line_number);

void kryon_register_event_handler(const char* logic_id, const char* function_name,
                                   const char* return_type, const char* parameters,
                                   const char* body, int line_number);

void kryon_register_helper_function(const char* name, const char* return_type,
                                     const char* parameters, const char* body, int line_number);

void kryon_add_include(const char* include_string, bool is_system, int line_number);

void kryon_add_preprocessor_directive(const char* directive_type, const char* condition,
                                       const char* value, int line_number);

void kryon_add_source_file(const char* filename, const char* full_path, const char* content);

void kryon_set_main_source_file(const char* filename);

// ============================================================================
// Application Lifecycle - Graceful Shutdown System
// ============================================================================

// Types defined in desktop_platform.h (single source of truth)
#include "../../runtime/desktop/desktop_platform.h"

// C-style naming aliases
typedef KryonShutdownState kryon_shutdown_state_t;
typedef KryonShutdownReason kryon_shutdown_reason_t;
typedef KryonShutdownCallback kryon_shutdown_callback_t;
typedef KryonCleanupCallback kryon_cleanup_callback_t;

/**
 * Request graceful shutdown
 * This initiates the shutdown sequence:
 * 1. State transitions to SHUTDOWN_REQUESTED
 * 2. All registered shutdown callbacks are called
 * 3. If not vetoed, state transitions to SHUTDOWN_IN_PROGRESS
 * 4. Main loop exits
 * 5. Cleanup callback is called (GPU resources can be freed here)
 * 6. State transitions to SHUTDOWN_COMPLETE
 * 7. Window is closed
 *
 * @param renderer The renderer instance (pass NULL to use global)
 * @param reason The reason for shutdown
 * @return true if shutdown is proceeding, false if vetoed
 */
bool kryon_request_shutdown(void* renderer, kryon_shutdown_reason_t reason);

/**
 * Register a shutdown callback
 * Callbacks are called in priority order (highest first) when shutdown is requested.
 * Only callbacks for USER-initiated shutdowns can veto by returning false.
 *
 * @param renderer The renderer instance (pass NULL to use global)
 * @param callback Function to call on shutdown
 * @param user_data User data to pass to callback
 * @param priority Callback priority (higher = called first)
 * @return true if registered successfully
 */
bool kryon_register_shutdown_callback(void* renderer,
                                       kryon_shutdown_callback_t callback,
                                       void* user_data,
                                       int priority);

/**
 * Set cleanup callback
 * The cleanup callback is called after the main loop exits but before the
 * window is closed. This is the correct place to free GPU resources like
 * textures, meshes, and shaders.
 *
 * @param renderer The renderer instance (pass NULL to use global)
 * @param callback Function to call for cleanup
 * @param user_data User data to pass to callback
 */
void kryon_set_cleanup_callback(void* renderer,
                                 kryon_cleanup_callback_t callback,
                                 void* user_data);

/**
 * Get current shutdown state
 * @param renderer The renderer instance (pass NULL to use global)
 * @return Current shutdown state
 */
kryon_shutdown_state_t kryon_get_shutdown_state(void* renderer);

/**
 * Check if renderer is shutting down
 * @param renderer The renderer instance (pass NULL to use global)
 * @return true if shutdown is in progress or complete
 */
bool kryon_is_shutting_down(void* renderer);

#ifdef __cplusplus
}
#endif

#endif // KRYON_H
