/**
 * Screenshot Utilities Implementation
 *
 * X11-based screenshot capture for external windows (e.g., TaijiOS emulator)
 * Pure Kryon implementation - no target modifications required.
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "screenshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>  // For uint8_t
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>  // For kill()

/* X11 headers */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

/* stb_image_write - single-file library */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../third_party/stb/stb_image_write.h"

/* ============================================================================
 * X11 Window Finding
 * ============================================================================ */

/**
 * Forward declarations
 */
static bool title_matches_pattern(const char* title, const char* pattern);

/**
 * Get window title using _NET_WM_NAME or WM_NAME
 */
static char* get_window_title(Display* display, Window window) {
    Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    Atom wm_name = XInternAtom(display, "WM_NAME", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop = NULL;

    // Try _NET_WM_NAME (UTF-8) first
    if (XGetWindowProperty(display, window, net_wm_name, 0, 1024, False,
                          utf8_string, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            char* title = strndup((char*)prop, nitems);
            XFree(prop);
            return title;
        }
        if (prop) XFree(prop);
    }

    // Fallback to WM_NAME (legacy)
    if (XGetWindowProperty(display, window, wm_name, 0, 1024, False,
                          AnyPropertyType, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            char* title = strndup((char*)prop, nitems);
            XFree(prop);
            return title;
        }
        if (prop) XFree(prop);
    }

    return NULL;
}

/**
 * Get window PID using _NET_WM_PID
 */
static pid_t get_window_pid(Display* display, Window window) {
    Atom net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop = NULL;
    pid_t pid = 0;

    if (XGetWindowProperty(display, window, net_wm_pid, 0, 1, False,
                          XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            // CORRECT: Read as native 32-bit integer (CARDINAL is in native byte order)
            pid = *(pid_t*)prop;
        }
        if (prop) XFree(prop);
    }

    return pid;
}

/**
 * Check if window is visible and mapped
 */
static bool is_window_visible(Display* display, Window window) {
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs) == 0) {
        return false;
    }
    return attrs.map_state == IsViewable;
}

/**
 * Check if window is a terminal by examining _NET_WM_WINDOW_TYPE
 */
static bool is_terminal_window(Display* display, Window window) {
    Atom net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop = NULL;

    if (XGetWindowProperty(display, window, net_wm_window_type, 0, 10, False,
                          XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            Atom* atom_vals = (Atom*)prop;
            // Check for terminal type
            for (unsigned long i = 0; i < nitems; i++) {
                char* atom_name = XGetAtomName(display, atom_vals[i]);
                if (atom_name) {
                    bool is_terminal = (strcmp(atom_name, "_NET_WM_WINDOW_TYPE_TERMINAL") == 0 ||
                                       strcmp(atom_name, "_NET_WM_WINDOW_TYPE_DIALOG") == 0);
                    XFree(atom_name);
                    if (is_terminal) {
                        XFree(prop);
                        return true;
                    }
                }
            }
        }
        if (prop) XFree(prop);
    }
    return false;
}

/**
 * Check if window is a terminal by examining WM_CLASS
 */
