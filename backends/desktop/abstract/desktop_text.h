/**
 * Desktop Text - Platform-Agnostic Text Measurement and Wrapping
 *
 * This header defines text measurement callbacks and utilities
 * used by the layout system
 */

#ifndef DESKTOP_TEXT_H
#define DESKTOP_TEXT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Text measurement callback function type
 *
 * Renderers implement this callback to measure text dimensions
 * for layout computation
 *
 * @param text Text to measure
 * @param font_size Font size in pixels
 * @param max_width Maximum width for wrapping (0 = no wrapping)
 * @param out_width Output: measured width in pixels
 * @param out_height Output: measured height in pixels (including wrapped lines)
 */
typedef void (*DesktopTextMeasureCallback)(
    const char* text,
    float font_size,
    float max_width,
    float* out_width,
    float* out_height
);

/**
 * Text wrapping result
 */
typedef struct {
    char** lines;      // Array of wrapped line strings
    int line_count;    // Number of lines
    float width;       // Width of longest line
    float height;      // Total height of all lines
} DesktopTextWrapResult;

/**
 * Free a text wrap result
 */
void desktop_text_wrap_result_free(DesktopTextWrapResult* result);

#ifdef __cplusplus
}
#endif

#endif // DESKTOP_TEXT_H
