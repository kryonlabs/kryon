/**
 * Desktop Fonts - Platform-Agnostic Font Abstraction
 *
 * This header defines platform-agnostic font interfaces that can be
 * implemented by different desktop renderers (SDL3/TTF, raylib, GLFW, etc.)
 */

#ifndef DESKTOP_FONTS_H
#define DESKTOP_FONTS_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque font handle - each renderer provides its own implementation
// SDL3:   DesktopFont* wraps TTF_Font*
// Raylib: DesktopFont* wraps Font
// GLFW:   DesktopFont* wraps custom font structure
typedef struct DesktopFont DesktopFont;

// Font weight enumeration
typedef enum {
    DESKTOP_FONT_WEIGHT_THIN = 100,
    DESKTOP_FONT_WEIGHT_EXTRA_LIGHT = 200,
    DESKTOP_FONT_WEIGHT_LIGHT = 300,
    DESKTOP_FONT_WEIGHT_NORMAL = 400,
    DESKTOP_FONT_WEIGHT_MEDIUM = 500,
    DESKTOP_FONT_WEIGHT_SEMI_BOLD = 600,
    DESKTOP_FONT_WEIGHT_BOLD = 700,
    DESKTOP_FONT_WEIGHT_EXTRA_BOLD = 800,
    DESKTOP_FONT_WEIGHT_BLACK = 900
} DesktopFontWeight;

// Font descriptor for creating fonts
typedef struct {
    const char* family;        // Font family name (e.g., "Arial", "Roboto")
    float size;                // Font size in points
    DesktopFontWeight weight;  // Font weight
    bool italic;               // Italic style
} DesktopFontDescriptor;

// Font operations table - implemented by each renderer
typedef struct {
    /**
     * Load a font from a file path
     * @param path Font file path (e.g., "/usr/share/fonts/truetype/arial.ttf")
     * @param size Font size in pixels
     * @return Opaque font handle, or NULL on failure
     */
    DesktopFont* (*load_font)(const char* path, int size);

    /**
     * Unload a font and free resources
     * @param font Font handle to unload
     */
    void (*unload_font)(DesktopFont* font);

    /**
     * Measure the width of text in pixels
     * @param font Font handle
     * @param text Text to measure
     * @return Width in pixels
     */
    float (*measure_text_width)(DesktopFont* font, const char* text);

    /**
     * Get the height of a font (line height)
     * @param font Font handle
     * @return Height in pixels
     */
    float (*get_font_height)(DesktopFont* font);

    /**
     * Get the ascent of a font (baseline to top)
     * @param font Font handle
     * @return Ascent in pixels
     */
    float (*get_font_ascent)(DesktopFont* font);

    /**
     * Get the descent of a font (baseline to bottom)
     * @param font Font handle
     * @return Descent in pixels (usually negative)
     */
    float (*get_font_descent)(DesktopFont* font);
} DesktopFontOps;

#ifdef __cplusplus
}
#endif

#endif // DESKTOP_FONTS_H
