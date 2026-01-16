/**
 * Raylib Layout & Text Measurement
 *
 * Raylib-specific text measurement and wrapping utilities for the IR layout system.
 * This module provides:
 * - Text wrapping using Raylib MeasureTextEx
 * - Text measurement callback for IR layout
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <raylib.h>

#include "../../desktop_internal.h"
#include "raylib_renderer.h"

// ============================================================================
// TEXT MEASUREMENT FOR TWO-PASS LAYOUT SYSTEM
// ============================================================================

/**
 * Wrap text into lines based on max width using Raylib measurement
 *
 * @param text Text to wrap
 * @param max_width Maximum width in pixels
 * @param font Raylib font for accurate measurement
 * @param font_size Font size for measurement
 * @param out_lines Output array of wrapped lines (caller must free)
 * @return Number of lines
 */
int raylib_wrap_text_into_lines(const char* text, float max_width, Font font,
                                 float font_size, char*** out_lines) {
    if (!text || text[0] == '\0') {
        return 0;
    }

    // If no max width, return single line
    if (max_width <= 0) {
        if (out_lines) {
            *out_lines = malloc(sizeof(char*));
            (*out_lines)[0] = strdup(text);
        }
        return 1;
    }

    int line_count = 0;
    char** lines = NULL;
    const char* p = text;

    while (*p) {
        // Find how much fits
        const char* line_end = p;
        const char* last_space = NULL;

        while (*line_end && *line_end != '\n') {
            // Try adding one more character
            size_t len = line_end - p + 1;
            char* test = strndup(p, len);
            Vector2 size = MeasureTextEx(font, test, font_size, 0);
            free(test);

            if (size.x > max_width && line_end != p) {
                // Too wide, break at last space or here
                line_end = last_space ? last_space : line_end;
                break;
            }

            if (*line_end == ' ') last_space = line_end;
            line_end++;
        }

        // Handle newline
        if (*line_end == '\n') line_end++;
        if (line_end == p) line_end++; // Ensure progress

        // Store line if requested
        if (out_lines) {
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            size_t len = line_end - p;
            if (last_space && line_end == last_space) len--; // Skip trailing space
            lines[line_count] = strndup(p, len);
        }

        line_count++;
        p = line_end;
        if (last_space && *p == ' ') p++; // Skip leading space on next line
        last_space = NULL;
    }

    if (out_lines) {
        *out_lines = lines;
    }

    return line_count;
}

/**
 * Heuristic text wrapping fallback when font not available
 */
static int raylib_wrap_text_heuristic(const char* text, float max_width,
                                      float font_size, char*** out_lines) {
    if (!text || text[0] == '\0') {
        return 0;
    }

    size_t text_len = strlen(text);
    float char_width = font_size * TEXT_CHAR_WIDTH_RATIO;
    int chars_per_line = max_width > 0 ? (int)(max_width / char_width) : (int)strlen(text);

    if (chars_per_line <= 0) chars_per_line = 1;

    int line_count = 0;
    char** lines = NULL;
    size_t start = 0;

    while (start < text_len) {
        if (out_lines) {
            size_t len = (start + chars_per_line <= text_len) ? chars_per_line : (text_len - start);
            lines = realloc(lines, sizeof(char*) * (line_count + 1));
            lines[line_count] = strndup(text + start, len);
        }
        line_count++;
        start += chars_per_line;
    }

    if (out_lines) {
        *out_lines = lines;
    }

    return line_count;
}

/**
 * Text measurement callback for IR layout system
 *
 * This callback is invoked during layout to measure text dimensions.
 * It uses Raylib's MeasureTextEx for accurate measurement.
 */
void raylib_text_measure_callback(const char* text, const char* font_family,
                                   float font_size, float max_width,
                                   float* out_width, float* out_height,
                                   void* user_data) {
    (void)user_data;  // Unused

    if (!text || !out_width || !out_height) {
        if (out_width) *out_width = 0;
        if (out_height) *out_height = 0;
        return;
    }

    // Resolve font path
    char font_path[512] = {0};
    bool has_font = raylib_resolve_font_path(font_family, false, false,
                                             font_path, sizeof(font_path));

    // Load font for measurement
    Font font = has_font ? raylib_load_cached_font(font_path, (int)font_size)
                         : GetFontDefault();

    // Measure text
    size_t text_len = strlen(text);
    float char_width = font_size * TEXT_CHAR_WIDTH_RATIO;

    if (max_width > 0) {
        // Wrapping enabled, measure wrapped text
        char** lines = NULL;
        int line_count = raylib_wrap_text_into_lines(text, max_width, font,
                                                     font_size, &lines);

        // Find widest line
        float max_line_width = 0;
        for (int i = 0; i < line_count; i++) {
            Vector2 size = MeasureTextEx(font, lines[i], font_size, 0);
            if (size.x > max_line_width) {
                max_line_width = size.x;
            }
            free(lines[i]);
        }
        free(lines);

        *out_width = (line_count < text_len) ? max_width : text_len * char_width;
        *out_height = line_count * font_size * TEXT_LINE_HEIGHT_RATIO;
    } else {
        // No wrapping, measure single line
        Vector2 size = MeasureTextEx(font, text, font_size, 0);
        *out_width = size.x;
        *out_height = font_size * TEXT_LINE_HEIGHT_RATIO;
    }
}
