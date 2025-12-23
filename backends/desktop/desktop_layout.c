/*
 * Desktop Layout Utilities
 *
 * Minimal utilities for the new two-pass layout system in ir/ir_layout.c
 * All layout computation is now in the IR layer - this file only provides:
 * - Dimension conversion utilities
 * - Text measurement callback for SDL_TTF
 * - Text wrapping utility
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "desktop_internal.h"

// ============================================================================
// DIMENSION CONVERSION UTILITY
// ============================================================================

/**
 * Convert an IR dimension to pixel value
 */
float ir_dimension_to_pixels(IRDimension dimension, float container_size) {
    switch (dimension.type) {
        case IR_DIMENSION_PX:
            return dimension.value;
        case IR_DIMENSION_PERCENT:
            return (dimension.value / 100.0f) * container_size;
        case IR_DIMENSION_AUTO:
            return 0.0f;
        case IR_DIMENSION_FLEX:
            return dimension.value;
        default:
            return 0.0f;
    }
}

// ============================================================================
// TEXT MEASUREMENT FOR TWO-PASS LAYOUT SYSTEM
// ============================================================================

#ifdef ENABLE_SDL3

#define TEXT_LINE_HEIGHT_RATIO 1.2f
#define TEXT_CHAR_WIDTH_RATIO 0.5f

/**
 * Wrap text into lines based on max width
 * Used by both old measure_text_dimensions and new callback
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
 * Text measurement callback for IR layout system
 * This is registered with ir_layout_set_text_measure_callback() and called
 * during intrinsic size computation.
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

#endif // ENABLE_SDL3
