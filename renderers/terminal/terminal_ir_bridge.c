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
#include "../../ir/include/ir_core.h"

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

    // Get text content
    const char* text = component->text_content;
    if (!text || !text[0]) return true;  // Empty text is ok

    // Get computed layout bounds
    IRComputedLayout* bounds = ir_layout_get_bounds(component);
    if (!bounds || !bounds->valid) {
        return false;  // Layout not computed
    }

    // Convert pixel position to character position
    int char_x, char_y;
    terminal_pixel_to_char_coords(ctx, bounds->x, bounds->y, &char_x, &char_y);

    // Bounds check
    if (char_x >= ctx->term_width || char_y >= ctx->term_height) {
        return true;  // Off-screen is ok
    }
    if (char_x < 0) char_x = 0;
    if (char_y < 0) char_y = 0;

    // Get text color from style
    uint32_t color = 0xFFFFFFFF;  // Default white
    if (component->style && component->style->font.color.type == IR_COLOR_SOLID) {
        IRColor c = component->style->font.color;
        color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
    }

    // Apply color
    terminal_set_fg_color(color, ctx->color_mode);

    // Position cursor and render text
    terminal_move_cursor(char_x, char_y);

    // Calculate available width for text
    int char_width = (int)(bounds->width / ctx->char_width);

    // Render text (with truncation if too long)
    int len = strlen(text);
    if (len > char_width) {
        // Truncate with ellipsis
        printf("%.*s...", char_width - 3, text);
    } else {
        printf("%s", text);
    }

    // Reset color
    printf("\033[0m");

    return true;
}

