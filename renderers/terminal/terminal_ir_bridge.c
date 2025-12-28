/**
 * Kryon Terminal Renderer - IR Bridge Implementation
 *
 * CRITICAL: This module is completely SOURCE-LANGUAGE AGNOSTIC.
 * Works ONLY with IR types from ANY frontend (TSX, Kry, HTML, etc.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "terminal_ir_bridge.h"
#include "terminal_box_drawing.h"
#include "../../ir/ir_core.h"

// ============================================================================
// Coordinate Conversion
// ============================================================================

void terminal_pixel_to_char_coords(
    const TerminalRenderContext* ctx,
    float pixel_x,
    float pixel_y,
    int* char_x,
    int* char_y
) {
    if (!ctx || !char_x || !char_y) return;

    *char_x = (int)(pixel_x / ctx->char_width);
    *char_y = (int)(pixel_y / ctx->char_height);
}

void terminal_char_to_pixel_coords(
    const TerminalRenderContext* ctx,
    int char_x,
    int char_y,
    float* pixel_x,
    float* pixel_y
) {
    if (!ctx || !pixel_x || !pixel_y) return;

    *pixel_x = char_x * ctx->char_width;
    *pixel_y = char_y * ctx->char_height;
}

void terminal_pixel_to_char_size(
    const TerminalRenderContext* ctx,
    float pixel_width,
    float pixel_height,
    int* char_width,
    int* char_height
) {
    if (!ctx || !char_width || !char_height) return;

    *char_width = (int)((pixel_width + ctx->char_width - 1) / ctx->char_width);
    *char_height = (int)((pixel_height + ctx->char_height - 1) / ctx->char_height);
}

// ============================================================================
// Color Conversion
// ============================================================================

int terminal_ir_color_to_terminal(uint32_t color, uint8_t color_mode, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!r || !g || !b) return 0;

    *r = (color >> 24) & 0xFF;
    *g = (color >> 16) & 0xFF;
    *b = (color >> 8) & 0xFF;

    if (color_mode == 24) {
        return -1;  // Use RGB
    } else if (color_mode == 8) {
        // Convert to 256-color palette (simplified)
        return 16 + ((*r / 51) * 36) + ((*g / 51) * 6) + (*b / 51);
    } else {
        // Convert to 16-color palette (simplified)
        int bright = (*r + *g + *b) > 384 ? 8 : 0;
        int red = (*r > 128) ? 1 : 0;
        int green = (*g > 128) ? 2 : 0;
        int blue = (*b > 128) ? 4 : 0;
        return bright + red + green + blue;
    }
}

void terminal_set_fg_color(uint32_t color, uint8_t color_mode) {
    uint8_t r, g, b;
    int code = terminal_ir_color_to_terminal(color, color_mode, &r, &g, &b);

    if (code == -1) {
        // Truecolor
        printf("\033[38;2;%d;%d;%dm", r, g, b);
    } else if (color_mode == 8) {
        // 256-color
        printf("\033[38;5;%dm", code);
    } else {
        // 16-color
        printf("\033[%dm", 30 + (code % 8));
    }
}

void terminal_set_bg_color(uint32_t color, uint8_t color_mode) {
    uint8_t r, g, b;
    int code = terminal_ir_color_to_terminal(color, color_mode, &r, &g, &b);

    if (code == -1) {
        // Truecolor
        printf("\033[48;2;%d;%d;%dm", r, g, b);
    } else if (color_mode == 8) {
        // 256-color
        printf("\033[48;5;%dm", code);
    } else {
        // 16-color
        printf("\033[%dm", 40 + (code % 8));
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

void terminal_move_cursor(int char_x, int char_y) {
    printf("\033[%d;%dH", char_y + 1, char_x + 1);
}

void terminal_clear_region(int char_x, int char_y, int char_width, int char_height) {
    for (int y = char_y; y < char_y + char_height && y < 1000; y++) {
        terminal_move_cursor(char_x, y);
        for (int x = 0; x < char_width; x++) {
            printf(" ");
        }
    }
}

void terminal_draw_hline(int char_x, int char_y, int length, const char* line_char) {
    terminal_move_cursor(char_x, char_y);
    for (int i = 0; i < length; i++) {
        printf("%s", line_char);
    }
}

void terminal_draw_vline(int char_x, int char_y, int length, const char* line_char) {
    for (int i = 0; i < length; i++) {
        terminal_move_cursor(char_x, char_y + i);
        printf("%s", line_char);
    }
}

void terminal_draw_box(int char_x, int char_y, int char_width, int char_height, bool unicode_support) {
    BoxChars box = terminal_get_box_chars(BOX_STYLE_SINGLE, unicode_support);

    // Top edge
    terminal_move_cursor(char_x, char_y);
    printf("%s", box.top_left);
    for (int i = 1; i < char_width - 1; i++) printf("%s", box.horizontal);
    printf("%s", box.top_right);

    // Sides
    for (int y = 1; y < char_height - 1; y++) {
        terminal_move_cursor(char_x, char_y + y);
        printf("%s", box.vertical);
        terminal_move_cursor(char_x + char_width - 1, char_y + y);
        printf("%s", box.vertical);
    }

    // Bottom edge
    terminal_move_cursor(char_x, char_y + char_height - 1);
    printf("%s", box.bottom_left);
    for (int i = 1; i < char_width - 1; i++) printf("%s", box.horizontal);
    printf("%s", box.bottom_right);
}

// ============================================================================
// Component Renderers (IR-Based, Source-Agnostic) - STUBS FOR NOW
// ============================================================================

bool terminal_render_text(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // TODO: Implement using component->text_content and component->style->font.color
    // For now, just return success to allow compilation
    (void)ctx;
    (void)component;
    return true;
}

bool terminal_render_button(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // TODO: Implement button rendering
    (void)ctx;
    (void)component;
    return true;
}

bool terminal_render_container(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // Render children
    for (uint32_t i = 0; i < component->child_count; i++) {
        terminal_render_ir_component(ctx, component->children[i]);
    }

    return true;
}

bool terminal_render_input(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // TODO: Implement input rendering
    (void)ctx;
    (void)component;
    return true;
}

bool terminal_render_checkbox(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // TODO: Implement checkbox rendering
    (void)ctx;
    (void)component;
    return true;
}

bool terminal_render_dropdown(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // TODO: Implement dropdown rendering
    (void)ctx;
    (void)component;
    return true;
}

// ============================================================================
// IR Component Tree Rendering
// ============================================================================

bool terminal_render_ir_component(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // Dispatch to component-specific renderer based on IR type
    switch (component->type) {
        case IR_COMPONENT_TEXT:
            return terminal_render_text(ctx, component);

        case IR_COMPONENT_BUTTON:
            return terminal_render_button(ctx, component);

        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
        case IR_COMPONENT_CENTER:
            return terminal_render_container(ctx, component);

        case IR_COMPONENT_INPUT:
            return terminal_render_input(ctx, component);

        case IR_COMPONENT_CHECKBOX:
            return terminal_render_checkbox(ctx, component);

        case IR_COMPONENT_DROPDOWN:
            return terminal_render_dropdown(ctx, component);

        default:
            // Unsupported component type - try rendering children
            for (uint32_t i = 0; i < component->child_count; i++) {
                terminal_render_ir_component(ctx, component->children[i]);
            }
            return true;
    }
}

bool terminal_render_ir_tree(TerminalRenderContext* ctx, IRComponent* root) {
    if (!ctx || !root) return false;

    // Clear screen
    printf("\033[2J\033[H");

    // Render component tree recursively
    return terminal_render_ir_component(ctx, root);
}
