/**
 * Kryon UI Framework - C DSL Frontend
 *
 * Beautiful declarative macro-based DSL for Kryon in C.
 * Provides a concise syntax similar to .kry files.
 *
 * Usage:
 *   #include <kryon_dsl.h>
 *
 *   int main(void) {
 *       kryon_init("App", 800, 600);
 *
 *       KRYON_APP(
 *           COLUMN(
 *               WIDTH_PCT(100), HEIGHT_PCT(100),
 *               BG_COLOR(0x101820),
 *
 *               TEXT("Hello World", COLOR_YELLOW, FONT_SIZE(24))
 *           )
 *       );
 *
 *       kryon_finalize("output.kir");
 *       kryon_cleanup();
 *       return 0;
 *   }
 */

#ifndef KRYON_DSL_H
#define KRYON_DSL_H

#include "kryon.h"
#include "../../ir/include/ir_native_canvas.h"
#include "../../ir/include/ir_component_factory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Internal DSL Infrastructure
// ============================================================================

// Parent stack management functions
void _kryon_push_parent(IRComponent* parent);
void _kryon_pop_parent(void);
IRComponent* _kryon_get_current_parent(void);

// Helper wrapper functions for property setters (return int for comma operator compatibility)
static inline int _set_width(IRComponent* comp, float val, const char* unit) {
    kryon_set_width(comp, val, unit);
    return 0;
}
static inline int _set_height(IRComponent* comp, float val, const char* unit) {
    kryon_set_height(comp, val, unit);
    return 0;
}
static inline int _set_min_width(IRComponent* comp, float val, const char* unit) {
    kryon_set_min_width(comp, val, unit);
    return 0;
}
static inline int _set_max_width(IRComponent* comp, float val, const char* unit) {
    kryon_set_max_width(comp, val, unit);
    return 0;
}
static inline int _set_min_height(IRComponent* comp, float val, const char* unit) {
    kryon_set_min_height(comp, val, unit);
    return 0;
}
static inline int _set_max_height(IRComponent* comp, float val, const char* unit) {
    kryon_set_max_height(comp, val, unit);
    return 0;
}
static inline int _set_position_absolute(IRComponent* comp, float x, float y) {
    kryon_set_position_absolute(comp, x, y);
    return 0;
}
static inline int _set_pos_x(IRComponent* comp, float x) {
    // Set position mode to absolute and update x
    // Use kryon_set_position_absolute to ensure style is created
    if (comp) {
        float current_y = (comp->style) ? comp->style->absolute_y : 0;
        kryon_set_position_absolute(comp, x, current_y);
    }
    return 0;
}
static inline int _set_pos_y(IRComponent* comp, float y) {
    // Set position mode to absolute and update y
    // Use kryon_set_position_absolute to ensure style is created
    if (comp) {
        float current_x = (comp->style) ? comp->style->absolute_x : 0;
        kryon_set_position_absolute(comp, current_x, y);
    }
    return 0;
}
static inline int _set_background(IRComponent* comp, uint32_t color) {
    kryon_set_background(comp, color);
    return 0;
}
static inline int _set_color(IRComponent* comp, uint32_t color) {
    kryon_set_color(comp, color);
    return 0;
}
static inline int _set_border_color(IRComponent* comp, uint32_t color) {
    kryon_set_border_color(comp, color);
    return 0;
}
static inline int _set_justify_content(IRComponent* comp, int alignment) {
    kryon_set_justify_content(comp, alignment);
    return 0;
}
static inline int _set_align_items(IRComponent* comp, int alignment) {
    kryon_set_align_items(comp, alignment);
    return 0;
}
static inline int _set_gap(IRComponent* comp, float gap) {
    kryon_set_gap(comp, gap);
    return 0;
}
static inline int _set_flex_grow(IRComponent* comp, uint8_t val) {
    kryon_set_flex_grow(comp, val);
    return 0;
}
static inline int _set_flex_shrink(IRComponent* comp, uint8_t val) {
    kryon_set_flex_shrink(comp, val);
    return 0;
}
static inline int _set_flex_wrap(IRComponent* comp, bool wrap) {
    kryon_set_flex_wrap(comp, wrap);
    return 0;
}
static inline int _set_padding(IRComponent* comp, float padding) {
    kryon_set_padding(comp, padding);
    return 0;
}
static inline int _set_margin(IRComponent* comp, float margin) {
    kryon_set_margin(comp, margin);
    return 0;
}
static inline int _set_padding_sides(IRComponent* comp, float t, float r, float b, float l) {
    kryon_set_padding_sides(comp, t, r, b, l);
    return 0;
}
static inline int _set_margin_sides(IRComponent* comp, float t, float r, float b, float l) {
    kryon_set_margin_sides(comp, t, r, b, l);
    return 0;
}
static inline int _set_dropdown_options(IRComponent* comp, const char** opts, uint32_t count) {
    ir_set_dropdown_options(comp, (char**)opts, count);
    return 0;
}
static inline int _set_dropdown_selected_index(IRComponent* comp, int32_t index) {
    ir_set_dropdown_selected_index(comp, index);
    return 0;
}
static inline int _set_font_size(IRComponent* comp, float size) {
    kryon_set_font_size(comp, size);
    return 0;
}
static inline int _set_font_family(IRComponent* comp, const char* family) {
    kryon_set_font_family(comp, family);
    return 0;
}
static inline int _set_font_weight(IRComponent* comp, uint16_t weight) {
    kryon_set_font_weight(comp, weight);
    return 0;
}
static inline int _set_font_bold(IRComponent* comp, bool bold) {
    kryon_set_font_bold(comp, bold);
    return 0;
}
static inline int _set_font_italic(IRComponent* comp, bool italic) {
    kryon_set_font_italic(comp, italic);
    return 0;
}
static inline int _set_line_height(IRComponent* comp, float height) {
    kryon_set_line_height(comp, height);
    return 0;
}
static inline int _set_letter_spacing(IRComponent* comp, float spacing) {
    kryon_set_letter_spacing(comp, spacing);
    return 0;
}
static inline int _set_text_align(IRComponent* comp, int align) {
    kryon_set_text_align(comp, align);
    return 0;
}
static inline int _set_border_width(IRComponent* comp, float width) {
    kryon_set_border_width(comp, width);
    return 0;
}
static inline int _set_border_radius(IRComponent* comp, float radius) {
    kryon_set_border_radius(comp, radius);
    return 0;
}
static inline int _set_opacity(IRComponent* comp, float opacity) {
    kryon_set_opacity(comp, opacity);
    return 0;
}
static inline int _set_z_index(IRComponent* comp, int32_t z_index) {
    kryon_set_z_index(comp, z_index);
    return 0;
}
static inline int _set_visible(IRComponent* comp, bool visible) {
    kryon_set_visible(comp, visible);
    return 0;
}
static inline int _on_click(IRComponent* comp, KryonEventHandler handler, const char* name) {
    kryon_on_click(comp, handler, name);
    return 0;
}
static inline int _on_change(IRComponent* comp, KryonEventHandler handler, const char* name) {
    kryon_on_change(comp, handler, name);
    return 0;
}
static inline int _on_hover(IRComponent* comp, KryonEventHandler handler, const char* name) {
    kryon_on_hover(comp, handler, name);
    return 0;
}
static inline int _on_focus(IRComponent* comp, KryonEventHandler handler, const char* name) {
    kryon_on_focus(comp, handler, name);
    return 0;
}

