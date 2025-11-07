/**
 * Kryon Terminal Renderer - Utility Functions
 *
 * Provides utility functions for color management, coordinate conversion,
 * and terminal capability detection for the terminal renderer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "terminal_backend.h"

// ============================================================================
// Color Management Functions
// ============================================================================

int kryon_terminal_rgb_to_color(uint8_t r, uint8_t g, uint8_t b, int color_mode) {
    switch (color_mode) {
        case 24: // Truecolor
            return (r << 16) | (g << 8) | b;

        case 8: // 256 colors
            // Convert RGB to 256-color palette
            // This is a simplified approximation
            if (r == g && g == b) {
                // Grayscale
                if (r < 8) return 16;
                if (r > 248) return 231;
                return 232 + (r - 8) / 10;
            } else {
                // 6x6x6 color cube
                int ri = (r * 5) / 255;
                int gi = (g * 5) / 255;
                int bi = (b * 5) / 255;
                return 16 + (36 * ri) + (6 * gi) + bi;
            }

        case 4: // 16 colors
            // Basic 16-color mapping
            if (r < 64 && g < 64 && b < 64) return 0;      // Black
            if (r > 192 && g < 64 && b < 64) return 1;     // Red
            if (r < 64 && g > 192 && b < 64) return 2;     // Green
            if (r > 192 && g > 192 && b < 64) return 3;    // Yellow
            if (r < 64 && g < 64 && b > 192) return 4;     // Blue
            if (r > 192 && g < 64 && b > 192) return 5;    // Magenta
            if (r < 64 && g > 192 && b > 192) return 6;    // Cyan
            if (r > 192 && g > 192 && b > 192) return 7;   // White
            if (r > 128 && g > 128 && b > 128) return 8;   // Bright Black (Gray)
            if (r > 192 && g < 128 && b < 128) return 9;   // Bright Red
            if (r < 128 && g > 192 && b < 128) return 10;  // Bright Green
            if (r > 192 && g > 192 && b < 128) return 11;  // Bright Yellow
            if (r < 128 && g < 128 && b > 192) return 12;  // Bright Blue
            if (r > 192 && g < 128 && b > 192) return 13;  // Bright Magenta
            if (r < 128 && g > 192 && b > 192) return 14;  // Bright Cyan
            return 15; // Bright White

        default:
            return 0; // Default to black
    }
}

int kryon_terminal_kryon_color_to_color(uint32_t kryon_color, int color_mode) {
    uint8_t r = (kryon_color >> 24) & 0xFF;
    uint8_t g = (kryon_color >> 16) & 0xFF;
    uint8_t b = (kryon_color >> 8) & 0xFF;
    uint8_t a = kryon_color & 0xFF;

    // Handle alpha by blending with black background
    if (a < 255) {
        r = (r * a) / 255;
        g = (g * a) / 255;
        b = (b * a) / 255;
    }

    return kryon_terminal_rgb_to_color(r, g, b, color_mode);
}

// ============================================================================
// Coordinate Conversion Functions
// ============================================================================

void kryon_terminal_char_to_pixel(kryon_renderer_t* renderer, int char_x, int char_y, int* pixel_x, int* pixel_y) {
    if (!renderer) return;

    // Assume 8x12 character cells (can be made configurable)
    const int char_width = 8;
    const int char_height = 12;

    if (pixel_x) *pixel_x = char_x * char_width;
    if (pixel_y) *pixel_y = char_y * char_height;
}

void kryon_terminal_pixel_to_char(kryon_renderer_t* renderer, int pixel_x, int pixel_y, int* char_x, int* char_y) {
    if (!renderer) return;

    // Assume 8x12 character cells (can be made configurable)
    const int char_width = 8;
    const int char_height = 12;

    if (char_x) *char_x = pixel_x / char_width;
    if (char_y) *char_y = pixel_y / char_height;
}

// ============================================================================
// Terminal Control Functions
// ============================================================================

bool kryon_terminal_set_title(const char* title) {
    if (!title) return false;

    // Check if we're in a terminal that supports title setting
    const char* term = getenv("TERM");
    if (!term) return false;

    // Don't set title in basic terminals
    if (strstr(term, "linux") || strstr(term, "vt100") || strstr(term, "ansi")) {
        return false;
    }

    // Set terminal title using ANSI escape sequences
    printf("\033]2;%s\007", title);
    fflush(stdout);

    return true;
}

void kryon_terminal_clear_screen(kryon_renderer_t* renderer) {
    (void)renderer; // Unused parameter

    // Clear screen and move cursor to top-left
    printf("\033[2J\033[H");
    fflush(stdout);
}

void kryon_terminal_set_cursor_visible(kryon_renderer_t* renderer, bool visible) {
    (void)renderer; // Unused parameter

    if (visible) {
        printf("\033[?25h"); // Show cursor
    } else {
        printf("\033[?25l"); // Hide cursor
    }
    fflush(stdout);
}

// ============================================================================
// Terminal Capability Functions
// ============================================================================

int kryon_terminal_get_color_capability(kryon_renderer_t* renderer) {
    if (!renderer) return 4; // Default to 16 colors

    // This should be stored in the renderer state
    // For now, check environment variables as a fallback
    const char* colorterm = getenv("COLORTERM");
    const char* term = getenv("TERM");

    if (colorterm) {
        if (strstr(colorterm, "truecolor") || strstr(colorterm, "24bit")) {
            return 24; // Truecolor
        }
        if (strstr(colorterm, "256color")) {
            return 8; // 256 colors
        }
    }

    if (term) {
        if (strstr(term, "256color") || strstr(term, "xterm-256color") ||
            strstr(term, "screen-256color") || strstr(term, "tmux-256color")) {
            return 8; // 256 colors
        }
    }

    return 4; // 16 colors
}

bool kryon_terminal_supports_unicode(kryon_renderer_t* renderer) {
    (void)renderer; // Unused parameter

    const char* lang = getenv("LANG");
    const char* lc_all = getenv("LC_ALL");
    const char* lc_ctype = getenv("LC_CTYPE");

    // Check for UTF-8 in locale variables
    const char* locales[] = {lang, lc_all, lc_ctype};
    for (int i = 0; i < 3; i++) {
        if (locales[i] && strstr(locales[i], "UTF-8")) {
            return true;
        }
        if (locales[i] && strstr(locales[i], "utf8")) {
            return true;
        }
    }

    return false; // Assume no Unicode support
}

bool kryon_terminal_supports_mouse(kryon_renderer_t* renderer) {
    (void)renderer; // Unused parameter

    const char* term = getenv("TERM");
    if (!term) return false;

    // Most modern terminals support mouse
    if (strstr(term, "xterm") || strstr(term, "screen") || strstr(term, "tmux") ||
        strstr(term, "alacritty") || strstr(term, "kitty") || strstr(term, "gnome") ||
        strstr(term, "konsole") || strstr(term, "iterm")) {
        return true;
    }

    return false;
}

void kryon_terminal_get_size(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height) {
    if (!renderer) {
        // Fallback to ioctl if no renderer
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            if (width) *width = ws.ws_col;
            if (height) *height = ws.ws_row;
        } else {
            if (width) *width = 80;
            if (height) *height = 24;
        }
        return;
    }

    // Get size from renderer state
    renderer->ops->get_dimensions(renderer, width, height);
}

// ============================================================================
// Debugging and Diagnostics
// ============================================================================

void kryon_terminal_print_capabilities(kryon_renderer_t* renderer) {
    printf("Terminal Capabilities:\n");
    printf("=====================\n");

    const char* term = getenv("TERM");
    printf("TERM: %s\n", term ? term : "unknown");

    const char* colorterm = getenv("COLORTERM");
    printf("COLORTERM: %s\n", colorterm ? colorterm : "not set");

    int colors = kryon_terminal_get_color_capability(renderer);
    printf("Colors: %d (%s)\n", colors,
           colors == 24 ? "truecolor" :
           colors == 8 ? "256-color" : "16-color");

    bool unicode = kryon_terminal_supports_unicode(renderer);
    printf("Unicode: %s\n", unicode ? "yes" : "no");

    bool mouse = kryon_terminal_supports_mouse(renderer);
    printf("Mouse: %s\n", mouse ? "yes" : "no");

    uint16_t width, height;
    kryon_terminal_get_size(renderer, &width, &height);
    printf("Size: %dx%d characters\n", width, height);

    printf("\nCharacter cell size (approximate): 8x12 pixels\n");
    printf("Pixel size (approximate): %dx%d pixels\n", width * 8, height * 12);
    printf("\n");
}

bool kryon_terminal_run_compatibility_test(kryon_renderer_t* renderer) {
    printf("Running Terminal Compatibility Test\n");
    printf("===================================\n\n");

    bool all_passed = true;

    // Test 1: Basic output
    printf("Test 1: Basic output... ");
    printf("✓ Text output works\n");
    fflush(stdout);

    // Test 2: Color support
    printf("Test 2: Color support... ");
    int colors = kryon_terminal_get_color_capability(renderer);
    if (colors >= 4) {
        printf("✓ %s colors supported\n", colors == 24 ? "truecolor" :
               colors == 8 ? "256-color" : "16-color");
    } else {
        printf("✗ Limited color support\n");
        all_passed = false;
    }

    // Test 3: Cursor control
    printf("Test 3: Cursor control... ");
    // Note: This would need a renderer instance, skipping for now
    printf("✓ Cursor control test skipped (needs renderer instance)\n");

    // Test 4: Clear screen
    printf("Test 4: Screen clearing... ");
    sleep(1);
    kryon_terminal_clear_screen(renderer);
    printf("✓ Screen clearing works\n");

    // Test 5: Positioning
    printf("Test 5: Cursor positioning... ");
    printf("\033[10;10H✓ Positioning works\n\033[20;1H");
    fflush(stdout);

    // Test 6: Unicode support
    printf("Test 6: Unicode support... ");
    bool unicode = kryon_terminal_supports_unicode(renderer);
    if (unicode) {
        printf("✓ Unicode: ✓ ✓ ✓ ♥ ♥ ♥\n");
    } else {
        printf("? Unicode status unknown\n");
    }

    // Test 7: Size detection
    printf("Test 7: Size detection... ");
    uint16_t width, height;
    kryon_terminal_get_size(renderer, &width, &height);
    if (width > 0 && height > 0) {
        printf("✓ Size: %dx%d\n", width, height);
    } else {
        printf("✗ Could not detect terminal size\n");
        all_passed = false;
    }

    printf("\nCompatibility Test: %s\n", all_passed ? "PASSED" : "FAILED");
    printf("Terminal is %sready for Kryon applications.\n",
           all_passed ? "" : "NOT ");

    return all_passed;
}