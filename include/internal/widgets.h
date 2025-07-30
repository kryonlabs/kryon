/**
 * @file widgets.h
 * @brief Kryon Widget System - Complete Widget Interface
 */

#ifndef KRYON_INTERNAL_WIDGETS_H
#define KRYON_INTERNAL_WIDGETS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "internal/renderer_interface.h"
#include "internal/events.h"

// =============================================================================
// WIDGET BASE TYPES
// =============================================================================

typedef enum {
    KRYON_WIDGET_BUTTON,
    KRYON_WIDGET_INPUT,
    KRYON_WIDGET_DROPDOWN,
    KRYON_WIDGET_CHECKBOX,
    KRYON_WIDGET_SLIDER,
    KRYON_WIDGET_CONTAINER,
    KRYON_WIDGET_TEXT,
    KRYON_WIDGET_IMAGE,
    KRYON_WIDGET_CANVAS
} KryonWidgetType;

typedef struct {
    float top, right, bottom, left;
} KryonPadding, KryonMargin;

typedef struct {
    KryonColor background_color;
    KryonColor text_color;
    KryonColor border_color;
    float border_width;
    float border_radius;
    float font_size;
    char* font_family;
    KryonPadding padding;
    KryonMargin margin;
    float opacity;
} KryonWidgetStyle;

typedef struct {
    KryonWidgetType type;
    char* id;
    char* class_name;
    KryonRect bounds;
    KryonWidgetState state;
    KryonWidgetStyle style;
    bool visible;
    bool enabled;
    void* user_data;
} KryonWidget;

// =============================================================================
// DROPDOWN WIDGET TYPES
// =============================================================================

typedef struct KryonDropdownWidget KryonDropdownWidget;

typedef void (*KryonDropdownChangeCallback)(KryonDropdownWidget* dropdown, int selected_index, 
                                           const char* selected_value, void* user_data);
typedef void (*KryonDropdownOpenCallback)(KryonDropdownWidget* dropdown, void* user_data);
typedef void (*KryonDropdownCloseCallback)(KryonDropdownWidget* dropdown, void* user_data);

// =============================================================================
// WIDGET BASE FUNCTIONS
// =============================================================================

/**
 * Initialize a widget base structure
 */
void kryon_widget_init(KryonWidget* widget, KryonWidgetType type);

/**
 * Cleanup a widget base structure
 */
void kryon_widget_cleanup(KryonWidget* widget);

/**
 * Set widget bounds
 */
void kryon_widget_set_bounds(KryonWidget* widget, KryonRect bounds);

/**
 * Set widget style
 */
void kryon_widget_set_style(KryonWidget* widget, const KryonWidgetStyle* style);

/**
 * Check if point is inside widget bounds
 */
bool kryon_point_in_rect(KryonVec2 point, KryonRect rect);

// =============================================================================
// DROPDOWN WIDGET API
// =============================================================================

/**
 * Create a new dropdown widget
 */
KryonDropdownWidget* kryon_dropdown_create(const char* id);

/**
 * Destroy a dropdown widget
 */
void kryon_dropdown_destroy(KryonDropdownWidget* dropdown);

/**
 * Add an option to the dropdown
 */
bool kryon_dropdown_add_option(KryonDropdownWidget* dropdown, const char* text, const char* value);

/**
 * Clear all options from the dropdown
 */
void kryon_dropdown_clear_options(KryonDropdownWidget* dropdown);

/**
 * Set the selected index
 */
bool kryon_dropdown_set_selected_index(KryonDropdownWidget* dropdown, int index);

/**
 * Get the selected index
 */
int kryon_dropdown_get_selected_index(KryonDropdownWidget* dropdown);

/**
 * Get the selected value
 */
const char* kryon_dropdown_get_selected_value(KryonDropdownWidget* dropdown);

/**
 * Get the selected text
 */
const char* kryon_dropdown_get_selected_text(KryonDropdownWidget* dropdown);

/**
 * Set dropdown callbacks
 */
void kryon_dropdown_set_callbacks(KryonDropdownWidget* dropdown,
                                 KryonDropdownChangeCallback on_change,
                                 KryonDropdownOpenCallback on_open,
                                 KryonDropdownCloseCallback on_close,
                                 void* user_data);

/**
 * Set maximum height for dropdown list
 */
void kryon_dropdown_set_max_height(KryonDropdownWidget* dropdown, float height);

/**
 * Set height for each dropdown item
 */
void kryon_dropdown_set_item_height(KryonDropdownWidget* dropdown, float height);

/**
 * Check if dropdown is open
 */
bool kryon_dropdown_is_open(KryonDropdownWidget* dropdown);

/**
 * Open the dropdown
 */
void kryon_dropdown_open(KryonDropdownWidget* dropdown);

/**
 * Close the dropdown
 */
void kryon_dropdown_close(KryonDropdownWidget* dropdown);

/**
 * Render the dropdown widget
 */
void kryon_dropdown_render(KryonDropdownWidget* dropdown, KryonRenderContext* ctx);

/**
 * Handle events for the dropdown widget
 */
bool kryon_dropdown_handle_event(KryonDropdownWidget* dropdown, const KryonEvent* event);

/**
 * Update the dropdown widget (animations, etc.)
 */
void kryon_dropdown_update(KryonDropdownWidget* dropdown, float delta_time);

// =============================================================================
// HTML/WEB INTEGRATION
// =============================================================================

/**
 * Convert dropdown to HTML select element
 */
char* kryon_dropdown_to_html_select(KryonDropdownWidget* dropdown);

/**
 * Convert dropdown to React component
 */
char* kryon_dropdown_to_react_select(KryonDropdownWidget* dropdown);

