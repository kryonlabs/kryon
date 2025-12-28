/**
 * Kryon Terminal Renderer Backend - Public API
 *
 * libtickit-based terminal UI backend for Kryon UI framework.
 * Provides TUI (Terminal User Interface) capabilities with
 * cross-platform compatibility and modern terminal features.
 */

#ifndef KRYON_TERMINAL_BACKEND_H
#define KRYON_TERMINAL_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include <tickit.h>

#include "../../core/include/kryon.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Terminal Renderer State
// ============================================================================

typedef struct kryon_terminal_event_state kryon_terminal_event_state_t;

// ============================================================================
// Terminal Renderer Creation
// ============================================================================

/**
 * Create a new terminal renderer instance
 * @return Pointer to renderer instance or NULL on failure
 */
kryon_renderer_t* kryon_terminal_renderer_create(void);

/**
 * Destroy a terminal renderer instance
 * @param renderer Renderer instance to destroy
 */
void kryon_terminal_renderer_destroy(kryon_renderer_t* renderer);

// ============================================================================
// Terminal Event System
// ============================================================================

/**
 * Initialize terminal event handling system
 * @param tickit Tickit instance
 * @param root Root window instance
 * @return Event state instance or NULL on failure
 */
kryon_terminal_event_state_t* kryon_terminal_events_init(Tickit* tickit, TickitWindow* root);

/**
 * Shutdown terminal event handling system
 * @param event_state Event state instance
 */
void kryon_terminal_events_shutdown(kryon_terminal_event_state_t* event_state);

/**
 * Poll for pending events (non-blocking)
 * @param event_state Event state instance
 * @param event Output event structure
 * @return True if event was retrieved, false otherwise
 */
bool kryon_terminal_events_poll(kryon_terminal_event_state_t* event_state, kryon_event_t* event);

/**
 * Wait for events (blocking with optional timeout)
 * @param event_state Event state instance
 * @param event Output event structure
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return True if event was retrieved, false otherwise
 */
bool kryon_terminal_events_wait(kryon_terminal_event_state_t* event_state, kryon_event_t* event, int timeout_ms);

/**
 * Check if terminal resize is pending
 * @param event_state Event state instance
 * @param width Output for new width (optional)
 * @param height Output for new height (optional)
 * @return True if resize is pending, false otherwise
 */
bool kryon_terminal_events_has_resize(kryon_terminal_event_state_t* event_state, uint16_t* width, uint16_t* height);

/**
 * Clear resize pending flag
 * @param event_state Event state instance
 */
void kryon_terminal_events_clear_resize(kryon_terminal_event_state_t* event_state);

/**
 * Get current mouse position
 * @param event_state Event state instance
 * @param x Output for x coordinate (optional)
 * @param y Output for y coordinate (optional)
 */
void kryon_terminal_events_get_mouse_position(kryon_terminal_event_state_t* event_state, int* x, int* y);

/**
 * Check if mouse drag is active
 * @param event_state Event state instance
 * @return True if drag is active, false otherwise
 */
bool kryon_terminal_events_is_drag_active(kryon_terminal_event_state_t* event_state);

/**
 * Enable or disable mouse support
 * @param event_state Event state instance
 * @param enabled True to enable mouse, false to disable
 */
void kryon_terminal_events_set_mouse_enabled(kryon_terminal_event_state_t* event_state, bool enabled);

/**
 * Get event queue statistics
 * @param event_state Event state instance
 * @param queued Output for number of queued events (optional)
 * @param capacity Output for queue capacity (optional)
 */
void kryon_terminal_events_get_stats(kryon_terminal_event_state_t* event_state, size_t* queued, size_t* capacity);

// ============================================================================
// Terminal Capabilities
// ============================================================================

/**
 * Check terminal color capability
 * @param renderer Terminal renderer instance
 * @return Number of colors supported (16, 256, or 16777216 for truecolor)
 */
int kryon_terminal_get_color_capability(kryon_renderer_t* renderer);

/**
 * Check if terminal supports Unicode
 * @param renderer Terminal renderer instance
 * @return True if Unicode is supported, false otherwise
 */
bool kryon_terminal_supports_unicode(kryon_renderer_t* renderer);

