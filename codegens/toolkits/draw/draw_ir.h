/**
 * Kryon DrawIR - Draw Toolkit Intermediate Representation
 *
 * This is the Draw-specific IR, NOT Tk!
 * Draw uses different widget types than Tk (e.g., "Button" not "button", "Textfield" not "entry")
 * This IR is designed specifically for the Limbo Draw toolkit.
 */

#ifndef DRAW_IR_H
#define DRAW_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DrawIR Types
// ============================================================================

/**
 * DrawIR widget types
 * These are Draw toolkit specific types, NOT Tk types!
 */
typedef enum {
    DRAW_IR_WIDGET_BUTTON,       // Button (not "button")
    DRAW_IR_WIDGET_TEXT,         // Text (not "label")
    DRAW_IR_WIDGET_TEXTFIELD,    // Textfield (not "entry"!)
    DRAW_IR_WIDGET_TEXTAREA,     // Textarea
    DRAW_IR_WIDGET_CHECKBOX,     // Checkbox
    DRAW_IR_WIDGET_RADIO,        // Radio
    DRAW_IR_WIDGET_IMAGE,        // Image
    DRAW_IR_WIDGET_CANVAS,       // Canvas
    DRAW_IR_WIDGET_FRAME,        // Frame (not "frame")
    DRAW_IR_WIDGET_COLUMN,       // Column layout
    DRAW_IR_WIDGET_ROW,          // Row layout
    DRAW_IR_WIDGET_SCROLL,       // Scroll container
    DRAW_IR_WIDGET_LIST,         // List
    DRAW_IR_WIDGET_SLIDER,       // Slider
    DRAW_IR_WIDGET_PROGRESSBAR,  // Progressbar
    DRAW_IR_WIDGET_UNKNOWN
} DrawIRWidgetType;

/**
 * DrawIR property types
 */
typedef enum {
    DRAW_IR_PROPERTY_STRING,
    DRAW_IR_PROPERTY_INTEGER,
    DRAW_IR_PROPERTY_FLOAT,
    DRAW_IR_PROPERTY_BOOLEAN,
    DRAW_IR_PROPERTY_COLOR,
    DRAW_IR_PROPERTY_POINT
} DrawIRPropertyType;

/**
 * DrawIR property value
 */
typedef struct DrawIRProperty {
    const char* name;                  // Property name
    DrawIRPropertyType type;           // Property type
    union {
        const char* string_value;
        int int_value;
        float float_value;
        bool bool_value;
        struct {                      // Color value
            uint8_t r, g, b, a;
        } color_value;
        struct {                      // Point value (for position)
            int x, y;
        } point_value;
    };
    struct DrawIRProperty* next;       // Next property in linked list
} DrawIRProperty;

/**
 * DrawIR widget
 */
typedef struct DrawIRWidget {
    DrawIRWidgetType type;             // Widget type
    const char* id;                    // Widget ID (auto-generated)
    const char* kir_type;              // Original KIR type (for debugging)

    // Layout properties (Draw uses absolute positioning)
    int x;                             // X position (0 for auto)
    int y;                             // Y position (0 for auto)
    int width;                         // Width (0 for auto)
    int height;                        // Height (0 for auto)
    bool has_position;                 // Whether position is set

    // Properties
    DrawIRProperty* properties;        // Linked list of properties

    // Event handlers
    char** events;                     // Event handler names
    int event_count;                   // Number of event handlers

    // Children
    struct DrawIRWidget** children;    // Array of child widgets
    int child_count;                   // Number of children

    // Parent
    struct DrawIRWidget* parent;       // Parent widget (or NULL for root)
} DrawIRWidget;

/**
 * DrawIR root
 * Represents the entire UI tree
 */
typedef struct DrawIRRoot {
    DrawIRWidget* root_widget;         // Root widget
    char** imports;                    // Required imports
    int import_count;                  // Number of imports

    // Event handlers
    char** handlers;                   // Handler function names
    int handler_count;                 // Number of handlers

    // Global properties
    const char* window_title;          // Window title
    int window_width;                  // Window width
    int window_height;                 // Window height
} DrawIRRoot;

// ============================================================================
// DrawIR Builder API (KIR → DrawIR)
// ============================================================================

/**
 * Create DrawIR from KIR JSON
 * This is the main entry point for converting KIR to DrawIR
 *
 * @param kir_json KIR JSON string
 * @param compute_layout Whether to compute layout (true) or keep as-is (false)
 * @return DrawIR root (caller must free with drawir_free), or NULL on error
 */
DrawIRRoot* drawir_build_from_kir(const char* kir_json, bool compute_layout);

/**
 * Free DrawIR root and all associated memory
 *
 * @param root DrawIR root to free
 */
void drawir_free(DrawIRRoot* root);

// ============================================================================
// DrawIR Widget API
// ============================================================================