bool terminal_render_button(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // Get button text (from text_content or from children)
    const char* text = component->text_content;
    if (!text || !text[0]) {
        // Try to get text from first child if it's a Text component
        if (component->child_count > 0 &&
            component->children[0]->type == IR_COMPONENT_TEXT) {
            text = component->children[0]->text_content;
        }
    }
    if (!text || !text[0]) text = "Button";

    // Get computed bounds
    IRComputedLayout* bounds = ir_layout_get_bounds(component);
    if (!bounds || !bounds->valid) return false;

    // Convert to characters
    int char_x, char_y, char_width, char_height;
    terminal_pixel_to_char_coords(ctx, bounds->x, bounds->y, &char_x, &char_y);
    terminal_pixel_to_char_size(ctx, bounds->width, bounds->height, &char_width, &char_height);

    // Bounds check
    if (char_x >= ctx->term_width || char_y >= ctx->term_height) return true;

    // Get colors
    uint32_t bg_color = 0x444444FF;  // Default dark gray
    uint32_t fg_color = 0xFFFFFFFF;  // Default white

    if (component->style) {
        if (component->style->background.type == IR_COLOR_SOLID) {
            IRColor c = component->style->background;
            bg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
        if (component->style->font.color.type == IR_COLOR_SOLID) {
            IRColor c = component->style->font.color;
            fg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
    }

    // Draw button background
    uint8_t r = (bg_color >> 24) & 0xFF;
    uint8_t g = (bg_color >> 16) & 0xFF;
    uint8_t b = (bg_color >> 8) & 0xFF;
    printf("\033[48;2;%d;%d;%dm", r, g, b);

    for (int y = 0; y < char_height && (char_y + y) < ctx->term_height; y++) {
        terminal_move_cursor(char_x, char_y + y);
        for (int x = 0; x < char_width && (char_x + x) < ctx->term_width; x++) {
            printf(" ");
        }
    }

    // Draw border using box drawing characters
    if (component->style && component->style->border.width > 0) {
        terminal_draw_box(char_x, char_y, char_width, char_height, ctx->unicode_support);
    }

    // Draw button text (centered)
    int text_len = strlen(text);
    int text_x = char_x + (char_width - text_len) / 2;
    int text_y = char_y + char_height / 2;

    if (text_x >= 0 && text_x < ctx->term_width &&
        text_y >= 0 && text_y < ctx->term_height) {
        terminal_set_fg_color(fg_color, ctx->color_mode);
        terminal_move_cursor(text_x, text_y);

        // Truncate if needed
        if (text_len > char_width - 2) {
            printf("%.*s", char_width - 2, text);
        } else {
            printf("%s", text);
        }
    }

    // Reset colors
    printf("\033[0m");

    return true;
}

bool terminal_render_container(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // Get computed bounds
    IRComputedLayout* bounds = ir_layout_get_bounds(component);
    if (bounds && bounds->valid) {
        // Convert to characters
        int char_x, char_y, char_width, char_height;
        terminal_pixel_to_char_coords(ctx, bounds->x, bounds->y, &char_x, &char_y);
        terminal_pixel_to_char_size(ctx, bounds->width, bounds->height, &char_width, &char_height);

        // Render background if specified
        if (component->style && component->style->background.type == IR_COLOR_SOLID) {
            IRColor c = component->style->background;
            uint32_t bg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;

            uint8_t r = (bg_color >> 24) & 0xFF;
            uint8_t g = (bg_color >> 16) & 0xFF;
            uint8_t b = (bg_color >> 8) & 0xFF;
            printf("\033[48;2;%d;%d;%dm", r, g, b);

            // Fill background
            for (int y = 0; y < char_height && (char_y + y) < ctx->term_height; y++) {
                if (char_y + y < 0) continue;
                terminal_move_cursor(char_x, char_y + y);
                for (int x = 0; x < char_width && (char_x + x) < ctx->term_width; x++) {
                    if (char_x + x >= 0) printf(" ");
                }
            }
            printf("\033[0m");
        }

        // Render border if specified
        if (component->style && component->style->border.width > 0) {
            // Set border color
            if (component->style->border_color.type == IR_COLOR_SOLID) {
                IRColor c = component->style->border_color;
                uint32_t border_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
                terminal_set_fg_color(border_color, ctx->color_mode);
            }

            // Draw border
            terminal_draw_box(char_x, char_y, char_width, char_height, ctx->unicode_support);
            printf("\033[0m");
        }
    }

    // Render children
    for (uint32_t i = 0; i < component->child_count; i++) {
        terminal_render_ir_component(ctx, component->children[i]);
    }

    return true;
}

bool terminal_render_input(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // Get current value (from text_content)
    const char* value = "";
    if (component->text_content) {
        value = component->text_content;
    }

    // Get placeholder (from component properties if available, otherwise default)
    const char* placeholder = "Enter text...";
    // TODO: Read from component->properties when available

    // Get bounds
    IRComputedLayout* bounds = ir_layout_get_bounds(component);
    if (!bounds || !bounds->valid) return false;

    int char_x, char_y, char_width, char_height;
    terminal_pixel_to_char_coords(ctx, bounds->x, bounds->y, &char_x, &char_y);
    terminal_pixel_to_char_size(ctx, bounds->width, bounds->height, &char_width, &char_height);

    // Bounds check
    if (char_x >= ctx->term_width || char_y >= ctx->term_height) return true;

    // Get colors
    uint32_t bg_color = 0xFFFFFFFF;  // Default white background
    uint32_t fg_color = 0x000000FF;  // Default black text
    uint32_t border_color = 0xBDC3C7FF;  // Default light gray border

    if (component->style) {
        if (component->style->background.type == IR_COLOR_SOLID) {
            IRColor c = component->style->background;
            bg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
        if (component->style->font.color.type == IR_COLOR_SOLID) {
            IRColor c = component->style->font.color;
            fg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
        if (component->style->border_color.type == IR_COLOR_SOLID) {
            IRColor c = component->style->border_color;
            border_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
    }

    // Draw background
    uint8_t r = (bg_color >> 24) & 0xFF;
    uint8_t g = (bg_color >> 16) & 0xFF;
    uint8_t b = (bg_color >> 8) & 0xFF;
    printf("\033[48;2;%d;%d;%dm", r, g, b);

    for (int y = 0; y < char_height && (char_y + y) < ctx->term_height; y++) {
        if (char_y + y < 0) continue;
        terminal_move_cursor(char_x, char_y + y);
        for (int x = 0; x < char_width && (char_x + x) < ctx->term_width; x++) {
            if (char_x + x >= 0) printf(" ");
        }
    }
    printf("\033[0m");

    // Draw border
    terminal_set_fg_color(border_color, ctx->color_mode);
    terminal_draw_box(char_x, char_y, char_width, char_height, ctx->unicode_support);
    printf("\033[0m");

    // Draw value or placeholder
    int text_x = char_x + 1;
    int text_y = char_y + char_height / 2;

    if (text_x >= 0 && text_x < ctx->term_width &&
        text_y >= 0 && text_y < ctx->term_height) {

        terminal_move_cursor(text_x, text_y);

        if (value && value[0]) {
            // Show value
            terminal_set_fg_color(fg_color, ctx->color_mode);
            int max_len = char_width - 3;  // Leave room for cursor and padding
            int len = strlen(value);
            if (len > max_len) {
                printf("%.*s", max_len, value);
            } else {
                printf("%s", value);
            }

            // Show cursor if focused (TODO: check focus state)
            // printf("▋");
        } else {
            // Show placeholder in gray
            terminal_set_fg_color(0x888888FF, ctx->color_mode);
            int max_len = char_width - 3;
            int len = strlen(placeholder);
            if (len > max_len) {
                printf("%.*s", max_len, placeholder);
            } else {
                printf("%s", placeholder);
            }
        }

        printf("\033[0m");
    }

    return true;
}

bool terminal_render_checkbox(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // Get checked state (from custom_data or properties)
    bool checked = false;
    // TODO: Read from component state when available
    // For now, checkboxes default to unchecked

    // Get label text
    const char* label = component->text_content;
    if (!label || !label[0]) label = "Checkbox";

    // Get bounds
    IRComputedLayout* bounds = ir_layout_get_bounds(component);
    if (!bounds || !bounds->valid) return false;

    int char_x, char_y;
    terminal_pixel_to_char_coords(ctx, bounds->x, bounds->y, &char_x, &char_y);

    // Bounds check
    if (char_x >= ctx->term_width || char_y >= ctx->term_height) return true;

    // Get text color
    uint32_t fg_color = 0xFFFFFFFF;  // Default white
    if (component->style && component->style->font.color.type == IR_COLOR_SOLID) {
        IRColor c = component->style->font.color;
        fg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
    }

    // Position cursor
    terminal_move_cursor(char_x, char_y);

    // Render checkbox symbol
    terminal_set_fg_color(fg_color, ctx->color_mode);
    if (ctx->unicode_support) {
        printf(checked ? "☑ " : "☐ ");
    } else {
        printf(checked ? "[x] " : "[ ] ");
    }

    // Render label
    int label_x = char_x + (ctx->unicode_support ? 2 : 4);
    if (label_x < ctx->term_width) {
        terminal_move_cursor(label_x, char_y);
        int max_len = ctx->term_width - label_x - 1;
        int len = strlen(label);
        if (len > max_len) {
            printf("%.*s", max_len, label);
        } else {
            printf("%s", label);
        }
    }

    printf("\033[0m");

    return true;
}

bool terminal_render_dropdown(TerminalRenderContext* ctx, IRComponent* component) {
    if (!ctx || !component) return false;

    // Get selected value
    const char* selected = "Select...";
    if (component->text_content && component->text_content[0]) {
        selected = component->text_content;
    }

    // Get expanded state (TODO: track in component custom_data)
    bool expanded = false;

    // Get bounds
    IRComputedLayout* bounds = ir_layout_get_bounds(component);
    if (!bounds || !bounds->valid) return false;

    int char_x, char_y, char_width, char_height;
    terminal_pixel_to_char_coords(ctx, bounds->x, bounds->y, &char_x, &char_y);
    terminal_pixel_to_char_size(ctx, bounds->width, bounds->height, &char_width, &char_height);

    // Bounds check
    if (char_x >= ctx->term_width || char_y >= ctx->term_height) return true;

    // Get colors
    uint32_t bg_color = 0xFFFFFFFF;  // Default white background
    uint32_t fg_color = 0x000000FF;  // Default black text
    uint32_t border_color = 0xBDC3C7FF;  // Default light gray border

    if (component->style) {
        if (component->style->background.type == IR_COLOR_SOLID) {
            IRColor c = component->style->background;
            bg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
        if (component->style->font.color.type == IR_COLOR_SOLID) {
            IRColor c = component->style->font.color;
            fg_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
        if (component->style->border_color.type == IR_COLOR_SOLID) {
            IRColor c = component->style->border_color;
            border_color = (c.data.r << 24) | (c.data.g << 16) | (c.data.b << 8) | c.data.a;
        }
    }

    // Draw background
    uint8_t r = (bg_color >> 24) & 0xFF;
    uint8_t g = (bg_color >> 16) & 0xFF;
    uint8_t b = (bg_color >> 8) & 0xFF;
    printf("\033[48;2;%d;%d;%dm", r, g, b);

    for (int y = 0; y < char_height && (char_y + y) < ctx->term_height; y++) {
        if (char_y + y < 0) continue;
        terminal_move_cursor(char_x, char_y + y);
        for (int x = 0; x < char_width && (char_x + x) < ctx->term_width; x++) {
            if (char_x + x >= 0) printf(" ");
        }
    }
    printf("\033[0m");

    // Draw dropdown box border
    terminal_set_fg_color(border_color, ctx->color_mode);
    terminal_draw_box(char_x, char_y, char_width, char_height, ctx->unicode_support);
    printf("\033[0m");

    // Draw selected value
    int text_x = char_x + 1;
    int text_y = char_y + char_height / 2;

    if (text_x >= 0 && text_x < ctx->term_width &&
        text_y >= 0 && text_y < ctx->term_height) {
        terminal_move_cursor(text_x, text_y);
        terminal_set_fg_color(fg_color, ctx->color_mode);

        int text_len = char_width - 4;  // Leave room for arrow
        int len = strlen(selected);
        if (len > text_len) {
            printf("%.*s", text_len, selected);
        } else {
            printf("%s", selected);
        }
        printf("\033[0m");
    }

    // Draw dropdown arrow
    int arrow_x = char_x + char_width - 2;
    int arrow_y = char_y + char_height / 2;

    if (arrow_x >= 0 && arrow_x < ctx->term_width &&
        arrow_y >= 0 && arrow_y < ctx->term_height) {
        terminal_move_cursor(arrow_x, arrow_y);
        terminal_set_fg_color(fg_color, ctx->color_mode);
        printf(ctx->unicode_support ? "▼" : "v");
        printf("\033[0m");
    }

    // If expanded, draw options popup (TODO: implement when state tracking is available)
    if (expanded) {
        // Draw options list below
        // This would render the dropdown menu below the main box
    }

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