// Table styling helpers
static inline int _set_table_cell_padding(IRComponent* comp, float padding) {
    kryon_table_set_cell_padding(comp, padding);
    return 0;
}
static inline int _set_table_striped(IRComponent* comp, bool striped) {
    kryon_table_set_striped(comp, striped);
    return 0;
}
static inline int _set_table_show_borders(IRComponent* comp, bool show) {
    kryon_table_set_show_borders(comp, show);
    return 0;
}
static inline int _set_table_header_bg(IRComponent* comp, uint32_t color) {
    kryon_table_set_header_background(comp, color);
    return 0;
}
static inline int _set_table_even_row_bg(IRComponent* comp, uint32_t color) {
    kryon_table_set_even_row_background(comp, color);
    return 0;
}
static inline int _set_table_odd_row_bg(IRComponent* comp, uint32_t color) {
    kryon_table_set_odd_row_background(comp, color);
    return 0;
}
static inline int _set_table_border_color(IRComponent* comp, uint32_t color) {
    kryon_table_set_border_color(comp, color);
    return 0;
}

// Helper to add component to current parent
static inline IRComponent* _kryon_add_to_parent(IRComponent* comp) {
    IRComponent* parent = _kryon_get_current_parent();
    if (parent) {
        kryon_add_child(parent, comp);
    }
    return comp;
}

