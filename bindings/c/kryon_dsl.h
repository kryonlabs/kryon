/**
 * Kryon UI Framework - C DSL Frontend
 *
 * Beautiful declarative macro-based DSL for Kryon in C.
 * Provides a concise syntax similar to Nim DSL and .kry files.
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
        (void)0, ##__VA_ARGS__; \
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
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

#define IMAGE(src, alt, ...) \
    ({ \
        IRComponent* _comp = kryon_image(src, alt); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

// ============================================================================
// Table Components
// ============================================================================

#define TABLE(...) _KRYON_COMP(kryon_table(), ##__VA_ARGS__)
#define TABLE_HEAD(...) _KRYON_COMP(kryon_table_head(), ##__VA_ARGS__)
#define TABLE_BODY(...) _KRYON_COMP(kryon_table_body(), ##__VA_ARGS__)
#define TR(...) _KRYON_COMP(kryon_table_row(), ##__VA_ARGS__)

#define TH(content, ...) \
    ({ \
        IRComponent* _comp = kryon_table_header_cell(content); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

#define TD(content, ...) \
    ({ \
        IRComponent* _comp = kryon_table_cell(content); \
        _kryon_add_to_parent(_comp); \
        (void)0, ##__VA_ARGS__; \
        _comp; \
    })

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

#define WIDTH(val) kryon_set_width(_comp, (float)(val), "px")
#define HEIGHT(val) kryon_set_height(_comp, (float)(val), "px")
#define WIDTH_PCT(val) kryon_set_width(_comp, (float)(val), "%")
#define HEIGHT_PCT(val) kryon_set_height(_comp, (float)(val), "%")
#define MIN_WIDTH(val) kryon_set_min_width(_comp, (float)(val), "px")
#define MAX_WIDTH(val) kryon_set_max_width(_comp, (float)(val), "px")
#define MIN_HEIGHT(val) kryon_set_min_height(_comp, (float)(val), "px")
#define MAX_HEIGHT(val) kryon_set_max_height(_comp, (float)(val), "px")

// ============================================================================
// Property Macros - Colors
// ============================================================================

#define BG_COLOR(hex) kryon_set_background(_comp, (uint32_t)(hex))
#define TEXT_COLOR(hex) kryon_set_color(_comp, (uint32_t)(hex))
#define BORDER_COLOR(hex) kryon_set_border_color(_comp, (uint32_t)(hex))

// Named color shortcuts (using existing KRYON_COLOR_* constants)
#define COLOR_WHITE kryon_set_color(_comp, KRYON_COLOR_WHITE)
#define COLOR_BLACK kryon_set_color(_comp, KRYON_COLOR_BLACK)
#define COLOR_RED kryon_set_color(_comp, KRYON_COLOR_RED)
#define COLOR_GREEN kryon_set_color(_comp, KRYON_COLOR_GREEN)
#define COLOR_BLUE kryon_set_color(_comp, KRYON_COLOR_BLUE)
#define COLOR_YELLOW kryon_set_color(_comp, KRYON_COLOR_YELLOW)
#define COLOR_CYAN kryon_set_color(_comp, KRYON_COLOR_CYAN)
#define COLOR_MAGENTA kryon_set_color(_comp, KRYON_COLOR_MAGENTA)
#define COLOR_GRAY kryon_set_color(_comp, KRYON_COLOR_GRAY)
#define COLOR_ORANGE kryon_set_color(_comp, KRYON_COLOR_ORANGE)
#define COLOR_PURPLE kryon_set_color(_comp, KRYON_COLOR_PURPLE)

// Background color shortcuts
#define BG_WHITE kryon_set_background(_comp, KRYON_COLOR_WHITE)
#define BG_BLACK kryon_set_background(_comp, KRYON_COLOR_BLACK)
#define BG_RED kryon_set_background(_comp, KRYON_COLOR_RED)
#define BG_GREEN kryon_set_background(_comp, KRYON_COLOR_GREEN)
#define BG_BLUE kryon_set_background(_comp, KRYON_COLOR_BLUE)
#define BG_YELLOW kryon_set_background(_comp, KRYON_COLOR_YELLOW)
#define BG_CYAN kryon_set_background(_comp, KRYON_COLOR_CYAN)
#define BG_MAGENTA kryon_set_background(_comp, KRYON_COLOR_MAGENTA)
#define BG_GRAY kryon_set_background(_comp, KRYON_COLOR_GRAY)
#define BG_ORANGE kryon_set_background(_comp, KRYON_COLOR_ORANGE)
#define BG_PURPLE kryon_set_background(_comp, KRYON_COLOR_PURPLE)

// ============================================================================
// Property Macros - Layout & Flexbox
// ============================================================================

#define JUSTIFY_CENTER kryon_set_justify_content(_comp, IR_ALIGNMENT_CENTER)
#define JUSTIFY_START kryon_set_justify_content(_comp, IR_ALIGNMENT_START)
#define JUSTIFY_END kryon_set_justify_content(_comp, IR_ALIGNMENT_END)
#define JUSTIFY_BETWEEN kryon_set_justify_content(_comp, IR_ALIGNMENT_SPACE_BETWEEN)
#define JUSTIFY_AROUND kryon_set_justify_content(_comp, IR_ALIGNMENT_SPACE_AROUND)
#define JUSTIFY_EVENLY kryon_set_justify_content(_comp, IR_ALIGNMENT_SPACE_EVENLY)

#define ALIGN_CENTER kryon_set_align_items(_comp, IR_ALIGNMENT_CENTER)
#define ALIGN_START kryon_set_align_items(_comp, IR_ALIGNMENT_START)
#define ALIGN_END kryon_set_align_items(_comp, IR_ALIGNMENT_END)
#define ALIGN_STRETCH kryon_set_align_items(_comp, IR_ALIGNMENT_STRETCH)
#define ALIGN_BASELINE kryon_set_align_items(_comp, IR_ALIGNMENT_BASELINE)

#define GAP(val) kryon_set_gap(_comp, (float)(val))
#define FLEX_GROW(val) kryon_set_flex_grow(_comp, (uint8_t)(val))
#define FLEX_SHRINK(val) kryon_set_flex_shrink(_comp, (uint8_t)(val))
#define FLEX_WRAP kryon_set_flex_wrap(_comp, true)

// ============================================================================
// Property Macros - Spacing
// ============================================================================

#define PADDING(val) kryon_set_padding(_comp, (float)(val))
#define MARGIN(val) kryon_set_margin(_comp, (float)(val))
#define PADDING_TRBL(t,r,b,l) kryon_set_padding_sides(_comp, (float)(t), (float)(r), (float)(b), (float)(l))
#define MARGIN_TRBL(t,r,b,l) kryon_set_margin_sides(_comp, (float)(t), (float)(r), (float)(b), (float)(l))

// Convenient shortcuts for common patterns
#define PADDING_H(val) kryon_set_padding_sides(_comp, 0, (float)(val), 0, (float)(val))
#define PADDING_V(val) kryon_set_padding_sides(_comp, (float)(val), 0, (float)(val), 0)
#define MARGIN_H(val) kryon_set_margin_sides(_comp, 0, (float)(val), 0, (float)(val))
#define MARGIN_V(val) kryon_set_margin_sides(_comp, (float)(val), 0, (float)(val), 0)

// ============================================================================
// Property Macros - Typography
// ============================================================================

#define FONT_SIZE(size) kryon_set_font_size(_comp, (float)(size))
#define FONT_FAMILY(family) kryon_set_font_family(_comp, family)
#define FONT_WEIGHT(weight) kryon_set_font_weight(_comp, (uint16_t)(weight))
#define FONT_BOLD kryon_set_font_bold(_comp, true)
#define FONT_ITALIC kryon_set_font_italic(_comp, true)
#define LINE_HEIGHT(height) kryon_set_line_height(_comp, (float)(height))
#define LETTER_SPACING(spacing) kryon_set_letter_spacing(_comp, (float)(spacing))

#define TEXT_ALIGN_LEFT kryon_set_text_align(_comp, IR_TEXT_ALIGN_LEFT)
#define TEXT_ALIGN_CENTER kryon_set_text_align(_comp, IR_TEXT_ALIGN_CENTER)
#define TEXT_ALIGN_RIGHT kryon_set_text_align(_comp, IR_TEXT_ALIGN_RIGHT)
#define TEXT_ALIGN_JUSTIFY kryon_set_text_align(_comp, IR_TEXT_ALIGN_JUSTIFY)

// Font weight shortcuts
#define FONT_THIN kryon_set_font_weight(_comp, 100)
#define FONT_EXTRA_LIGHT kryon_set_font_weight(_comp, 200)
#define FONT_LIGHT kryon_set_font_weight(_comp, 300)
#define FONT_NORMAL kryon_set_font_weight(_comp, 400)
#define FONT_MEDIUM kryon_set_font_weight(_comp, 500)
#define FONT_SEMI_BOLD kryon_set_font_weight(_comp, 600)
#define FONT_EXTRA_BOLD kryon_set_font_weight(_comp, 800)
#define FONT_BLACK kryon_set_font_weight(_comp, 900)

// ============================================================================
// Property Macros - Border & Effects
// ============================================================================

#define BORDER_WIDTH(width) kryon_set_border_width(_comp, (float)(width))
#define BORDER_RADIUS(radius) kryon_set_border_radius(_comp, (float)(radius))
#define OPACITY(val) kryon_set_opacity(_comp, (float)(val))
#define Z_INDEX(val) kryon_set_z_index(_comp, (int32_t)(val))
#define VISIBLE(val) kryon_set_visible(_comp, (bool)(val))
#define HIDDEN kryon_set_visible(_comp, false)

// ============================================================================
// Property Macros - Event Handlers
// ============================================================================

#define ON_CLICK(handler) kryon_on_click(_comp, handler, #handler)
#define ON_CHANGE(handler) kryon_on_change(_comp, handler, #handler)
#define ON_HOVER(handler) kryon_on_hover(_comp, handler, #handler)
#define ON_FOCUS(handler) kryon_on_focus(_comp, handler, #handler)

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
 * FULL_SIZE - Make component fill parent
 */