/**
 * Convert dropdown to render command
 */
KryonRenderCommand kryon_dropdown_to_render_command(KryonDropdownWidget* dropdown);

// =============================================================================
// WIDGET MANAGER
// =============================================================================

typedef struct KryonWidgetManager KryonWidgetManager;

/**
 * Create a widget manager
 */
KryonWidgetManager* kryon_widget_manager_create(void);

/**
 * Destroy a widget manager
 */
void kryon_widget_manager_destroy(KryonWidgetManager* manager);

/**
 * Register a widget with the manager
 */
bool kryon_widget_register(KryonWidgetManager* manager, KryonWidget* widget);

/**
 * Unregister a widget
 */
void kryon_widget_unregister(KryonWidgetManager* manager, const char* widgetId);

/**
 * Find a widget by ID
 */
KryonWidget* kryon_widget_find(KryonWidgetManager* manager, const char* widgetId);

/**
 * Update widget states based on input
 */
void kryon_widget_update_all(KryonWidgetManager* manager, 
                            const KryonInputState* input,
                            KryonEventSystem* eventSystem);

/**
 * Handle mouse events for widgets
 */
bool kryon_widget_handle_mouse(KryonWidgetManager* manager,
                              const KryonEvent* event,
                              KryonEventSystem* eventSystem);

/**
 * Handle keyboard events for widgets
 */
bool kryon_widget_handle_keyboard(KryonWidgetManager* manager,
                                 const KryonEvent* event,
                                 KryonEventSystem* eventSystem);

/**
 * Set keyboard focus to a widget
 */
void kryon_widget_set_focus(KryonWidgetManager* manager, const char* widgetId);

/**
 * Clear keyboard focus
 */
void kryon_widget_clear_focus(KryonWidgetManager* manager);

/**
 * Get the currently focused widget
 */
KryonWidget* kryon_widget_get_focused(KryonWidgetManager* manager);

// =============================================================================
// WIDGET CREATION HELPERS
// =============================================================================

/**
 * Create a button widget
 */
KryonWidget* kryon_widget_create_button(const char* id, KryonRect bounds);

/**
 * Create an input widget
 */
KryonWidget* kryon_widget_create_input(const char* id, KryonRect bounds, size_t textCapacity);

/**
 * Create a dropdown widget
 */
KryonWidget* kryon_widget_create_dropdown(const char* id, KryonRect bounds, size_t itemCount);

/**
 * Create a checkbox widget
 */
KryonWidget* kryon_widget_create_checkbox(const char* id, KryonRect bounds);

/**
 * Create a slider widget
 */
KryonWidget* kryon_widget_create_slider(const char* id, KryonRect bounds,
                                       float minValue, float maxValue, float initialValue);

/**
 * Destroy a widget
 */
void kryon_widget_destroy(KryonWidget* widget);

// =============================================================================
// WIDGET INTERACTION
// =============================================================================

/**
 * Check if a point is inside a widget
 */
static inline bool kryon_widget_contains_point(const KryonWidget* widget, KryonVec2 point) {
    return point.x >= widget->bounds.x &&
           point.x <= widget->bounds.x + widget->bounds.width &&
           point.y >= widget->bounds.y &&
           point.y <= widget->bounds.y + widget->bounds.height;
}

/**
 * Update button state
 */
void kryon_widget_button_update(KryonWidget* widget, bool isPressed, KryonEventSystem* eventSystem);

/**
 * Update input text
 */
void kryon_widget_input_set_text(KryonWidget* widget, const char* text);

/**
 * Insert text at cursor position
 */
void kryon_widget_input_insert_text(KryonWidget* widget, const char* text, KryonEventSystem* eventSystem);

/**
 * Handle input key press
 */
void kryon_widget_input_handle_key(KryonWidget* widget, KryonKey key, KryonEventSystem* eventSystem);

/**
 * Update dropdown selection
 */
void kryon_widget_dropdown_select(KryonWidget* widget, int index, KryonEventSystem* eventSystem);

/**
 * Toggle dropdown open state
 */
void kryon_widget_dropdown_toggle(KryonWidget* widget, KryonEventSystem* eventSystem);

/**
 * Update checkbox state
 */
void kryon_widget_checkbox_toggle(KryonWidget* widget, KryonEventSystem* eventSystem);

/**
 * Update slider value
 */
void kryon_widget_slider_set_value(KryonWidget* widget, float value, KryonEventSystem* eventSystem);

// =============================================================================
// WIDGET RENDERING HELPERS
// =============================================================================

/**
 * Generate render commands for a button
 */
KryonRenderCommand kryon_widget_button_render(const KryonWidget* widget,
                                             const char* text,
                                             KryonColor bgColor,
                                             KryonColor textColor);

/**
 * Generate render commands for an input
 */
KryonRenderCommand kryon_widget_input_render(const KryonWidget* widget,
                                            const char* placeholder,
                                            KryonColor bgColor,
                                            KryonColor textColor);

/**
 * Generate render commands for a dropdown
 */
KryonRenderCommand kryon_widget_dropdown_render(const KryonWidget* widget,
                                               KryonDropdownItem* items,
                                               size_t itemCount,
                                               KryonColor bgColor,
                                               KryonColor textColor);

/**
 * Generate render commands for a checkbox
 */
KryonRenderCommand kryon_widget_checkbox_render(const KryonWidget* widget,
                                               const char* label,
                                               KryonColor bgColor,
                                               KryonColor checkColor);

/**
 * Generate render commands for a slider
 */
KryonRenderCommand kryon_widget_slider_render(const KryonWidget* widget,
                                             KryonColor trackColor,
                                             KryonColor thumbColor);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_WIDGETS_H