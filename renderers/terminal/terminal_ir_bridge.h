/**
 * Kryon Terminal Renderer - IR Bridge
 *
 * CRITICAL: This module is completely SOURCE-LANGUAGE AGNOSTIC.
 * It works ONLY with IR (Intermediate Representation) types.
 *
 * Supports ANY frontend: TSX, Kry, HTML, Markdown, Lua, C, etc.
 * As long as source compiles to KIR, this renderer works.
 */

#ifndef KRYON_TERMINAL_IR_BRIDGE_H
#define KRYON_TERMINAL_IR_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

// ONLY IR headers - NO parser-specific headers
#include "../../ir/include/ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct TickitRenderBuffer TickitRenderBuffer;
typedef struct TickitPen TickitPen;

// ============================================================================
// Terminal Rendering Context
// ============================================================================

/**
 * Terminal rendering context (passed to all rendering functions)
 */
typedef struct {
    // Terminal capabilities
    bool unicode_support;
    uint8_t color_mode;          // 4=16-color, 8=256-color, 24=truecolor
    uint16_t term_width;         // Terminal width in characters
    uint16_t term_height;        // Terminal height in characters

    // Character cell dimensions (for pixel → char conversion)
    uint8_t char_width;          // Pixels per character (default 8)
    uint8_t char_height;         // Pixels per character (default 12)

    // Rendering backend (libtickit or ANSI)
    TickitRenderBuffer* buffer;  // libtickit render buffer (if available)
    TickitPen* pen;              // libtickit pen (if available)
    bool use_libtickit;          // true = libtickit, false = ANSI codes

    // Rendering state
    bool needs_redraw;
    uint32_t frame_count;

} TerminalRenderContext;

// ============================================================================
// IR Component Tree Rendering (Source-Agnostic)
// ============================================================================

/**
 * Render entire IR component tree to terminal
 *
 * CRITICAL: Works with IRComponent* from ANY source (TSX, Kry, HTML, etc.)
 *
 * @param ctx Terminal rendering context
 * @param root IR component tree root
 * @return true on success, false on failure
 */
bool terminal_render_ir_tree(TerminalRenderContext* ctx, IRComponent* root);

/**
 * Render single IR component (dispatches to type-specific renderers)
 *
 * @param ctx Terminal rendering context
 * @param component IR component to render
 * @return true on success, false on failure
 */
bool terminal_render_ir_component(TerminalRenderContext* ctx, IRComponent* component);

// ============================================================================
// Coordinate Conversion (Pixel ↔ Character)
// ============================================================================

/**
 * Convert IR pixel coordinates to terminal character coordinates
 *
 * IR layout system uses pixels, terminal uses character cells.
 * This converts between them.
 *
 * @param ctx Terminal rendering context
 * @param pixel_x Pixel X coordinate (from IR layout)
 * @param pixel_y Pixel Y coordinate (from IR layout)
 * @param char_x Output character X coordinate
 * @param char_y Output character Y coordinate
 */
void terminal_pixel_to_char_coords(
    const TerminalRenderContext* ctx,
    float pixel_x,
    float pixel_y,
    int* char_x,
    int* char_y
);

/**
 * Convert terminal character coordinates to IR pixel coordinates
 *
 * @param ctx Terminal rendering context
 * @param char_x Character X coordinate
 * @param char_y Character Y coordinate
 * @param pixel_x Output pixel X coordinate
 * @param pixel_y Output pixel Y coordinate
 */
void terminal_char_to_pixel_coords(
    const TerminalRenderContext* ctx,
    int char_x,
    int char_y,
    float* pixel_x,
    float* pixel_y
);

/**
 * Convert IR pixel dimensions to terminal character dimensions
 *
 * @param ctx Terminal rendering context
 * @param pixel_width Pixel width (from IR layout)
 * @param pixel_height Pixel height (from IR layout)
 * @param char_width Output character width
 * @param char_height Output character height
 */