#define FULL_SIZE WIDTH_PCT(100), HEIGHT_PCT(100)

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
// Desktop Renderer Macros
// ============================================================================

#ifndef KRYON_KIR_ONLY
// Include desktop renderer header for standalone builds
#include "../../backends/desktop/ir_desktop_renderer.h"
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
    // For standalone builds - run with desktop renderer using kryon_init() values
    extern KryonAppState g_app_state;
    DesktopRendererConfig config = {
        .backend_type = DESKTOP_BACKEND_SDL3,
        .window_width = g_app_state.window_width > 0 ? g_app_state.window_width : 800,
        .window_height = g_app_state.window_height > 0 ? g_app_state.window_height : 600,
        .window_title = g_app_state.window_title ? g_app_state.window_title : "Kryon App",
        .resizable = true,
        .fullscreen = false,
        .vsync_enabled = true,
        .target_fps = 60
    };

    DesktopIRRenderer* renderer = desktop_ir_renderer_create(&config);
    if (!renderer) {
        fprintf(stderr, "Failed to create desktop renderer\n");
        kryon_cleanup();
        return 1;
    }

    if (!desktop_ir_renderer_initialize(renderer)) {
        fprintf(stderr, "Failed to initialize desktop renderer\n");
        desktop_ir_renderer_destroy(renderer);
        kryon_cleanup();
        return 1;
    }

    desktop_ir_renderer_run_main_loop(renderer, root);
    desktop_ir_renderer_destroy(renderer);
    kryon_cleanup();
    return 0;
#endif
}

#ifdef __cplusplus
}
#endif

#endif // KRYON_DSL_H