static bool is_terminal_by_class(Display* display, Window window) {
    Atom wm_class = XInternAtom(display, "WM_CLASS", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop = NULL;

    if (XGetWindowProperty(display, window, wm_class, 0, 1024, False,
                          AnyPropertyType, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success) {
        if (prop && nitems > 0) {
            char* class_str = strndup((char*)prop, nitems);
            XFree(prop);

            // Check for terminal class names (case-insensitive)
            if (class_str) {
                bool is_term = (strcasestr(class_str, "terminal") != NULL ||
                               strcasestr(class_str, "gnome-terminal") != NULL ||
                               strcasestr(class_str, "konsole") != NULL ||
                               strcasestr(class_str, "xterm") != NULL ||
                               strcasestr(class_str, "urxvt") != NULL ||
                               strcasestr(class_str, "alacritty") != NULL ||
                               strcasestr(class_str, "kitty") != NULL);
                free(class_str);
                return is_term;
            }
        }
        if (prop) XFree(prop);
    }
    return false;
}

/**
 * Check if window size suggests it's an emulator window
 */
static bool is_emulator_window_size(int width, int height) {
    // Emulator windows typically > 640x480
    return width >= 640 && height >= 480;
}

/**
 * Structure to hold window candidates
 */
typedef struct {
    Window window;
    char* title;
    int width;
    int height;
    int score;  // Higher is better
} WindowCandidate;

/**
 * Global state for collecting window candidates
 */
static struct {
    WindowCandidate* candidates;
    int count;
    int capacity;
} candidate_list = {NULL, 0, 0};

/**
 * Add a window to the candidate list
 */
static void add_candidate(Window window, const char* title, int width, int height) {
    if (candidate_list.count >= candidate_list.capacity) {
        int new_capacity = candidate_list.capacity == 0 ? 10 : candidate_list.capacity * 2;
        candidate_list.candidates = realloc(candidate_list.candidates,
                                           new_capacity * sizeof(WindowCandidate));
        candidate_list.capacity = new_capacity;
    }

    candidate_list.candidates[candidate_list.count].window = window;
    candidate_list.candidates[candidate_list.count].title = strdup(title);
    candidate_list.candidates[candidate_list.count].width = width;
    candidate_list.candidates[candidate_list.count].height = height;
    // Score based on size (larger is more likely to be emulator)
    candidate_list.candidates[candidate_list.count].score = width * height;
    candidate_list.count++;
}

/**
 * Reset candidate list
 */
static void reset_candidates(void) {
    for (int i = 0; i < candidate_list.count; i++) {
        free(candidate_list.candidates[i].title);
    }
    free(candidate_list.candidates);
    candidate_list.candidates = NULL;
    candidate_list.count = 0;
    candidate_list.capacity = 0;
}

/* ============================================================================
 * Window List Management (for Delta Tracking)
 * ============================================================================ */

/**
 * Simple list of Window IDs for delta tracking
 */
typedef struct {
    Window* windows;
    int count;
    int capacity;
} WindowList;

/**
 * Helper struct for recursion in get_windows_matching_pattern
 */
typedef struct {
    Display* display;
    const char* pattern;
    WindowList* list;
} CollectContext;

/**
 * Recursive collection function - forward declaration
 */
static void collect_windows_recursive(Window window, CollectContext* ctx);

/**
 * Recursive collection function
 */
static void collect_windows_recursive(Window window, CollectContext* ctx) {
    Window parent;
    Window* children;
    unsigned int nchildren;

    // Check if this window matches
    char* title = get_window_title(ctx->display, window);
    if (title && title_matches_pattern(title, ctx->pattern)) {
        if (is_window_visible(ctx->display, window)) {
            // Add to list
            if (ctx->list->count >= ctx->list->capacity) {
                int new_capacity = ctx->list->capacity * 2;
                Window* new_windows = realloc(ctx->list->windows,
                                               sizeof(Window) * new_capacity);
                if (!new_windows) {
                    free(title);
                    return;
                }
                ctx->list->windows = new_windows;
                ctx->list->capacity = new_capacity;
            }
            ctx->list->windows[ctx->list->count++] = window;
        }
    }
    if (title) free(title);

    // Recurse into children
    if (XQueryTree(ctx->display, window, &window, &parent, &children, &nchildren) == 0) {
        return;
    }

    for (unsigned int i = 0; i < nchildren; i++) {
        collect_windows_recursive(children[i], ctx);
    }

    if (children) XFree(children);
}

/**
 * Create a window list by collecting all windows matching a pattern
 */
static WindowList get_windows_matching_pattern(Display* display, Window root,
                                               const char* pattern) {
    WindowList list = {0};
    list.capacity = 16;
    list.windows = safe_malloc(sizeof(Window) * list.capacity);

    // Collect all matching windows
    CollectContext ctx = {display, pattern, &list};
    collect_windows_recursive(root, &ctx);

    return list;
}

/**
 * Free a window list
 */
static void free_window_list(WindowList* list) {
    if (list->windows) {
        free(list->windows);
        list->windows = NULL;
    }
    list->count = list->capacity = 0;
}

/**
 * Wait for emulator window and select the appropriate one
 *
 * The TaijiOS/Inferno emulator reuses existing windows, so we can't rely on
 * delta tracking. Instead, we wait for the emulator to start and then select
 * the last matching window (most recently active in X11 window stack).
 *
 * @param display X11 display connection
 * @param root Root window
 * @param pattern Window title pattern to match (e.g., "Inferno")
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return Window ID to capture, or None if no suitable window found
 */
static Window wait_for_new_window(Display* display, Window root,
                                  const char* pattern,
                                  int timeout_ms) {
    // Wait for emulator to start/reuse window
    const int poll_interval_ms = 500;
    int elapsed = 0;

    printf("  Waiting for emulator window to stabilize...\n");

    while (elapsed < timeout_ms) {
        usleep(poll_interval_ms * 1000);
        elapsed += poll_interval_ms;

        WindowList windows = get_windows_matching_pattern(display, root, pattern);

        // Debug: show window count
        if (elapsed % 1000 == 0 && elapsed > 0) {
            printf("  [DEBUG] Poll: %d ms, found %d '%s' window(s)\n",
                   elapsed, windows.count, pattern);
        }

        free_window_list(&windows);

        // Progress indicator
        if (elapsed % 1000 == 0 && elapsed > 0) {
            printf("  Waiting... (%d/%d ms)\n", elapsed, timeout_ms);
        }
    }

    // Get current window list and use the last one (most recently active)
    WindowList current = get_windows_matching_pattern(display, root, pattern);
    Window target_window = None;

    if (current.count > 0) {
        // Use the last window (most recently created/active in X11 window stack)
        target_window = current.windows[current.count - 1];
        printf("  ✓ Using window (last of %d '%s' windows)\n", current.count, pattern);
    } else {
        printf("  ✗ No '%s' windows found\n", pattern);
    }

    free_window_list(&current);
    return target_window;
}

/**
 * Case-insensitive substring match
 */
static bool title_matches_pattern(const char* title, const char* pattern) {
    if (!title || !pattern) return false;

    size_t pattern_len = strlen(pattern);
    size_t title_len = strlen(title);

    if (title_len < pattern_len) return false;

    for (size_t i = 0; i <= title_len - pattern_len; i++) {
        bool match = true;
        for (size_t j = 0; j < pattern_len; j++) {
            if (tolower((unsigned char)title[i + j]) !=
                tolower((unsigned char)pattern[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }

    return false;
}

/**
 * Recursive search for all windows matching title pattern
 */
static void search_matching_windows(Display* display, Window window,
                                    const char* title_pattern) {
    Window parent;
    Window* children;
    unsigned int nchildren;

    // Check if this window matches
    char* title = get_window_title(display, window);
    if (title && title_matches_pattern(title, title_pattern)) {
        // Check if window is visible
        if (is_window_visible(display, window)) {
            // Get window size
            XWindowAttributes attrs;
            if (XGetWindowAttributes(display, window, &attrs) != 0) {
                // Filter out terminal windows
                bool is_term = is_terminal_window(display, window) ||
                              is_terminal_by_class(display, window);

                if (is_term) {
                    printf("Filtered terminal window: '%s' (%dx%d)\n",
                           title, attrs.width, attrs.height);
                } else if (!is_emulator_window_size(attrs.width, attrs.height)) {
                    printf("Filtered small window: '%s' (%dx%d)\n",
                           title, attrs.width, attrs.height);
                } else {
                    // Add to candidates
                    printf("Found candidate: '%s' (%dx%d) score=%d\n",
                           title, attrs.width, attrs.height,
                           attrs.width * attrs.height);
                    add_candidate(window, title, attrs.width, attrs.height);
                }
            }
        }
    }
    if (title) free(title);

    // Search children
    if (XQueryTree(display, window, &window, &parent, &children, &nchildren) == 0) {
        return;
    }

    for (unsigned int i = 0; i < nchildren; i++) {
        search_matching_windows(display, children[i], title_pattern);
    }

    if (children) XFree(children);
}

/**
 * Find best matching graphical window
 */
static Window find_best_graphical_window(Display* display, Window root,
                                        const char* title_pattern) {
    // Reset candidate list
    reset_candidates();

    // Search for all matching windows
    search_matching_windows(display, root, title_pattern);

    if (candidate_list.count == 0) {
        reset_candidates();
        return None;
    }

    // Find window with highest score
    int best_idx = 0;
    for (int i = 1; i < candidate_list.count; i++) {
        if (candidate_list.candidates[i].score > candidate_list.candidates[best_idx].score) {
            best_idx = i;
        }
    }

    Window best = candidate_list.candidates[best_idx].window;
    printf("Selected window: '%s' (%dx%d)\n",
           candidate_list.candidates[best_idx].title,
           candidate_list.candidates[best_idx].width,
           candidate_list.candidates[best_idx].height);

    // Cleanup
    reset_candidates();

    return best;
}

/**
 * Find window by title pattern
 */
Window find_x11_window_by_title(void* display_ptr, Window root,
                                const char* title_pattern) {
    Display* display = (Display*)display_ptr;
    if (!display || !title_pattern) {
        return None;
    }

    // Use new algorithm to find best graphical window
    return find_best_graphical_window(display, root, title_pattern);
}

/**
 * Recursive search for window by PID
 */
static Window search_window_by_pid(Display* display, Window root, pid_t target_pid) {
    Window parent;
    Window* children;
    unsigned int nchildren;

    // Check if this window matches
    pid_t pid = get_window_pid(display, root);
    if (pid == target_pid && is_window_visible(display, root)) {
        return root;
    }

    // Search children
    if (XQueryTree(display, root, &root, &parent, &children, &nchildren) == 0) {
        return None;
    }

    Window result = None;
    for (unsigned int i = 0; i < nchildren; i++) {
        result = search_window_by_pid(display, children[i], target_pid);
        if (result != None) {
            break;
        }
    }

    if (children) XFree(children);
    return result;
}

/**
 * Find window by process ID
 */
Window find_x11_window_by_pid(void* display_ptr, Window root, pid_t pid) {
    Display* display = (Display*)display_ptr;
    if (!display || pid <= 0) {
        return None;
    }

    return search_window_by_pid(display, root, pid);
}

/* ============================================================================
 * X11 Screenshot Capture
 * ============================================================================ */

/**
 * Convert XImage to RGBA format
 */
static uint8_t* convert_ximage_to_rgba(XImage* image, int width, int height) {
    if (!image || width <= 0 || height <= 0) {
        return NULL;
    }

    uint8_t* rgba = safe_malloc(width * height * 4);

    // Get visual info for color channels
    Display* display = NULL; // We don't have the display here, assume default visual

    // For TrueColor visuals (most common)
    unsigned long red_mask = image->red_mask;
    unsigned long green_mask = image->green_mask;
    unsigned long blue_mask = image->blue_mask;

    int red_shift, green_shift, blue_shift;

    // Calculate bit shifts for each channel
    if (red_mask == 0xFF0000) {
        red_shift = 16;
        green_shift = 8;
        blue_shift = 0;
    } else if (red_mask == 0x0000FF) {
        red_shift = 0;
        green_shift = 8;
        blue_shift = 16;
    } else {
        // Fallback: auto-detect shifts
        red_shift = green_shift = blue_shift = 0;
        while (!(red_mask & 1)) { red_mask >>= 1; red_shift++; }
        while (!(green_mask & 1)) { green_mask >>= 1; green_shift++; }
        while (!(blue_mask & 1)) { blue_mask >>= 1; blue_shift++; }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned long pixel = XGetPixel(image, x, y);
            int idx = (y * width + x) * 4;

            rgba[idx + 0] = (pixel >> red_shift) & 0xFF;   // R
            rgba[idx + 1] = (pixel >> green_shift) & 0xFF; // G
            rgba[idx + 2] = (pixel >> blue_shift) & 0xFF;  // B
            rgba[idx + 3] = 255;                            // A (fully opaque)
        }
    }

    return rgba;
}

/**
 * Capture X11 window to PNG file
 */
bool capture_x11_window_to_png(void* display_ptr, Window window,
                               const char* output_path) {
    Display* display = (Display*)display_ptr;

    if (!display || window == None || !output_path) {
        fprintf(stderr, "Error: Invalid parameters for X11 capture\n");
        return false;
    }

    // Get window attributes
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs) == 0) {
        fprintf(stderr, "Error: Failed to get window attributes\n");
        return false;
    }

    int width = attrs.width;
    int height = attrs.height;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Error: Invalid window size: %dx%d\n", width, height);
        return false;
    }

    // Capture window content
    XImage* image = XGetImage(display, window, 0, 0, width, height,
                             AllPlanes, ZPixmap);

    if (!image) {
        fprintf(stderr, "Error: Failed to capture window image\n");
        return false;
    }

    // Convert to RGBA
    uint8_t* rgba = convert_ximage_to_rgba(image, width, height);
    if (!rgba) {
        XDestroyImage(image);
        fprintf(stderr, "Error: Failed to convert image to RGBA\n");
        return false;
    }

    // Write PNG using stb_image_write
    int result = stbi_write_png(output_path, width, height, 4, rgba, width * 4);

    // Cleanup
    free(rgba);
    XDestroyImage(image);

    if (result == 0) {
        fprintf(stderr, "Error: Failed to write PNG to %s\n", output_path);
        return false;
    }

    printf("Screenshot saved: %s (%dx%d)\n", output_path, width, height);
    return true;
}

/**
 * Wait for window to appear and capture screenshot
 */
bool capture_window_when_ready(const char* title_pattern, const char* output_path,
                               int timeout_ms) {
    if (!title_pattern || !output_path) {
        return false;
    }

    // Open X11 display
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: Cannot open X11 display (is DISPLAY set?)\n");
        return false;
    }

    Window root = DefaultRootWindow(display);

    // Wait for window to appear
    const int poll_interval_ms = 100;
    int elapsed = 0;
    Window window = None;

    while (elapsed < timeout_ms) {
        window = find_x11_window_by_title(display, root, title_pattern);
        if (window != None) {
            break;
        }

        usleep(poll_interval_ms * 1000);
        elapsed += poll_interval_ms;
    }

    if (window == None) {
        fprintf(stderr, "Error: Window '%s' not found after %d ms\n",
                title_pattern, timeout_ms);
        XCloseDisplay(display);
        return false;
    }

    // Wait a bit for window to fully render
    usleep(500 * 1000); // 500ms

    // Capture screenshot
    bool success = capture_x11_window_to_png(display, window, output_path);

    XCloseDisplay(display);
    return success;
}

/* ============================================================================
 * High-Level Screenshot Functions
 * ============================================================================ */

/**
 * Validate that a PID represents an active process
 *
 * Uses kill(pid, 0) which doesn't actually send a signal but
 * performs error checking to determine if the process exists.
 *
 * @param pid Process ID to validate
 * @return true if process exists, false otherwise
 */
static bool validate_pid(pid_t pid) {
    if (pid <= 0) return false;
    return (kill(pid, 0) == 0);
}

/**
 * Parse environment variables for screenshot options
 */
void screenshot_parse_env_vars(ScreenshotOptions* options) {
    if (!options) return;

    // KRYON_SCREENSHOT environment variable
    if (!options->output_path) {
        const char* env_path = getenv("KRYON_SCREENSHOT");
        if (env_path && strlen(env_path) > 0) {
            options->output_path = env_path;
        }
    }

    // KRYON_SCREENSHOT_AFTER_FRAMES environment variable
    const char* env_frames = getenv("KRYON_SCREENSHOT_AFTER_FRAMES");
    if (env_frames && options->after_frames == 0) {
        options->after_frames = atoi(env_frames);
    }
}

/**
 * Capture TaijiOS/Inferno emulator window screenshot (high-level)
 * Uses PID-based window selection when PID is provided, otherwise
 * falls back to title-based search with multiple window titles
 */
bool capture_emulator_window(const ScreenshotOptions* options) {
    if (!options || !options->output_path) {
        fprintf(stderr, "Error: Screenshot output path not specified\n");
        return false;
    }

    // Open X11 display once for all attempts
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: Cannot open X11 display (is DISPLAY set?)\n");
        return false;
    }
    Window root = DefaultRootWindow(display);

    bool success = false;

    // STRATEGY 1: PID-based window selection (most precise)
    if (options->expected_pid > 0) {
        printf("Looking for window with PID: %d\n", options->expected_pid);

        // Validate that PID is still running
        if (!validate_pid(options->expected_pid)) {
            fprintf(stderr, "Error: Expected PID %d is not a valid process (may have exited)\n",
                    options->expected_pid);
            XCloseDisplay(display);
            return false;
        }

        // Wait for window to appear with PID match (QUICK attempt)
        const int poll_interval_ms = 100;
        int elapsed = 0;
        int timeout_ms = 2000;  // Only wait 2 seconds for PID match (quick attempt)
        Window window = None;

        printf("Quick PID search (timeout: %d ms)...\n", timeout_ms);

        while (elapsed < timeout_ms) {
            window = find_x11_window_by_pid(display, root, options->expected_pid);
            if (window != None) {
                printf("✓ Found window by PID after %d ms\n", elapsed);
                break;
            }

            if (elapsed % 500 == 0 && elapsed > 0) {
                printf("  PID search: %d/%d ms\n", elapsed, timeout_ms);
            }

            usleep(poll_interval_ms * 1000);
            elapsed += poll_interval_ms;
        }

        // If PID match failed, use window delta tracking to find the newly launched emulator
        // (Window might be created by child process with different PID)
        if (window == None) {
            printf("Note: PID search failed (window may lack _NET_WM_PID property)\n");
            printf("  Using window delta tracking to find newly launched emulator...\n");

            const char* pattern = "Inferno";  // Search for Inferno windows
            const int timeout_ms = 5000;       // Wait up to 5 seconds

            window = wait_for_new_window(display, root, pattern, timeout_ms);

            if (window == None) {
                fprintf(stderr, "Error: No new window appeared after %d ms\n", timeout_ms);
                XCloseDisplay(display);
                return false;
            }

            // Get window title for debug output
            char* title = get_window_title(display, window);
            if (title) {
                printf("  ✓ Capturing new window: '%s'\n", title);
                free(title);
            }
        }

        printf("✓ Found window for screenshot\n");

        // Wait a bit for window to fully render
        if (options->after_frames > 0) {
            int wait_ms = (options->after_frames * 1000) / 60;
            printf("Waiting %d ms for frame rendering...\n", wait_ms);
            usleep(wait_ms * 1000);
        } else {
            usleep(500 * 1000);  // Default 500ms
        }

        // Capture screenshot
        success = capture_x11_window_to_png(display, window, options->output_path);
        XCloseDisplay(display);
        return success;
    }

    // STRATEGY 2: Title-based window selection (fallback when PID == 0)
    printf("Notice: No PID provided, using title-based window selection\n");
    printf("  (This may select the wrong window if multiple emulators are running)\n");

    const char* window_titles[] = {"Inferno", "TaijiOS", NULL};
    const char* custom_title = options->window_title;

    // If custom title is specified, only try that one
    if (custom_title) {
        printf("Looking for window: %s\n", custom_title);
        XCloseDisplay(display);
        return capture_window_when_ready(custom_title, options->output_path, options->timeout_ms);
    }

    // Otherwise, try each title until we find a match
    for (int i = 0; window_titles[i] != NULL; i++) {
        printf("Looking for window: %s\n", window_titles[i]);

        // Calculate timeout based on after_frames
        // Assume 60 FPS, so after_frames * 16.67ms per frame
        int timeout_ms = options->timeout_ms;
        if (options->after_frames > 0) {
            // Add extra time for frame rendering
            timeout_ms += (options->after_frames * 1000) / 60;
        }

        if (capture_window_when_ready(window_titles[i], options->output_path, timeout_ms)) {
            XCloseDisplay(display);
            return true; // Successfully captured
        }

        // If this title failed, try the next one
        printf("Window '%s' not found, trying next option...\n", window_titles[i]);
    }

    // All titles failed
    fprintf(stderr, "Error: No emulator window found (tried 'Inferno' and 'TaijiOS')\n");
    XCloseDisplay(display);
    return false;
}