// ============================================================================
// Core Component Macros
// ============================================================================

/**
 * KRYON_APP - Root application macro
 * Wraps the entire component tree
 */
#define KRYON_APP(...) \
    ({ \
        IRComponent* _root = kryon_get_root(); \
        _kryon_push_parent(_root); \
        __VA_ARGS__; \
        _kryon_pop_parent(); \
        _root; \
    })

/**
 * Generic component macro - handles property application and nesting
 */
#define _KRYON_COMP(create_fn, ...) \
    ({ \
        IRComponent* _comp = create_fn; \
        _kryon_add_to_parent(_comp); \
        _kryon_push_parent(_comp); \
        do { (void)0; __VA_ARGS__; } while(0); \
        _kryon_pop_parent(); \
        _comp; \
    })

/**
 * Property-only component macro - for components with properties but no guaranteed children
 */
#define _KRYON_PROP_COMP(create_fn, ...) \
    ({ \
        IRComponent* _comp = create_fn; \
        _kryon_add_to_parent(_comp); \
        _kryon_push_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _kryon_pop_parent(); \
        _comp; \
    })

// ============================================================================
// Layout Components
// ============================================================================

#define COLUMN(...) _KRYON_COMP(kryon_column(), __VA_ARGS__)
#define ROW(...) _KRYON_COMP(kryon_row(), __VA_ARGS__)
#define CONTAINER(...) _KRYON_COMP(kryon_container(), __VA_ARGS__)
#define CENTER(...) _KRYON_COMP(kryon_center(), __VA_ARGS__)

// ============================================================================
// UI Components
// ============================================================================

#define TEXT(content, ...) \
    ({ \
        IRComponent* _comp = kryon_text(content); \
        _kryon_add_to_parent(_comp); \
        _kryon_push_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _kryon_pop_parent(); \
        _comp; \
    })

#define BUTTON(label, ...) \
    ({ \
        IRComponent* _comp = kryon_button(label); \
        _kryon_add_to_parent(_comp); \
        _kryon_push_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _kryon_pop_parent(); \
        _comp; \
    })

#define INPUT(placeholder, ...) \
    ({ \
        IRComponent* _comp = kryon_input(placeholder); \
        _kryon_add_to_parent(_comp); \
        _kryon_push_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _kryon_pop_parent(); \
        _comp; \
    })