/**
 * Check if terminal supports mouse events
 * @param renderer Terminal renderer instance
 * @return True if mouse is supported, false otherwise
 */
bool kryon_terminal_supports_mouse(kryon_renderer_t* renderer);

/**
 * Get terminal size in characters
 * @param renderer Terminal renderer instance
 * @param width Output for width in characters (optional)
 * @param height Output for height in characters (optional)
 */
void kryon_terminal_get_size(kryon_renderer_t* renderer, uint16_t* width, uint16_t* height);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert character coordinates to pixel coordinates (approximate)
 * @param renderer Terminal renderer instance
 * @param char_x Character X coordinate
 * @param char_y Character Y coordinate
 * @param pixel_x Output for pixel X coordinate (optional)
 * @param pixel_y Output for pixel Y coordinate (optional)
 */
void kryon_terminal_char_to_pixel(kryon_renderer_t* renderer, int char_x, int char_y, int* pixel_x, int* pixel_y);

/**
 * Convert pixel coordinates to character coordinates (approximate)
 * @param renderer Terminal renderer instance
 * @param pixel_x Pixel X coordinate
 * @param pixel_y Pixel Y coordinate
 * @param char_x Output for character X coordinate (optional)
 * @param char_y Output for character Y coordinate (optional)
 */
void kryon_terminal_pixel_to_char(kryon_renderer_t* renderer, int pixel_x, int pixel_y, int* char_x, int* char_y);

/**
 * Set terminal title (if supported)
 * @param title Terminal title string
 * @return True if title was set, false otherwise
 */
bool kryon_terminal_set_title(const char* title);

/**
 * Clear terminal screen
 * @param renderer Terminal renderer instance
 */
void kryon_terminal_clear_screen(kryon_renderer_t* renderer);

/**
 * Show or hide terminal cursor
 * @param renderer Terminal renderer instance
 * @param visible True to show cursor, false to hide
 */
void kryon_terminal_set_cursor_visible(kryon_renderer_t* renderer, bool visible);

// ============================================================================
// Color Management
// ============================================================================

/**
 * Convert RGB color to terminal-compatible color
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param color_mode Terminal color mode (16, 256, or 24)
 * @return Terminal color value
 */
int kryon_terminal_rgb_to_color(uint8_t r, uint8_t g, uint8_t b, int color_mode);

/**
 * Convert Kryon color (0xRRGGBBAA) to terminal color
 * @param kryon_color Kryon color value
 * @param color_mode Terminal color mode (16, 256, or 24)
 * @return Terminal color value
 */
int kryon_terminal_kryon_color_to_color(uint32_t kryon_color, int color_mode);

// ============================================================================
// Debugging and Diagnostics
// ============================================================================

/**
 * Print terminal capability information
 * @param renderer Terminal renderer instance
 */
void kryon_terminal_print_capabilities(kryon_renderer_t* renderer);

/**
 * Run terminal compatibility test
 * @param renderer Terminal renderer instance
 * @return True if all tests pass, false otherwise
 */
bool kryon_terminal_run_compatibility_test(kryon_renderer_t* renderer);

// ============================================================================
// IR Component Tree Rendering (Source-Agnostic)
// ============================================================================

// Forward declaration (avoids requiring ir_core.h in header)
typedef struct IRComponent IRComponent;

/**
 * Render IR component tree directly to terminal
 *
 * CRITICAL: This function is source-language agnostic.
 * Works with IRComponent* from ANY frontend: TSX, Kry, HTML, Markdown, Lua, C, etc.
 *
 * Example usage:
 *   // Load KIR file (compiled from TSX/Kry/HTML/etc.)
 *   IRComponent* root = ir_read_json_file("app.kir");
 *
 *   // Create terminal renderer
 *   kryon_renderer_t* renderer = kryon_terminal_renderer_create();
 *   kryon_terminal_renderer_init(renderer);
 *
 *   // Render (works regardless of source language)
 *   kryon_terminal_render_ir_tree(renderer, root);
 *
 * @param renderer Terminal renderer instance
 * @param root IR component tree root (from ANY source)
 * @return True on success, false on failure
 */
bool kryon_terminal_render_ir_tree(kryon_renderer_t* renderer, IRComponent* root);

#ifdef __cplusplus
}
#endif

#endif // KRYON_TERMINAL_BACKEND_H