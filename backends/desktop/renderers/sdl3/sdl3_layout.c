#define _POSIX_C_SOURCE 200809L

/**
 * SDL3 Layout & Text Measurement
 *
 * SDL3-specific text measurement and wrapping utilities for the IR layout system.
 * This module provides:
 * - Text wrapping using SDL_TTF measurement
 * - Text measurement callback for IR layout
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "../../desktop_internal.h"
#include "sdl3_renderer.h"

// ============================================================================
// TEXT MEASUREMENT FOR TWO-PASS LAYOUT SYSTEM
// ============================================================================

// TEXT_LINE_HEIGHT_RATIO and TEXT_CHAR_WIDTH_RATIO are defined in desktop_internal.h

/**
 * Wrap text into lines based on max width using SDL_TTF measurement
 *
 * @param text Text to wrap
 * @param max_width Maximum width in pixels
 * @param font SDL_TTF font for accurate measurement
 * @param font_size Font size for heuristic fallback
 * @param out_lines Output array of wrapped lines (caller must free)
 * @return Number of lines
 */
int wrap_text_into_lines(const char* text, float max_width, TTF_Font* font,
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

    // Use TTF to measure if font available
    if (font) {
        int line_count = 0;
        char** lines = NULL;
        const char* p = text;

        while (*p) {
            // Find how much fits
            const char* line_end = p;
            const char* last_space = NULL;
            int width = 0;

            while (*line_end && *line_end != '\n') {
                // Try adding one more character
                size_t len = line_end - p + 1;
                char* test = strndup(p, len);
                TTF_GetStringSize(font, test, 0, &width, NULL);
                free(test);

                if (width > max_width && line_end != p) {
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

        if (out_lines) *out_lines = lines;
        return line_count > 0 ? line_count : 1;
    }

    // Heuristic fallback
    float char_width = font_size * TEXT_CHAR_WIDTH_RATIO;
    int chars_per_line = max_width > 0 ? (int)(max_width / char_width) : strlen(text);
    if (chars_per_line < 1) chars_per_line = 1;

    size_t text_len = strlen(text);
    int line_count = (text_len + chars_per_line - 1) / chars_per_line;
    if (line_count < 1) line_count = 1;

    if (out_lines) {
        char** lines = malloc(sizeof(char*) * line_count);
        for (int i = 0; i < line_count; i++) {
            size_t start = i * chars_per_line;
            size_t len = (start + chars_per_line <= text_len) ? chars_per_line : (text_len - start);
            lines[i] = strndup(text + start, len);
        }
        *out_lines = lines;
    }

    return line_count;
}

/**
 * SDL3 text measurement callback for IR layout system
 *
 * This is registered with ir_layout_set_text_measure_callback() and called
 * during intrinsic size computation. Uses SDL_TTF for accurate measurement.
 *
 * @param text Text to measure
 * @param font_size Requested font size
 * @param max_width Maximum width for wrapping (0 = no wrap)
 * @param out_width Output width in pixels
 * @param out_height Output height in pixels
 */
void desktop_text_measure_callback(const char* text, float font_size,
                                   float max_width, float* out_width, float* out_height) {
    *out_width = 0.0f;
    *out_height = 0.0f;

    if (!text || text[0] == '\0') {
        return;
    }

    float line_height = font_size * TEXT_LINE_HEIGHT_RATIO;

    // Try to use actual font measurement via SDL_TTF if a renderer is available
    DesktopIRRenderer* renderer = g_desktop_renderer;
    if (renderer && g_default_font_path[0] != '\0') {
        // Get default font at the requested size
        TTF_Font* font = desktop_ir_get_cached_font(g_default_font_path, (int)font_size);

        if (font) {
            // Wrap text into lines
            char** lines = NULL;
            int line_count = wrap_text_into_lines(text, max_width, font, font_size, &lines);

            if (line_count > 0 && lines) {
                // Measure each line to find maximum width
                float max_line_width = 0.0f;
                for (int i = 0; i < line_count; i++) {
                    int line_w = 0, line_h = 0;
                    TTF_GetStringSize(font, lines[i], 0, &line_w, &line_h);
                    if (line_w > max_line_width) max_line_width = line_w;
                    free(lines[i]);
                }
                free(lines);

                *out_width = max_line_width;
                *out_height = line_count * line_height;
                return;
            }
        }
    }

    // Fallback: Estimate dimensions with wrapping using heuristic
    int line_count = wrap_text_into_lines(text, max_width, NULL, font_size, NULL);

    size_t text_len = strlen(text);
    float char_width = font_size * TEXT_CHAR_WIDTH_RATIO;
    int chars_per_line = max_width > 0 ? (int)(max_width / char_width) : text_len;
    if (chars_per_line < 1) chars_per_line = 1;

    *out_height = line_count * line_height;
    *out_width = (chars_per_line < text_len) ? max_width : text_len * char_width;
}