#define CHECKBOX(label, checked, ...) \
    ({ \
        IRComponent* _comp = kryon_checkbox(label, checked); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

#define DROPDOWN(placeholder, ...) \
    ({ \
        IRComponent* _comp = kryon_dropdown(placeholder); \
        _kryon_add_to_parent(_comp); \
        _kryon_push_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _kryon_pop_parent(); \
        _comp; \
    })

#define IMAGE(src, alt, ...) \
    ({ \
        IRComponent* _comp = kryon_image(src, alt); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

/**
 * NATIVE_CANVAS - Native rendering canvas with backend access
 * Allows direct Raylib/SDL3 rendering inside a callback
 *
 * Usage:
 *   NATIVE_CANVAS(render_callback,
 *       WIDTH(800),
 *       HEIGHT(600)
 *   )
 */
#define NATIVE_CANVAS(callback, ...) \
    ({ \
        IRComponent* _comp = ir_create_native_canvas(0, 0); \
        if (_comp) { \
            _kryon_add_to_parent(_comp); \
            _kryon_push_parent(_comp); \
            (void)0, ##__VA_ARGS__; \
            _kryon_pop_parent(); \
            ir_native_canvas_set_callback(_comp->id, callback); \
        } \
        _comp; \
    })

// ============================================================================
// Table Components
// ============================================================================

#define TABLE(...) _KRYON_COMP(kryon_table(), ##__VA_ARGS__)
#define TABLE_HEAD(...) _KRYON_COMP(kryon_table_head(), ##__VA_ARGS__)
#define TABLE_BODY(...) _KRYON_COMP(kryon_table_body(), ##__VA_ARGS__)
#define TABLE_ROW(...) _KRYON_COMP(kryon_table_row(), ##__VA_ARGS__)
#define TR(...) TABLE_ROW(__VA_ARGS__)  // Alias

#define TABLE_HEADER_CELL(content, ...) \
    ({ \
        IRComponent* _comp = kryon_table_header_cell(content); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })
#define TH(content, ...) TABLE_HEADER_CELL(content, ##__VA_ARGS__)  // Alias

#define TABLE_CELL(content, ...) \
    ({ \
        IRComponent* _comp = kryon_table_cell(content); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })
#define TD(content, ...) TABLE_CELL(content, ##__VA_ARGS__)  // Alias

// Table styling macros
#define CELL_PADDING(val) _set_table_cell_padding(_comp, (float)(val))
#define TABLE_STRIPED _set_table_striped(_comp, true)
#define TABLE_BORDERS _set_table_show_borders(_comp, true)
#define HEADER_BG(hex) _set_table_header_bg(_comp, (uint32_t)(hex))
#define EVEN_ROW_BG(hex) _set_table_even_row_bg(_comp, (uint32_t)(hex))
#define ODD_ROW_BG(hex) _set_table_odd_row_bg(_comp, (uint32_t)(hex))
#define TABLE_BORDER_COLOR(hex) _set_table_border_color(_comp, (uint32_t)(hex))

// ============================================================================
// Tab Components
// ============================================================================

#define TAB_GROUP(...) _KRYON_COMP(kryon_tab_group(), ##__VA_ARGS__)
#define TAB_BAR(...) _KRYON_COMP(kryon_tab_bar(), ##__VA_ARGS__)
#define TAB_CONTENT(...) _KRYON_COMP(kryon_tab_content(), ##__VA_ARGS__)
#define TAB_PANEL(...) _KRYON_COMP(kryon_tab_panel(), ##__VA_ARGS__)

#define TAB(title, ...) \
    ({ \
        IRComponent* _comp = kryon_tab(title); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

// ============================================================================
// Property Macros - Dimensions
// ============================================================================

#define WIDTH(val) (_set_width(_comp, (float)(val), "px"), 0)
#define HEIGHT(val) _set_height(_comp, (float)(val), "px")
#define WIDTH_PCT(val) _set_width(_comp, (float)(val), "%")
#define HEIGHT_PCT(val) _set_height(_comp, (float)(val), "%")
#define MIN_WIDTH(val) _set_min_width(_comp, (float)(val), "px")
#define MAX_WIDTH(val) _set_max_width(_comp, (float)(val), "px")
#define MIN_HEIGHT(val) _set_min_height(_comp, (float)(val), "px")
#define MAX_HEIGHT(val) _set_max_height(_comp, (float)(val), "px")

// ============================================================================
// Property Macros - Positioning
// ============================================================================

#define POSITION_ABSOLUTE(x, y) _set_position_absolute(_comp, (float)(x), (float)(y))
#define POS_X(val) _set_pos_x(_comp, (float)(val))
#define POS_Y(val) _set_pos_y(_comp, (float)(val))

// ============================================================================
// Property Macros - Colors
// ============================================================================

#define BG_COLOR(hex) _set_background(_comp, (uint32_t)(hex))
#define TEXT_COLOR(hex) _set_color(_comp, (uint32_t)(hex))
#define BORDER_COLOR(hex) _set_border_color(_comp, (uint32_t)(hex))

// Named color shortcuts (using existing KRYON_COLOR_* constants)
#define COLOR_WHITE _set_color(_comp, KRYON_COLOR_WHITE)
#define COLOR_BLACK _set_color(_comp, KRYON_COLOR_BLACK)
#define COLOR_RED _set_color(_comp, KRYON_COLOR_RED)
#define COLOR_GREEN _set_color(_comp, KRYON_COLOR_GREEN)
#define COLOR_BLUE _set_color(_comp, KRYON_COLOR_BLUE)
#define COLOR_YELLOW _set_color(_comp, KRYON_COLOR_YELLOW)
#define COLOR_CYAN _set_color(_comp, KRYON_COLOR_CYAN)
#define COLOR_MAGENTA _set_color(_comp, KRYON_COLOR_MAGENTA)
#define COLOR_GRAY _set_color(_comp, KRYON_COLOR_GRAY)
#define COLOR_ORANGE _set_color(_comp, KRYON_COLOR_ORANGE)
#define COLOR_PURPLE _set_color(_comp, KRYON_COLOR_PURPLE)

// Background color shortcuts
#define BG_WHITE _set_background(_comp, KRYON_COLOR_WHITE)
#define BG_BLACK _set_background(_comp, KRYON_COLOR_BLACK)
#define BG_RED _set_background(_comp, KRYON_COLOR_RED)
#define BG_GREEN _set_background(_comp, KRYON_COLOR_GREEN)
#define BG_BLUE _set_background(_comp, KRYON_COLOR_BLUE)
#define BG_YELLOW _set_background(_comp, KRYON_COLOR_YELLOW)
#define BG_CYAN _set_background(_comp, KRYON_COLOR_CYAN)
#define BG_MAGENTA _set_background(_comp, KRYON_COLOR_MAGENTA)
#define BG_GRAY _set_background(_comp, KRYON_COLOR_GRAY)
#define BG_ORANGE _set_background(_comp, KRYON_COLOR_ORANGE)
#define BG_PURPLE _set_background(_comp, KRYON_COLOR_PURPLE)

// ============================================================================
// Property Macros - Layout & Flexbox
// ============================================================================

#define JUSTIFY_CENTER _set_justify_content(_comp, IR_ALIGNMENT_CENTER)
#define JUSTIFY_START _set_justify_content(_comp, IR_ALIGNMENT_START)
#define JUSTIFY_END _set_justify_content(_comp, IR_ALIGNMENT_END)
#define JUSTIFY_BETWEEN _set_justify_content(_comp, IR_ALIGNMENT_SPACE_BETWEEN)
#define JUSTIFY_SPACE_BETWEEN JUSTIFY_BETWEEN  // Alias for codegen
#define JUSTIFY_AROUND _set_justify_content(_comp, IR_ALIGNMENT_SPACE_AROUND)
#define JUSTIFY_SPACE_AROUND JUSTIFY_AROUND  // Alias for codegen
#define JUSTIFY_EVENLY _set_justify_content(_comp, IR_ALIGNMENT_SPACE_EVENLY)

#define ALIGN_CENTER _set_align_items(_comp, IR_ALIGNMENT_CENTER)
#define ALIGN_START _set_align_items(_comp, IR_ALIGNMENT_START)
#define ALIGN_END _set_align_items(_comp, IR_ALIGNMENT_END)
#define ALIGN_STRETCH _set_align_items(_comp, IR_ALIGNMENT_STRETCH)
#define ALIGN_BASELINE _set_align_items(_comp, IR_ALIGNMENT_BASELINE)

#define GAP(val) _set_gap(_comp, (float)(val))
#define FLEX_GROW(val) _set_flex_grow(_comp, (uint8_t)(val))
#define FLEX_SHRINK(val) _set_flex_shrink(_comp, (uint8_t)(val))
#define FLEX_WRAP _set_flex_wrap(_comp, true)

// ============================================================================
// Property Macros - Spacing
// ============================================================================

#define PADDING(val) _set_padding(_comp, (float)(val))
#define MARGIN(val) _set_margin(_comp, (float)(val))
#define PADDING_TRBL(t,r,b,l) _set_padding_sides(_comp, (float)(t), (float)(r), (float)(b), (float)(l))
#define PADDING_SIDES(t,r,b,l) PADDING_TRBL(t,r,b,l)  // Alias for codegen compatibility
#define MARGIN_TRBL(t,r,b,l) _set_margin_sides(_comp, (float)(t), (float)(r), (float)(b), (float)(l))
#define MARGIN_SIDES(t,r,b,l) MARGIN_TRBL(t,r,b,l)  // Alias for codegen compatibility

// Convenient shortcuts for common patterns
#define PADDING_H(val) ({ kryon_set_padding_sides(_comp, 0, (float)(val), 0, (float)(val)); 0; })
#define PADDING_V(val) ({ kryon_set_padding_sides(_comp, (float)(val), 0, (float)(val), 0); 0; })
#define MARGIN_H(val) ({ kryon_set_margin_sides(_comp, 0, (float)(val), 0, (float)(val)); 0; })
#define MARGIN_V(val) ({ kryon_set_margin_sides(_comp, (float)(val), 0, (float)(val), 0); 0; })

// ============================================================================
// Property Macros - Typography
// ============================================================================

#define FONT_SIZE(size) _set_font_size(_comp, (float)(size))
#define FONT_FAMILY(family) _set_font_family(_comp, family)
#define FONT_WEIGHT(weight) _set_font_weight(_comp, (uint16_t)(weight))
#define FONT_BOLD _set_font_bold(_comp, true)
#define FONT_ITALIC _set_font_italic(_comp, true)
#define LINE_HEIGHT(height) _set_line_height(_comp, (float)(height))
#define LETTER_SPACING(spacing) _set_letter_spacing(_comp, (float)(spacing))

#define TEXT_ALIGN_LEFT _set_text_align(_comp, IR_TEXT_ALIGN_LEFT)
#define TEXT_ALIGN_CENTER _set_text_align(_comp, IR_TEXT_ALIGN_CENTER)
#define TEXT_ALIGN_RIGHT _set_text_align(_comp, IR_TEXT_ALIGN_RIGHT)
#define TEXT_ALIGN_JUSTIFY _set_text_align(_comp, IR_TEXT_ALIGN_JUSTIFY)

// Font weight shortcuts
#define FONT_THIN _set_font_weight(_comp, 100)
#define FONT_EXTRA_LIGHT _set_font_weight(_comp, 200)
#define FONT_LIGHT _set_font_weight(_comp, 300)
#define FONT_NORMAL _set_font_weight(_comp, 400)
#define FONT_MEDIUM _set_font_weight(_comp, 500)
#define FONT_SEMI_BOLD _set_font_weight(_comp, 600)
#define FONT_EXTRA_BOLD _set_font_weight(_comp, 800)
#define FONT_BLACK _set_font_weight(_comp, 900)

// ============================================================================
// Property Macros - Border & Effects
// ============================================================================

#define BORDER_WIDTH(width) _set_border_width(_comp, (float)(width))
#define BORDER_RADIUS(radius) _set_border_radius(_comp, (float)(radius))
#define OPACITY(val) _set_opacity(_comp, (float)(val))
#define Z_INDEX(val) _set_z_index(_comp, (int32_t)(val))
#define VISIBLE(val) _set_visible(_comp, (bool)(val))
#define HIDDEN _set_visible(_comp, false)

// ============================================================================
// Property Macros - Event Handlers
// ============================================================================

#define ON_CLICK(handler) _on_click(_comp, handler, #handler)
#define ON_CHANGE(handler) _on_change(_comp, handler, #handler)
#define ON_HOVER(handler) _on_hover(_comp, handler, #handler)
#define ON_FOCUS(handler) _on_focus(_comp, handler, #handler)

// ============================================================================
// Property Macros - Data Binding
// ============================================================================

/**
 * BIND - Generic property binding
 * Binds a property to a variable expression (currently just stores the expression)
 * In future, this could enable reactive updates
 * Uses statement expression (GCC extension) to work in __VA_ARGS__ context
 */
#define BIND(prop, expr) \
    ((void)_comp, (void)0)

/**
 * OPTIONS - Set dropdown options from string array
 * Uses compound literal (C99) + helper function
 */
#define OPTIONS(count, ...) \
    ((void)_set_dropdown_options(_comp, (const char*[]){__VA_ARGS__}, (uint32_t)(count)), (void)0)

/**
 * SELECTED_INDEX - Set selected index for dropdown/tab components
 * Uses comma operator for side effect in __VA_ARGS__ context
 */
#define SELECTED_INDEX(expr) \
    ((void)_set_dropdown_selected_index(_comp, (int32_t)(expr)), (void)0)

// ============================================================================
// Iteration Macros
// ============================================================================

/**
 * FOR_EACH - Runtime iteration for dynamic component generation
 *
 * Works inside other component macros to generate children dynamically.
 * Uses the parent stack mechanism - each iteration's components are added
 * to the current parent via _kryon_add_to_parent().
 *
 * @param item_var - Loop variable name (will be declared with inferred type)
 * @param array    - Array to iterate over
 * @param count    - Number of elements in array
 * @param body     - Component macro(s) to generate for each item
 *
 * Example:
 *   const char* tabs[] = {"Home", "Settings", "Profile"};
 *   TAB_BAR(
 *       FOR_EACH(tab, tabs, 3, TAB(tab))
 *   )
 *
 * Example with structs:
 *   typedef struct { const char* name; bool done; } Task;
 *   Task tasks[] = {{"Buy milk", false}, {"Walk dog", true}};
 *   COLUMN(
 *       FOR_EACH(t, tasks, 2,
 *           ROW(
 *               CHECKBOX(t.name, t.done),
 *               TEXT(t.name)
 *           )
 *       )
 *   )
 */
#define FOR_EACH(item_var, array, count, body) \
    ({ \
        for (int _for_idx = 0; _for_idx < (count); _for_idx++) { \
            typeof((array)[0]) item_var = (array)[_for_idx]; \
            body; \
        } \
        (void)0; \
    })

/**
 * FOR_EACH_TYPED - Typed iteration for void* arrays
 * Use when the array type cannot be inferred (e.g., function result arrays)
 *
 * @param item_type  The type of each element (e.g., HabitItem*)
 * @param item_var   Variable name for the current item
 * @param array      The array to iterate (can be void*)
 * @param count      Number of elements
 * @param body       Code to execute for each element
 */
#define FOR_EACH_TYPED(item_type, item_var, array, count, body) \
    ({ \
        for (int _for_idx = 0; _for_idx < (count); _for_idx++) { \
            item_type item_var = ((item_type*)(array))[_for_idx]; \
            body; \
        } \
        (void)0; \
    })

// ============================================================================
// Application Lifecycle - Shutdown API Macros
// ============================================================================

/**
 * KRYON_ON_CLEANUP - Set cleanup callback
 * The cleanup callback is called after the main loop exits but before the
 * window is closed. Use this to free GPU resources (textures, meshes, etc.)
 *
 * Usage:
 *   void my_cleanup(void* user_data) {
 *       // Free textures, meshes, shaders, etc.
 *   }
 *   KRYON_ON_CLEANUP(my_cleanup, NULL);
 */
#define KRYON_ON_CLEANUP(callback, user_data) \
    kryon_set_cleanup_callback(NULL, callback, user_data)

/**
 * KRYON_ON_SHUTDOWN - Register shutdown callback with priority
 * Callbacks are called in priority order (highest first) when shutdown is requested.
 * Return false from the callback to veto user-initiated shutdown.
 *
 * Usage:
 *   bool confirm_exit(kryon_shutdown_reason_t reason, void* user_data) {
 *       if (reason == KRYON_SHUTDOWN_REASON_USER) {
 *           return ask_user_to_confirm();  // Return false to cancel
 *       }
 *       return true;  // Proceed with shutdown
 *   }
 *   KRYON_ON_SHUTDOWN(confirm_exit, NULL, 100);  // Priority 100
 */
#define KRYON_ON_SHUTDOWN(callback, user_data, priority) \
    kryon_register_shutdown_callback(NULL, callback, user_data, priority)

/**
 * KRYON_SHUTDOWN - Request application shutdown
 * Initiates the graceful shutdown sequence.
 *
 * Usage:
 *   void on_quit_button(void) {
 *       KRYON_SHUTDOWN(KRYON_SHUTDOWN_REASON_USER);
 *   }
 */
#define KRYON_SHUTDOWN(reason) \
    kryon_request_shutdown(NULL, reason)

/**
 * KRYON_IS_SHUTTING_DOWN - Check if shutdown is in progress
 * Returns true if the application is shutting down.
 */
#define KRYON_IS_SHUTTING_DOWN() \
    kryon_is_shutting_down(NULL)

/**
 * KRYON_GET_SHUTDOWN_STATE - Get current shutdown state
 * Returns the current kryon_shutdown_state_t value.
 */
#define KRYON_GET_SHUTDOWN_STATE() \
    kryon_get_shutdown_state(NULL)

// ============================================================================
// Utility Macros
// ============================================================================

/**
 * FLEX_CENTER - Shortcut for centering content both horizontally and vertically
 */
#define FLEX_CENTER JUSTIFY_CENTER, ALIGN_CENTER

/**
 * FLEX_ROW_CENTER - Center items in a row
 */
#define FLEX_ROW_CENTER JUSTIFY_CENTER, ALIGN_CENTER

/**
 * FLEX_COLUMN_CENTER - Center items in a column
 */
#define FLEX_COLUMN_CENTER JUSTIFY_CENTER, ALIGN_CENTER

/**
 * FULL_SIZE - Make component fill parent (both dimensions)
 */
#define FULL_SIZE WIDTH_PCT(100), HEIGHT_PCT(100)

/**
 * FULL_WIDTH - Make component fill parent width
 */
#define FULL_WIDTH WIDTH_PCT(100)

/**
 * FULL_HEIGHT - Make component fill parent height
 */
#define FULL_HEIGHT HEIGHT_PCT(100)

/**
 * SIZE - Set both width and height in pixels
 */
#define SIZE(val) WIDTH(val), HEIGHT(val)

/**
 * SQUARE - Create square component
 */
#define SQUARE(val) WIDTH(val), HEIGHT(val)

/**
 * NAMED - Assign component to variable and automatically set its name for KIR output
 * Usage: NAMED(my_screen, COLUMN(...))
 * This makes the KIR file show: "name": "my_screen"
 */
#define NAMED(var, expr) \
    ({ \
        var = expr; \
        if (var) { \
            kryon_set_tag(var, #var); \
        } \
        var; \
    })

// ============================================================================
// Desktop Renderer Macros (DEPRECATED - Desktop rendering removed)
// ============================================================================

#ifndef KRYON_KIR_ONLY
// Desktop rendering has been removed - see KRYON_RUN implementation
#endif

/**
 * KRYON_RUN - Run the application
 * Automatically handles both KIR generation (kryon run) and standalone execution
 * Uses window settings from kryon_init()
 */
#define KRYON_RUN() \
    ({ \
        _kryon_run_impl(); \
    })

// Implementation function (don't call directly, use KRYON_RUN macro)
static inline int _kryon_run_impl(void) {
    IRComponent* root = kryon_get_root();
    if (!root) {
        fprintf(stderr, "Failed to get root component\n");
        kryon_cleanup();
        return 1;
    }

#ifdef KRYON_KIR_ONLY
    // For kryon run - just generate KIR and exit
    kryon_finalize("output.kir");
    kryon_cleanup();
    return 0;
#else
    // Desktop rendering has been removed
    // Use the Web or DIS targets instead:
    // - Web: kryon build web --input app.kry --output app.html
    // - DIS: kryon build dis --input app.kry --output app.dis
    fprintf(stderr, "Error: Desktop rendering is no longer supported.\n");
    fprintf(stderr, "Please use one of these targets instead:\n");
    fprintf(stderr, "  - Web: kryon build web --input app.kry --output app.html\n");
    fprintf(stderr, "  - DIS: kryon build dis --input app.kry --output app.dis\n");
    fprintf(stderr, "Then run: kryon run web|dis\n");
    kryon_cleanup();
    return 1;
#endif
}

#ifdef __cplusplus
}
#endif

#endif // KRYON_DSL_H
