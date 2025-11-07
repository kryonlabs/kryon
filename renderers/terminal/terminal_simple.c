/**
 * Simplified Kryon Terminal Renderer Backend
 *
 * Basic terminal renderer using ANSI escape sequences for cross-platform
 * compatibility. This focuses on core rendering functionality without
 * complex event handling for now.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "../../core/include/kryon.h"

// ============================================================================
// Terminal Renderer State
// ============================================================================

typedef struct {
    // Terminal dimensions
    uint16_t term_width;
    uint16_t term_height;

    // Terminal capabilities
    bool supports_color;
    bool supports_unicode;

    // Current cursor position
    uint16_t cursor_x;
    uint16_t cursor_y;

    // Terminal settings
    struct termios orig_termios;
    bool raw_mode_enabled;

} kryon_terminal_simple_state_t;

// ============================================================================
// Utility Functions
// ============================================================================

static void set_raw_mode(bool enable) {
    struct termios termios;
    if (tcgetattr(STDIN_FILENO, &termios) == 0) {
        static struct termios orig_termios;
        static bool saved = false;

        if (!saved) {
            orig_termios = termios;
            saved = true;
        }

        if (enable) {
            termios.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &termios);
        } else {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        }
    }
}

static void get_terminal_size(uint16_t* width, uint16_t* height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (width) *width = ws.ws_col;
        if (height) *height = ws.ws_row;
    } else {
        // Fallback to reasonable defaults
        if (width) *width = 80;
        if (height) *height = 24;
    }
}

static void move_cursor(uint16_t x, uint16_t y) {
    printf("\033[%d;%dH", y + 1, x + 1);
}

static void set_color(uint32_t color, bool background) {
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;

    // Check if we're dealing with bright colors
    bool bright = (r > 128 || g > 128 || b > 128);

    // Simple color mapping to basic ANSI colors
    int color_code = 0; // Default to black
    if (r > 128 && g < 128 && b < 128) color_code = 1;      // Red
    else if (r < 128 && g > 128 && b < 128) color_code = 2; // Green
    else if (r > 128 && g > 128 && b < 128) color_code = 3; // Yellow
    else if (r < 128 && g < 128 && b > 128) color_code = 4; // Blue
    else if (r > 128 && g < 128 && b > 128) color_code = 5; // Magenta
    else if (r < 128 && g > 128 && b > 128) color_code = 6; // Cyan
    else if (r > 128 && g > 128 && b > 128) color_code = 7; // White

    if (bright) color_code += 60; // Bright colors (90-97 instead of 30-37)

    printf("\033[%dm", (background ? color_code + 10 : color_code + 30));
}

static void reset_color(void) {
    printf("\033[0m");
}

// ============================================================================
// Drawing Functions
// ============================================================================

static void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    // Move to starting position
    move_cursor(x, y);

    // Set background color
    set_color(color, true);

    // Draw filled rectangle using spaces
    for (uint16_t row = 0; row < h && (y + row) < 24; row++) {
        move_cursor(x, y + row);
        for (uint16_t col = 0; col < w && (x + col) < 80; col++) {
            putchar(' ');
        }
    }

    reset_color();
    fflush(stdout);
}

static void draw_text(const char* text, uint16_t x, uint16_t y, uint32_t color) {
    if (!text) return;

    // Move to position
    move_cursor(x, y);

    // Set text color
    set_color(color, false);

    // Print text
    printf("%s", text);

    reset_color();
    fflush(stdout);
}

static void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color) {
    // Simple line drawing using character approximation
    set_color(color, false);

    if (x1 == x2) {
        // Vertical line
        uint16_t start_y = (y1 < y2) ? y1 : y2;
        uint16_t end_y = (y1 > y2) ? y1 : y2;
        for (uint16_t y = start_y; y <= end_y; y++) {
            move_cursor(x1, y);
            putchar('|');
        }
    } else if (y1 == y2) {
        // Horizontal line
        uint16_t start_x = (x1 < x2) ? x1 : x2;
        uint16_t end_x = (x1 > x2) ? x1 : x2;
        move_cursor(start_x, y1);
        for (uint16_t x = start_x; x <= end_x; x++) {
            putchar('-');
        }
    } else {
        // Diagonal line - use dots
        int16_t dx = (x2 > x1) ? 1 : -1;
        int16_t dy = (y2 > y1) ? 1 : -1;
        int16_t x = x1, y = y1;

        while (x != x2 && y != y2) {
            move_cursor(x, y);
            putchar('*');
            x += dx;
            y += dy;
        }
    }

    reset_color();
    fflush(stdout);
}

// ============================================================================
// Renderer Operations Implementation
// ============================================================================

static bool terminal_init(kryon_renderer_t* renderer, void* native_window) {
    (void)native_window; // Unused parameter

    kryon_terminal_simple_state_t* state = malloc(sizeof(kryon_terminal_simple_state_t));
    if (!state) return false;

    memset(state, 0, sizeof(kryon_terminal_simple_state_t));
    renderer->backend_data = state;

    // Get terminal size
    get_terminal_size(&state->term_width, &state->term_height);

    // Check basic capabilities
    const char* term = getenv("TERM");
    state->supports_color = (term && strstr(term, "color"));
    state->supports_unicode = (term && strstr(term, "xterm"));

    // Clear screen initially
    printf("\033[2J\033[H");
    fflush(stdout);

    return true;
}

static void terminal_shutdown(kryon_renderer_t* renderer) {
    kryon_terminal_simple_state_t* state = (kryon_terminal_simple_state_t*)renderer->backend_data;
    if (!state) return;

    // Restore terminal settings
    if (state->raw_mode_enabled) {
        set_raw_mode(false);
    }

    // Clear screen and show cursor
    printf("\033[2J\033[H\033[?25h");
    fflush(stdout);

    free(state);
    renderer->backend_data = NULL;
}

static void terminal_begin_frame(kryon_renderer_t* renderer) {
    kryon_terminal_simple_state_t* state = (kryon_terminal_simple_state_t*)renderer->backend_data;
    if (!state) return;

    // Check for terminal resize
    uint16_t new_width, new_height;
    get_terminal_size(&new_width, &new_height);

    if (new_width != state->term_width || new_height != state->term_height) {
        state->term_width = new_width;
        state->term_height = new_height;
        renderer->width = new_width;
        renderer->height = new_height;

        // Clear screen on resize
        printf("\033[2J\033[H");
        fflush(stdout);
    }
}

static void terminal_end_frame(kryon_renderer_t* renderer) {
    (void)renderer; // Unused parameter

    // Move cursor to bottom of screen to avoid interfering with output
    printf("\033[%d;1H", 24); // Move to line 24
    fflush(stdout);
}

static void terminal_execute_commands(kryon_renderer_t* renderer, kryon_cmd_buf_t* buf) {
    kryon_terminal_simple_state_t* state = (kryon_terminal_simple_state_t*)renderer->backend_data;
    if (!state || !buf) return;

    kryon_command_t cmd;
    while (kryon_cmd_buf_pop(buf, &cmd)) {
        switch (cmd.type) {
            case KRYON_CMD_DRAW_RECT:
                draw_rect(cmd.data.draw_rect.x, cmd.data.draw_rect.y,
                         cmd.data.draw_rect.w, cmd.data.draw_rect.h,
                         cmd.data.draw_rect.color);
                break;

            case KRYON_CMD_DRAW_TEXT:
                draw_text(cmd.data.draw_text.text, cmd.data.draw_text.x, cmd.data.draw_text.y,
                         cmd.data.draw_text.color);
                break;

            case KRYON_CMD_DRAW_LINE:
                draw_line(cmd.data.draw_line.x1, cmd.data.draw_line.y1,
                         cmd.data.draw_line.x2, cmd.data.draw_line.y2,
                         cmd.data.draw_line.color);
                break;

            case KRYON_CMD_DRAW_ARC:
                // Arc drawing not implemented in simple version
                break;

            default:
                // Unsupported command, ignore
                break;
        }
    }
}

static void terminal_swap_buffers(kryon_renderer_t* renderer) {
    (void)renderer; // No buffering in this simple implementation
}

static void terminal_get_dimensions(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height) {
    kryon_terminal_simple_state_t* state = (kryon_terminal_simple_state_t*)renderer->backend_data;
    if (!state) return;

    if (width) *width = state->term_width;
    if (height) *height = state->term_height;
}

static void terminal_set_clear_color(kryon_renderer_t* renderer, uint32_t color) {
    (void)renderer; (void)color; // Not implemented in simple version
}

// ============================================================================
// Renderer Operations Table
// ============================================================================

const kryon_renderer_ops_t kryon_terminal_simple_ops = {
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

kryon_renderer_t* kryon_terminal_simple_renderer_create(void) {
    kryon_renderer_t* renderer = malloc(sizeof(kryon_renderer_t));
    if (!renderer) return NULL;

    renderer->ops = &kryon_terminal_simple_ops;
    renderer->backend_data = NULL;
    renderer->width = 80;   // Will be updated during init
    renderer->height = 24;  // Will be updated during init
    renderer->clear_color = 0x000000FF;
    renderer->vsync_enabled = false;

    return renderer;
}

void kryon_terminal_simple_renderer_destroy(kryon_renderer_t* renderer) {
    if (!renderer) return;

    if (renderer->backend_data) {
        renderer->ops->shutdown(renderer);
    }

    free(renderer);
}

// Wrapper functions to match the expected API
kryon_renderer_t* kryon_terminal_renderer_create(void) {
    return kryon_terminal_simple_renderer_create();
}

void kryon_terminal_renderer_destroy(kryon_renderer_t* renderer) {
    kryon_terminal_simple_renderer_destroy(renderer);
}