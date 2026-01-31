/**
 * Screenshot Utilities for X11 Window Capture
 *
 * Provides functionality for capturing screenshots of X11 windows,
 * specifically designed for capturing the TaijiOS emulator window
 * when running Limbo target applications.
 *
 * This is a pure Kryon implementation - no TaijiOS modifications required.
 */

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>     // for uint8_t
#include <sys/types.h>  // for pid_t

/* Forward declarations to avoid including X11 headers in public header */
typedef struct _XDisplay Display;
typedef unsigned long Window;

/* ============================================================================
 * Screenshot Configuration
 * ============================================================================ */

/**
 * Screenshot options structure
 */
typedef struct {
    const char* output_path;      // Path where screenshot will be saved
    int after_frames;             // Number of frames to wait before capturing
    int timeout_ms;               // Maximum time to wait for window (default: 5000ms)
    const char* window_title;     // Window title pattern to search for (default: "TaijiOS")
} ScreenshotOptions;

/**
 * Default screenshot options
 */
#define SCREENSHOT_DEFAULTS { \
    .output_path = NULL, \
    .after_frames = 0, \
    .timeout_ms = 5000, \
    .window_title = "TaijiOS" \
}

/* ============================================================================
 * X11 Window Finding
 * ============================================================================ */

/**
 * Find an X11 window by title pattern
 *
 * Searches the window hierarchy for a window whose title matches
 * the given pattern (case-sensitive substring match).
 *
 * @param display X11 display connection
 * @param root Root window to start search from
 * @param title_pattern Title pattern to search for
 * @return Window ID or None if not found
 */
Window find_x11_window_by_title(void* display, Window root, const char* title_pattern);

/**
 * Find an X11 window by process ID
 *
 * Searches for windows owned by the specified process.
 *
 * @param display X11 display connection
 * @param root Root window to start search from
 * @param pid Process ID to search for
 * @return Window ID or None if not found
 */
Window find_x11_window_by_pid(void* display, Window root, pid_t pid);

/* ============================================================================
 * X11 Screenshot Capture
 * ============================================================================ */

/**
 * Capture screenshot of an X11 window to PNG file
 *
 * Captures the window content and saves it as a PNG file.
 * Uses stb_image_write for PNG encoding.
 *
 * @param display X11 display connection
 * @param window Window to capture
 * @param output_path Path where PNG will be saved
 * @return true on success, false on failure
 */
bool capture_x11_window_to_png(void* display, Window window, const char* output_path);

/**
 * Wait for window to appear and capture screenshot
 *
 * Waits up to timeout_ms for a window matching the title to appear,
 * then captures a screenshot.
 *
 * @param title_pattern Window title to search for
 * @param output_path Path where PNG will be saved
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return true on success, false on failure
 */
bool capture_window_when_ready(const char* title_pattern, const char* output_path,
                               int timeout_ms);

/* ============================================================================
 * High-Level Screenshot Functions
 * ============================================================================ */

/**
 * Capture TaijiOS emulator window screenshot
 *
 * High-level function that:
 * 1. Opens X11 display
 * 2. Finds the TaijiOS emulator window
 * 3. Waits for window to be ready
 * 4. Captures screenshot
 * 5. Closes display and saves PNG
 *
 * @param options Screenshot options (use SCREENSHOT_DEFAULTS for defaults)
 * @return true on success, false on failure
 */
bool capture_emulator_window(const ScreenshotOptions* options);

/**
 * Parse screenshot options from environment variables
 *
 * Reads KRYON_SCREENSHOT and KRYON_SCREENSHOT_AFTER_FRAMES
 * environment variables and populates options structure.
 *
 * @param options Options structure to populate
 */
void screenshot_parse_env_vars(ScreenshotOptions* options);

#endif // SCREENSHOT_H
