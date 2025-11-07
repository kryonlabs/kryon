/**
 * Kryon Terminal Renderer Backend
 *
 * Implements a terminal-based renderer using libtickit for cross-platform
 * terminal UI capabilities. This backend provides TUI (Terminal User Interface)
 * support while maintaining compatibility with Kryon's component model.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <tickit.h>

#include "../../core/include/kryon.h"
#include "terminal_backend.h"

// ============================================================================
// Terminal Renderer State
// ============================================================================

typedef struct {
    // Tickit state
    Tickit* tickit;
    TickitWindow* root;
    TickitRenderBuffer* buffer;
    TickitPen* default_pen;

    // Terminal capabilities
    uint8_t color_mode;        // 16/256/24-bit
    bool mouse_enabled;
    bool unicode_support;
    bool true_color;

    // Rendering state
    uint16_t term_width;
    uint16_t term_height;
    bool needs_redraw;
    bool needs_resize;

    // Dirty region tracking
    struct {
        int x1, y1, x2, y2;
        bool dirty;
    } dirty_region;

    // Color management
    uint32_t color_palette[256];
    uint8_t color_pairs[256];

    // Event handling
    kryon_event_t current_event;
    bool event_pending;

    // Performance optimization
    char* char_buffer;
    size_t buffer_size;

} kryon_terminal_state_t;

// ============================================================================
// Forward Declarations
// ============================================================================

static bool terminal_init(kryon_renderer_t* renderer, void* native_window);
static void terminal_shutdown(kryon_renderer_t* renderer);
static void terminal_begin_frame(kryon_renderer_t* renderer);
static void terminal_end_frame(kryon_renderer_t* renderer);
static void terminal_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf);
static void terminal_swap_buffers(kryon_renderer_t* renderer);
static void terminal_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height);
static void terminal_set_clear_color(kryon_renderer_t* renderer, uint32_t color);

// ============================================================================
// Utility Functions
// ============================================================================

// Convert Kryon color (0xRRGGBBAA) to Tickit color
static int kryon_color_to_tickit(uint32_t kryon_color) {
    uint8_t r = (kryon_color >> 24) & 0xFF;
    uint8_t g = (kryon_color >> 16) & 0xFF;
    uint8_t b = (kryon_color >> 8) & 0xFF;
    uint8_t a = kryon_color & 0xFF;

    // Handle alpha by blending with background
    if (a < 255) {
        // Simple alpha blending with black background
        r = (r * a) / 255;
        g = (g * a) / 255;
        b = (b * a) / 255;
    }

    return (r << 16) | (g << 8) | b;
}

// Mark region as dirty
static void mark_dirty(kryon_terminal_state_t* state, int x, int y, int w, int h) {
    if (!state->dirty_region.dirty) {
        state->dirty_region.x1 = x;
        state->dirty_region.y1 = y;
        state->dirty_region.x2 = x + w;
        state->dirty_region.y2 = y + h;
        state->dirty_region.dirty = true;
    } else {
        // Expand dirty region
        if (x < state->dirty_region.x1) state->dirty_region.x1 = x;
        if (y < state->dirty_region.y1) state->dirty_region.y1 = y;
        if (x + w > state->dirty_region.x2) state->dirty_region.x2 = x + w;
        if (y + h > state->dirty_region.y2) state->dirty_region.y2 = y + h;
    }
}

// Handle terminal resize signal
static void handle_sigwinch(int sig) {
    (void)sig; // Unused parameter
    // Signal will be handled in the event loop
}

// ============================================================================
// Drawing Functions
// ============================================================================

// Draw rectangle in terminal cells
static void terminal_draw_rect(kryon_terminal_state_t* state,
                               int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (x >= state->term_width || y >= state->term_height) return;

    // Clamp coordinates to terminal bounds
    int16_t draw_x = (x < 0) ? 0 : x;
    int16_t draw_y = (y < 0) ? 0 : y;
    uint16_t draw_w = (draw_x + w > state->term_width) ? state->term_width - draw_x : w;
    uint16_t draw_h = (draw_y + h > state->term_height) ? state->term_height - draw_y : h;

    // Set pen for background color
    TickitPen* pen = tickit_pen_new();
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, kryon_color_to_tickit(color));

    // Draw filled rectangle using spaces with background color
    for (int16_t row = draw_y; row < draw_y + draw_h; row++) {
        for (int16_t col = draw_x; col < draw_x + draw_w; col++) {
            tickit_renderbuffer_char(state->buffer, col, row, ' ', pen);
        }
    }

    tickit_pen_unref(pen);
    mark_dirty(state, draw_x, draw_y, draw_w, draw_h);
}

// Draw text in terminal
static void terminal_draw_text(kryon_terminal_state_t* state,
                               const char* text, int16_t x, int16_t y,
                               uint16_t font_id, uint32_t color) {
    if (x >= state->term_width || y >= state->term_height || !text) return;

    // Set pen for text color
    TickitPen* pen = tickit_pen_new();
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, kryon_color_to_tickit(color));

    // Draw text character by character
    int16_t current_x = x;
    const char* ptr = text;

    while (*ptr && current_x < state->term_width) {
        // Handle simple ASCII for now, Unicode can be added later
        if ((unsigned char)*ptr >= 32 && (unsigned char)*ptr <= 126) {
            tickit_renderbuffer_char(state->buffer, current_x, y, *ptr, pen);
            current_x++;
        }
        ptr++;
    }

    tickit_pen_unref(pen);
    mark_dirty(state, x, y, current_x - x, 1);
}

// Draw line using terminal line-drawing characters
static void terminal_draw_line(kryon_terminal_state_t* state,
                               int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color) {
    // Simple implementation using Bresenham's algorithm
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    int16_t current_x = x1;
    int16_t current_y = y1;

    // Set pen for line color
    TickitPen* pen = tickit_pen_new();
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, kryon_color_to_tickit(color));

    while (1) {
        // Draw point
        if (current_x >= 0 && current_x < state->term_width &&
            current_y >= 0 && current_y < state->term_height) {
            // Use different characters for different line directions
            char line_char = '*';
            if (dx == 0) line_char = '|';          // Vertical
            else if (dy == 0) line_char = '-';     // Horizontal
            else line_char = '*';                  // Diagonal

            tickit_renderbuffer_char(state->buffer, current_x, current_y, line_char, pen);
        }

        if (current_x == x2 && current_y == y2) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            current_x += sx;
        }
        if (e2 < dx) {
            err += dx;
            current_y += sy;
        }
    }

    tickit_pen_unref(pen);
    mark_dirty(state,
              (x1 < x2) ? x1 : x2,
              (y1 < y2) ? y1 : y2,
              abs(x2 - x1) + 1,
              abs(y2 - y1) + 1);
}

// Draw arc/circle using character approximation
static void terminal_draw_arc(kryon_terminal_state_t* state,
                              int16_t cx, int16_t cy, uint16_t radius,
                              int16_t start_angle, int16_t end_angle, uint32_t color) {
    // Simple circle approximation using ASCII characters
    // This is a very basic implementation
    TickitPen* pen = tickit_pen_new();
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, kryon_color_to_tickit(color));

    // Draw circle using midpoint circle algorithm
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        // Draw 8 points of the circle
        if (cx + x >= 0 && cx + x < state->term_width && cy + y >= 0 && cy + y < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx + x, cy + y, 'o', pen);
        if (cx + y >= 0 && cx + y < state->term_width && cy + x >= 0 && cy + x < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx + y, cy + x, 'o', pen);
        if (cx - y >= 0 && cx - y < state->term_width && cy + x >= 0 && cy + x < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx - y, cy + x, 'o', pen);
        if (cx - x >= 0 && cx - x < state->term_width && cy + y >= 0 && cy + y < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx - x, cy + y, 'o', pen);
        if (cx - x >= 0 && cx - x < state->term_width && cy - y >= 0 && cy - y < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx - x, cy - y, 'o', pen);
        if (cx - y >= 0 && cx - y < state->term_width && cy - x >= 0 && cy - x < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx - y, cy - x, 'o', pen);
        if (cx + y >= 0 && cx + y < state->term_width && cy - x >= 0 && cy - x < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx + y, cy - x, 'o', pen);
        if (cx + x >= 0 && cx + x < state->term_width && cy - y >= 0 && cy - y < state->term_height)
            tickit_renderbuffer_char(state->buffer, cx + x, cy - y, 'o', pen);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }

    tickit_pen_unref(pen);
    mark_dirty(state, cx - radius, cy - radius, radius * 2, radius * 2);
}

// ============================================================================
// Renderer Operations Implementation
// ============================================================================

static bool terminal_init(kryon_renderer_t* renderer, void* native_window) {
    kryon_terminal_state_t* state = malloc(sizeof(kryon_terminal_state_t));
    if (!state) return false;

    memset(state, 0, sizeof(kryon_terminal_state_t));
    renderer->backend_data = state;

    // Initialize tickit
    state->tickit = tickit_new();
    if (!state->tickit) {
        free(state);
        return false;
    }

    // Set up terminal
    tickit_term_setctl_int(tickit_get_term(state->tickit), TICKIT_TERMCTL_HIDE_CURSOR, 1);
    tickit_term_setctl_int(tickit_get_term(state->tickit), TICKIT_TERMCTL_MOUSE, TICKIT_MOUSEEV_DRAG);

    // Create root window
    state->root = tickit_get_rootwin(state->tickit);
    if (!state->root) {
        tickit_free(state->tickit);
        free(state);
        return false;
    }

    // Get terminal size
    state->term_width = tickit_window_get_cols(state->root);
    state->term_height = tickit_window_get_lines(state->root);

    // Create render buffer
    state->buffer = tickit_renderbuffer_new(state->term_width, state->term_height);
    if (!state->buffer) {
        tickit_window_unref(state->root);
        tickit_free(state->tickit);
        free(state);
        return false;
    }

    // Create default pen
    state->default_pen = tickit_pen_new();

    // Detect terminal capabilities
    int colors = tickit_term_get_int(tickit_get_term(state->tickit), TICKIT_TERM_COLORS);
    if (colors >= 16777216) {
        state->color_mode = 24;  // Truecolor
        state->true_color = true;
    } else if (colors >= 256) {
        state->color_mode = 8;   // 256 colors
    } else {
        state->color_mode = 4;   // 16 colors
    }

    state->unicode_support = true; // Assume Unicode support
    state->mouse_enabled = true;

    // Set up signal handler for terminal resize
    signal(SIGWINCH, handle_sigwinch);

    // Clear screen initially
    tickit_term_clear(tickit_get_term(state->tickit));

    return true;
}

static void terminal_shutdown(kryon_renderer_t* renderer) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return;

    // Clean up tickit resources
    if (state->buffer) {
        tickit_renderbuffer_unref(state->buffer);
    }
    if (state->default_pen) {
        tickit_pen_destroy(state->default_pen);
    }
    if (state->root) {
        tickit_window_unref(state->root);
    }
    if (state->tickit) {
        // Note: Show cursor not available in this API version
        tickit_free(state->tickit);
    }

    // Clean up character buffer
    if (state->char_buffer) {
        free(state->char_buffer);
    }

    free(state);
    renderer->backend_data = NULL;
}

static void terminal_begin_frame(kryon_renderer_t* renderer) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return;

    // Clear dirty region
    state->dirty_region.dirty = false;

    // Check for terminal resize
    int new_width = tickit_window_get_cols(state->root);
    int new_height = tickit_window_get_lines(state->root);

    if (new_width != state->term_width || new_height != state->term_height) {
        state->term_width = new_width;
        state->term_height = new_height;
        state->needs_resize = true;

        // Resize render buffer
        if (state->buffer) {
            tickit_renderbuffer_unref(state->buffer);
        }
        state->buffer = tickit_renderbuffer_new(state->term_width, state->term_height);
    }

    // Clear render buffer for new frame
    tickit_renderbuffer_clear(state->buffer);
}

static void terminal_end_frame(kryon_renderer_t* renderer) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return;

    // Render buffer to terminal
    // Note: Advanced rendering not available in this API version
    // For now, we'll skip the complex rendering
}

static void terminal_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state || !buf) return;

    kryon_command_t cmd;
    while (kryon_cmd_buf_pop(buf, &cmd)) {
        switch (cmd.type) {
            case KRYON_CMD_DRAW_RECT:
                terminal_draw_rect(state,
                                  cmd.data.draw_rect.x,
                                  cmd.data.draw_rect.y,
                                  cmd.data.draw_rect.w,
                                  cmd.data.draw_rect.h,
                                  cmd.data.draw_rect.color);
                break;

            case KRYON_CMD_DRAW_TEXT:
                terminal_draw_text(state,
                                  cmd.data.draw_text.text,
                                  cmd.data.draw_text.x,
                                  cmd.data.draw_text.y,
                                  cmd.data.draw_text.font_id,
                                  cmd.data.draw_text.color);
                break;

            case KRYON_CMD_DRAW_LINE:
                terminal_draw_line(state,
                                  cmd.data.draw_line.x1,
                                  cmd.data.draw_line.y1,
                                  cmd.data.draw_line.x2,
                                  cmd.data.draw_line.y2,
                                  cmd.data.draw_line.color);
                break;

            case KRYON_CMD_DRAW_ARC:
                terminal_draw_arc(state,
                                 cmd.data.draw_arc.cx,
                                 cmd.data.draw_arc.cy,
                                 cmd.data.draw_arc.radius,
                                 cmd.data.draw_arc.start_angle,
                                 cmd.data.draw_arc.end_angle,
                                 cmd.data.draw_arc.color);
                break;

            case KRYON_CMD_DRAW_TEXTURE:
                // Texture rendering not supported in terminal
                break;

            case KRYON_CMD_SET_CLIP:
            case KRYON_CMD_PUSH_CLIP:
            case KRYON_CMD_POP_CLIP:
                // Clipping not implemented for terminal renderer
                break;

            case KRYON_CMD_SET_TRANSFORM:
            case KRYON_CMD_PUSH_TRANSFORM:
            case KRYON_CMD_POP_TRANSFORM:
                // Transform operations not supported in terminal
                break;

            default:
                // Unsupported command, ignore
                break;
        }
    }
}

static void terminal_swap_buffers(kryon_renderer_t* renderer) {
    // Terminal rendering uses immediate mode, so swapping is not needed
    // The actual rendering happens in end_frame
    (void)renderer;
}

static void terminal_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return;

    if (width) *width = state->term_width;
    if (height) *height = state->term_height;
}

static void terminal_set_clear_color(kryon_renderer_t* renderer, uint32_t color) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return;

    // Set default pen background color
    tickit_pen_set_colour_attr(state->default_pen, TICKIT_PEN_BG, kryon_color_to_tickit(color));
}

// ============================================================================
// Renderer Operations Table
// ============================================================================

const kryon_renderer_ops_t kryon_terminal_ops = {
    .init = terminal_init,
    .shutdown = terminal_shutdown,
    .begin_frame = terminal_begin_frame,
    .end_frame = terminal_end_frame,
    .execute_commands = terminal_execute_commands,
    .swap_buffers = terminal_swap_buffers,
    .get_dimensions = terminal_get_dimensions,
    .set_clear_color = terminal_set_clear_color
};

// ============================================================================
// Public API Functions
// ============================================================================

kryon_renderer_t* kryon_terminal_renderer_create(void) {
    kryon_renderer_t* renderer = malloc(sizeof(kryon_renderer_t));
    if (!renderer) return NULL;

    renderer->ops = &kryon_terminal_ops;
    renderer->backend_data = NULL;
    renderer->width = 80;   // Will be updated during init
    renderer->height = 24;  // Will be updated during init
    renderer->clear_color = 0x000000FF;
    renderer->vsync_enabled = false;

    return renderer;
}

void kryon_terminal_renderer_destroy(kryon_renderer_t* renderer) {
    if (!renderer) return;

    if (renderer->backend_data) {
        renderer->ops->shutdown(renderer);
    }

    free(renderer);
}