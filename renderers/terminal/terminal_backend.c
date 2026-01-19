/**
 * Kryon Terminal Renderer Backend
 *
 * Implements a terminal-based renderer using libtickit for cross-platform
 * terminal UI capabilities. This backend provides TUI (Terminal User Interface)
 * support while maintaining compatibility with Kryon's component model.
 *
 * CRITICAL: This backend is source-language agnostic and works with IR only.
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

// IR integration (source-agnostic)
#include "../../ir/include/ir_core.h"
#include "../../ir/include/ir_serialization.h"
#include "terminal_ir_bridge.h"

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

// Draw rectangle in terminal cells (using ANSI codes)
static void terminal_draw_rect(kryon_terminal_state_t* state,
                               int16_t x, int16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (x >= state->term_width || y >= state->term_height) return;

    // Clamp coordinates to terminal bounds
    int16_t draw_x = (x < 0) ? 0 : x;
    int16_t draw_y = (y < 0) ? 0 : y;
    uint16_t draw_w = (draw_x + w > state->term_width) ? state->term_width - draw_x : w;
    uint16_t draw_h = (draw_y + h > state->term_height) ? state->term_height - draw_y : h;

    // Extract RGB from color
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;

    // Set background color using ANSI codes
    printf("\033[48;2;%d;%d;%dm", r, g, b);

    // Draw filled rectangle using spaces with background color
    for (int16_t row = draw_y; row < draw_y + draw_h; row++) {
        printf("\033[%d;%dH", row + 1, draw_x + 1);  // Move cursor
        for (uint16_t col = 0; col < draw_w; col++) {
            printf(" ");
        }
    }

    // Reset color
    printf("\033[0m");

    mark_dirty(state, draw_x, draw_y, draw_w, draw_h);
}

// Draw text in terminal (using ANSI codes)
static void terminal_draw_text(kryon_terminal_state_t* state,
                               const char* text, int16_t x, int16_t y,
                               uint16_t font_id, uint32_t color) {
    (void)font_id;  // Unused in terminal renderer
    if (x >= state->term_width || y >= state->term_height || !text) return;

    // Extract RGB from color
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;

    // Position cursor and set color
    printf("\033[%d;%dH", y + 1, x + 1);
    printf("\033[38;2;%d;%d;%dm", r, g, b);

    // Print text
    printf("%s", text);

    // Reset color
    printf("\033[0m");

    mark_dirty(state, x, y, strlen(text), 1);
}

// Draw line using ANSI codes (simplified)
static void terminal_draw_line(kryon_terminal_state_t* state,
                               int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color) {
    // Extract RGB from color
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;

    // Set color
    printf("\033[38;2;%d;%d;%dm", r, g, b);

    // Simple Bresenham's line algorithm
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t current_x = x1;
    int16_t current_y = y1;

    while (1) {
        if (current_x >= 0 && current_x < state->term_width &&
            current_y >= 0 && current_y < state->term_height) {
            char line_char = (dx == 0) ? '|' : (dy == 0) ? '-' : '*';
            printf("\033[%d;%dH%c", current_y + 1, current_x + 1, line_char);
        }

        if (current_x == x2 && current_y == y2) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; current_x += sx; }
        if (e2 < dx) { err += dx; current_y += sy; }
    }

    printf("\033[0m");
    mark_dirty(state, (x1 < x2) ? x1 : x2, (y1 < y2) ? y1 : y2, abs(x2 - x1) + 1, abs(y2 - y1) + 1);
}

// Draw arc/circle using ANSI codes (simplified)
static void terminal_draw_arc(kryon_terminal_state_t* state,
                              int16_t cx, int16_t cy, uint16_t radius,
                              int16_t start_angle, int16_t end_angle, uint32_t color) {
    (void)start_angle;  // Unused - full circle for now
    (void)end_angle;    // Unused - full circle for now

    // Extract RGB from color
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;

    // Set color
    printf("\033[38;2;%d;%d;%dm", r, g, b);

    // Simple midpoint circle algorithm
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        // Draw 8 points
        if (cx + x >= 0 && cx + x < state->term_width && cy + y >= 0 && cy + y < state->term_height)
            printf("\033[%d;%dHo", cy + y + 1, cx + x + 1);
        if (cx + y >= 0 && cx + y < state->term_width && cy + x >= 0 && cy + x < state->term_height)
            printf("\033[%d;%dHo", cy + x + 1, cx + y + 1);
        if (cx - y >= 0 && cx - y < state->term_width && cy + x >= 0 && cy + x < state->term_height)
            printf("\033[%d;%dHo", cy + x + 1, cx - y + 1);
        if (cx - x >= 0 && cx - x < state->term_width && cy + y >= 0 && cy + y < state->term_height)
            printf("\033[%d;%dHo", cy + y + 1, cx - x + 1);
        if (cx - x >= 0 && cx - x < state->term_width && cy - y >= 0 && cy - y < state->term_height)
            printf("\033[%d;%dHo", cy - y + 1, cx - x + 1);
        if (cx - y >= 0 && cx - y < state->term_width && cy - x >= 0 && cy - x < state->term_height)
            printf("\033[%d;%dHo", cy - x + 1, cx - y + 1);
        if (cx + y >= 0 && cx + y < state->term_width && cy - x >= 0 && cy - x < state->term_height)
            printf("\033[%d;%dHo", cy - x + 1, cx + y + 1);
        if (cx + x >= 0 && cx + x < state->term_width && cy - y >= 0 && cy - y < state->term_height)
            printf("\033[%d;%dHo", cy - y + 1, cx + x + 1);

        if (err <= 0) { y += 1; err += 2 * y + 1; }
        if (err > 0) { x -= 1; err -= 2 * x + 1; }
    }

    printf("\033[0m");
    mark_dirty(state, cx - radius, cy - radius, radius * 2, radius * 2);
}

// ============================================================================
// Renderer Operations Implementation
// ============================================================================

static bool terminal_init(kryon_renderer_t* renderer, void* native_window) {
    (void)native_window;  // Unused for terminal renderer

    kryon_terminal_state_t* state = malloc(sizeof(kryon_terminal_state_t));
    if (!state) return false;

    memset(state, 0, sizeof(kryon_terminal_state_t));
    renderer->backend_data = state;

    // For now, use a simplified ANSI-based terminal (no tickit)
    // Get terminal size using ANSI escape codes or defaults
    state->term_width = 80;   // Default, can be detected via ioctl
    state->term_height = 24;  // Default

    // Detect color mode from COLORTERM environment variable
    const char* colorterm = getenv("COLORTERM");
    if (colorterm && (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0)) {
        state->color_mode = 24;  // Truecolor
        state->true_color = true;
    } else {
        const char* term = getenv("TERM");
        if (term && strstr(term, "256color")) {
            state->color_mode = 8;   // 256 colors
        } else {
            state->color_mode = 4;   // 16 colors
        }
    }

    state->unicode_support = true; // Assume Unicode support
    state->mouse_enabled = false;   // Disabled for now

    // Set up signal handler for terminal resize
    signal(SIGWINCH, handle_sigwinch);

    // Hide cursor and clear screen using ANSI codes
    printf("\033[?25l");  // Hide cursor
    printf("\033[2J");    // Clear screen
    printf("\033[H");     // Move to home

    // Note: tickit members are NULL - using ANSI codes directly
    state->tickit = NULL;
    state->root = NULL;
    state->buffer = NULL;
    state->default_pen = NULL;

    return true;
}

static void terminal_shutdown(kryon_renderer_t* renderer) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return;

    // Restore terminal state using ANSI codes
    printf("\033[?25h");  // Show cursor
    printf("\033[0m");    // Reset colors
    printf("\033[2J");    // Clear screen
    printf("\033[H");     // Move to home

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

    // Move cursor to home (rendering will happen via IR bridge using ANSI codes)
    printf("\033[H");
}

static void terminal_end_frame(kryon_renderer_t* renderer) {
    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return;

    // Flush output
    fflush(stdout);

    // Clear dirty region after rendering
    state->dirty_region.dirty = false;
    state->needs_redraw = false;
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

    // Extract RGB and set background color using ANSI codes
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;

    printf("\033[48;2;%d;%d;%dm", r, g, b);
    printf("\033[2J");  // Clear screen with background color
    printf("\033[0m");  // Reset
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

// ============================================================================
// IR Component Tree Rendering (Source-Agnostic)
// ============================================================================

/**
 * Render IR component tree directly to terminal
 *
 * CRITICAL: This function is source-language agnostic.
 * Works with IR from ANY frontend: TSX, Kry, HTML, Markdown, Lua, C, etc.
 *
 * @param renderer Terminal renderer instance
 * @param root IR component tree root (from any source)
 * @return true on success, false on failure
 */
bool kryon_terminal_render_ir_tree(kryon_renderer_t* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    kryon_terminal_state_t* state = (kryon_terminal_state_t*)renderer->backend_data;
    if (!state) return false;

    // Create rendering context
    TerminalRenderContext ctx = {
        .unicode_support = state->unicode_support,
        .color_mode = state->color_mode,
        .term_width = state->term_width,
        .term_height = state->term_height,
        .char_width = 8,   // Default character cell width
        .char_height = 12, // Default character cell height
        .buffer = state->buffer,
        .pen = state->default_pen,
        .use_libtickit = (state->tickit != NULL),
        .needs_redraw = false,
        .frame_count = 0
    };

    // Compute layout using IR layout system (source-agnostic)
    // Convert terminal dimensions to pixels for layout calculation
    float pixel_width = ctx.term_width * ctx.char_width;
    float pixel_height = ctx.term_height * ctx.char_height;
    ir_layout_compute_tree(root, pixel_width, pixel_height);

    // Begin frame
    renderer->ops->begin_frame(renderer);

    // Render IR tree (works for ANY source language)
    bool success = terminal_render_ir_tree(&ctx, root);

    // End frame (flush to terminal)
    renderer->ops->end_frame(renderer);

    return success;
}