/**
 * Create a new DrawIR widget
 *
 * @param type Widget type
 * @param id Widget ID (or NULL to auto-generate)
 * @return New widget (caller must free), or NULL on error
 */
DrawIRWidget* drawir_widget_new(DrawIRWidgetType type, const char* id);

/**
 * Free a DrawIR widget and all its children
 *
 * @param widget Widget to free
 */
void drawir_widget_free(DrawIRWidget* widget);

/**
 * Add a child to a widget
 *
 * @param parent Parent widget
 * @param child Child widget (parent takes ownership)
 * @return true on success, false on failure
 */
bool drawir_widget_add_child(DrawIRWidget* parent, DrawIRWidget* child);

/**
 * Add a property to a widget
 *
 * @param widget Widget to add property to
 * @param name Property name
 * @param type Property type
 * @return New property (caller fills value), or NULL on error
 */
DrawIRProperty* drawir_widget_add_property(DrawIRWidget* widget,
                                          const char* name,
                                          DrawIRPropertyType type);

/**
 * Get a property from a widget
 *
 * @param widget Widget to get property from
 * @param name Property name
 * @return Property, or NULL if not found
 */
DrawIRProperty* drawir_widget_get_property(DrawIRWidget* widget, const char* name);

/**
 * Add an event handler to a widget
 *
 * @param widget Widget to add handler to
 * @param event_name Event name
 * @return true on success, false on failure
 */
bool drawir_widget_add_event(DrawIRWidget* widget, const char* event_name);

// ============================================================================
// DrawIR Type Conversion
// ============================================================================

/**
 * Convert KIR widget type to DrawIR widget type
 * This is the CRITICAL function that fixes Tk bias!
 *
 * @param kir_type KIR widget type (e.g., "Button", "Input")
 * @return DrawIR widget type
 */
DrawIRWidgetType drawir_type_from_kir(const char* kir_type);

/**
 * Convert DrawIR widget type to string
 *
 * @param type DrawIR widget type
 * @return Draw widget type string (e.g., "Button", "Textfield")
 */
const char* drawir_type_to_string(DrawIRWidgetType type);

/**
 * Convert DrawIR widget type to Limbo class name
 *
 * @param type DrawIR widget type
 * @return Limbo class name (e.g., "Button", "Textfield")
 */
const char* drawir_type_to_limbo_class(DrawIRWidgetType type);

// ============================================================================
// DrawIR Emitter API (DrawIR → Limbo code)
// ============================================================================

/**
 * Generate Limbo code from DrawIR
 * This is the main code generation function
 *
 * @param root DrawIR root
 * @return Generated Limbo code (caller must free), or NULL on error
 */
char* drawir_emit_to_limbo(DrawIRRoot* root);

/**
 * Generate widget declaration
 *
 * @param widget Widget to emit
 * @param indent Indentation level
 * @return Generated code (caller must free), or NULL on error
 */
char* drawir_emit_widget_decl(DrawIRWidget* widget, int indent);

/**
 * Generate widget instantiation
 *
 * @param widget Widget to emit
 * @param indent Indentation level
 * @return Generated code (caller must free), or NULL on error
 */
char* drawir_emit_widget_inst(DrawIRWidget* widget, int indent);

/**
 * Generate event handler
 *
 * @param widget Widget with event
 * @param event_name Event name
 * @param handler_name Handler function name
 * @param indent Indentation level
 * @return Generated code (caller must free), or NULL on error
 */
char* drawir_emit_event_handler(DrawIRWidget* widget,
                                const char* event_name,
                                const char* handler_name,
                                int indent);

// ============================================================================
// Layout Computation
// ============================================================================

/**
 * Compute layout for DrawIR tree
 * Draw uses absolute positioning, so this computes X/Y for all widgets
 *
 * @param root DrawIR root
 * @return true on success, false on failure
 */
bool drawir_compute_layout(DrawIRRoot* root);

/**
 * Compute layout for a single widget
 *
 * @param widget Widget to compute layout for
 * @param parent_width Parent widget width
 * @param parent_height Parent widget height
 * @param next_x Output for next X position
 * @param next_y Output for next Y position
 * @return true on success, false on failure
 */
bool drawir_compute_widget_layout(DrawIRWidget* widget,
                                  int parent_width,
                                  int parent_height,
                                  int* next_x,
                                  int* next_y);

// ============================================================================
// Debug Utilities
// ============================================================================

/**
 * Print DrawIR tree to stdout (for debugging)
 *
 * @param root DrawIR root
 */
void drawir_print_tree(DrawIRRoot* root);

/**
 * Print DrawIR widget to stdout (for debugging)
 *
 * @param widget Widget to print
 * @param indent Indentation level
 */
void drawir_print_widget(DrawIRWidget* widget, int indent);

#ifdef __cplusplus
}
#endif

#endif // DRAW_IR_H