void terminal_pixel_to_char_size(
    const TerminalRenderContext* ctx,
    float pixel_width,
    float pixel_height,
    int* char_width,
    int* char_height
);

// ============================================================================
// Component-Specific Renderers (IR-Based)
// ============================================================================

/**
 * Render Text component
 * Works with Text from ANY source (TSX <Text>, Kry Text, HTML <p>, etc.)
 */
bool terminal_render_text(TerminalRenderContext* ctx, IRComponent* component);

/**
 * Render Button component
 * Works with Button from ANY source (TSX <Button>, Kry Button, HTML <button>, etc.)
 */
bool terminal_render_button(TerminalRenderContext* ctx, IRComponent* component);

/**
 * Render Container/Row/Column component
 * Works with containers from ANY source (TSX <Container>, Kry Container, HTML <div>, etc.)
 */
bool terminal_render_container(TerminalRenderContext* ctx, IRComponent* component);

/**
 * Render Input component
 * Works with Input from ANY source (TSX <Input>, Kry Input, HTML <input>, etc.)
 */
bool terminal_render_input(TerminalRenderContext* ctx, IRComponent* component);

/**
 * Render Checkbox component
 * Works with Checkbox from ANY source
 */
bool terminal_render_checkbox(TerminalRenderContext* ctx, IRComponent* component);

/**
 * Render Dropdown component
 * Works with Dropdown from ANY source
 */
bool terminal_render_dropdown(TerminalRenderContext* ctx, IRComponent* component);

// ============================================================================
// Color Conversion (IR Color → Terminal Color)
// ============================================================================

/**
 * Convert IR color (0xRRGGBBAA) to terminal color code
 *
 * @param color IR color value
 * @param color_mode Terminal color mode (4/8/24)
 * @param r Output red component (for truecolor)
 * @param g Output green component (for truecolor)
 * @param b Output blue component (for truecolor)
 * @return Terminal color code (for 16/256-color modes)
 */
int terminal_ir_color_to_terminal(uint32_t color, uint8_t color_mode, uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * Set foreground color using ANSI escape codes
 *
 * @param color IR color value
 * @param color_mode Terminal color mode
 */
void terminal_set_fg_color(uint32_t color, uint8_t color_mode);

/**
 * Set background color using ANSI escape codes
 *
 * @param color IR color value
 * @param color_mode Terminal color mode
 */
void terminal_set_bg_color(uint32_t color, uint8_t color_mode);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Move cursor to character position
 *
 * @param char_x Character X coordinate
 * @param char_y Character Y coordinate
 */
void terminal_move_cursor(int char_x, int char_y);

/**
 * Clear rectangular region
 *
 * @param char_x Start X coordinate
 * @param char_y Start Y coordinate
 * @param char_width Width in characters
 * @param char_height Height in characters
 */
void terminal_clear_region(int char_x, int char_y, int char_width, int char_height);

/**
 * Draw horizontal line
 *
 * @param char_x Start X coordinate
 * @param char_y Y coordinate
 * @param length Length in characters
 * @param line_char Line character (e.g., "─" or "-")
 */
void terminal_draw_hline(int char_x, int char_y, int length, const char* line_char);

/**
 * Draw vertical line
 *
 * @param char_x X coordinate
 * @param char_y Start Y coordinate
 * @param length Length in characters
 * @param line_char Line character (e.g., "│" or "|")
 */
void terminal_draw_vline(int char_x, int char_y, int length, const char* line_char);

/**
 * Draw box border
 *
 * @param char_x Start X coordinate
 * @param char_y Start Y coordinate
 * @param char_width Width in characters
 * @param char_height Height in characters
 * @param unicode_support Whether to use Unicode box drawing chars
 */
void terminal_draw_box(int char_x, int char_y, int char_width, int char_height, bool unicode_support);

#ifdef __cplusplus
}
#endif

#endif // KRYON_TERMINAL_IR_BRIDGE_H
