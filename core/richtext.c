#include "include/kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Rich Text Creation and Destruction
// ============================================================================

kryon_rich_text_t* kryon_rich_text_create(uint16_t text_capacity,
                                          uint16_t run_capacity,
                                          uint16_t line_capacity) {
    kryon_rich_text_t* rt = (kryon_rich_text_t*)malloc(sizeof(kryon_rich_text_t));
    if (!rt) return NULL;

    memset(rt, 0, sizeof(kryon_rich_text_t));

    // Allocate runs array
    rt->runs = (kryon_text_run_t*)malloc(sizeof(kryon_text_run_t) * run_capacity);
    if (!rt->runs) {
        free(rt);
        return NULL;
    }
    memset(rt->runs, 0, sizeof(kryon_text_run_t) * run_capacity);
    rt->run_capacity = run_capacity;
    rt->run_count = 0;

    // Allocate text buffer
    rt->text_buffer = (char*)malloc(text_capacity);
    if (!rt->text_buffer) {
        free(rt->runs);
        free(rt);
        return NULL;
    }
    rt->text_buffer_size = text_capacity;
    rt->text_buffer_used = 0;

    // Allocate URL buffer
    rt->url_buffer = (char*)malloc(text_capacity / 4);
    if (!rt->url_buffer) {
        free(rt->text_buffer);
        free(rt->runs);
        free(rt);
        return NULL;
    }
    rt->url_buffer_size = text_capacity / 4;
    rt->url_buffer_used = 0;

    // Allocate lines array
    rt->lines = (kryon_text_line_t*)malloc(sizeof(kryon_text_line_t) * line_capacity);
    if (!rt->lines) {
        free(rt->url_buffer);
        free(rt->text_buffer);
        free(rt->runs);
        free(rt);
        return NULL;
    }
    memset(rt->lines, 0, sizeof(kryon_text_line_t) * line_capacity);
    rt->line_capacity = line_capacity;
    rt->line_count = 0;

    // Allocate run arrays for each line
    for (uint16_t i = 0; i < line_capacity; i++) {
        rt->lines[i].runs = (kryon_text_run_t**)malloc(sizeof(kryon_text_run_t*) * run_capacity);
        if (!rt->lines[i].runs) {
            // Cleanup already allocated line runs
            for (uint16_t j = 0; j < i; j++) {
                free(rt->lines[j].runs);
            }
            free(rt->lines);
            free(rt->url_buffer);
            free(rt->text_buffer);
            free(rt->runs);
            free(rt);
            return NULL;
        }
        rt->lines[i].run_capacity = run_capacity;
        rt->lines[i].run_count = 0;
    }

    // Set defaults
    rt->text_align = KRYON_ALIGN_START;
    rt->line_height = 0;  // Auto
    rt->paragraph_spacing = 0;
    rt->first_line_indent = 0;
    rt->max_width = 0;    // No limit
    rt->layout_valid = false;

    return rt;
}

void kryon_rich_text_destroy(kryon_rich_text_t* rich_text) {
    if (!rich_text) return;

    // Free line run arrays
    if (rich_text->lines) {
        for (uint16_t i = 0; i < rich_text->line_capacity; i++) {
            if (rich_text->lines[i].runs) {
                free(rich_text->lines[i].runs);
            }
        }
        free(rich_text->lines);
    }

    if (rich_text->runs) free(rich_text->runs);
    if (rich_text->text_buffer) free(rich_text->text_buffer);
    if (rich_text->url_buffer) free(rich_text->url_buffer);
    free(rich_text);
}

// ============================================================================
// Add Text Runs
// ============================================================================

kryon_text_run_t* kryon_rich_text_add_run(kryon_rich_text_t* rich_text,
                                          const char* text,
                                          uint16_t length) {
    if (!rich_text || !text) return NULL;

    // Check if we have capacity
    if (rich_text->run_count >= rich_text->run_capacity) {
        return NULL;  // Run capacity exceeded
    }

    // Check if text buffer has space
    if (rich_text->text_buffer_used + length + 1 > rich_text->text_buffer_size) {
        return NULL;  // Text buffer full
    }

    // Get new run
    kryon_text_run_t* run = &rich_text->runs[rich_text->run_count];
    memset(run, 0, sizeof(kryon_text_run_t));

    // Copy text into shared buffer
    run->offset = rich_text->text_buffer_used;
    run->text = &rich_text->text_buffer[run->offset];
    memcpy(&rich_text->text_buffer[run->offset], text, length);
    rich_text->text_buffer[run->offset + length] = '\0';  // Null terminate
    run->length = length;

    rich_text->text_buffer_used += length + 1;
    rich_text->run_count++;

    // Initialize run properties
    run->style_flags = 0;
    run->fg_color = 0;  // Inherit
    run->bg_color = 0;  // Transparent
    run->font_id = 0;   // Default
    run->font_size = 0; // Inherit
    run->link_url = NULL;
    run->link_url_offset = 0;
    run->link_url_length = 0;
    run->measured_width = 0;
    run->measured_height = 0;
    run->baseline = 0;
    run->next = NULL;

    // Invalidate layout
    rich_text->layout_valid = false;

    return run;
}

// ============================================================================
// Modify Run Styling
// ============================================================================

void kryon_text_run_set_bold(kryon_text_run_t* run, bool enable) {
    if (!run) return;
    if (enable) {
        run->style_flags |= KRYON_TEXT_STYLE_BOLD;
    } else {
        run->style_flags &= ~KRYON_TEXT_STYLE_BOLD;
    }
}

void kryon_text_run_set_italic(kryon_text_run_t* run, bool enable) {
    if (!run) return;
    if (enable) {
        run->style_flags |= KRYON_TEXT_STYLE_ITALIC;
    } else {
        run->style_flags &= ~KRYON_TEXT_STYLE_ITALIC;
    }
}

void kryon_text_run_set_underline(kryon_text_run_t* run, bool enable) {
    if (!run) return;
    if (enable) {
        run->style_flags |= KRYON_TEXT_STYLE_UNDERLINE;
    } else {
        run->style_flags &= ~KRYON_TEXT_STYLE_UNDERLINE;
    }
}

void kryon_text_run_set_strikethrough(kryon_text_run_t* run, bool enable) {
    if (!run) return;
    if (enable) {
        run->style_flags |= KRYON_TEXT_STYLE_STRIKETHROUGH;
    } else {
        run->style_flags &= ~KRYON_TEXT_STYLE_STRIKETHROUGH;
    }
}

void kryon_text_run_set_code(kryon_text_run_t* run, bool enable) {
    if (!run) return;
    if (enable) {
        run->style_flags |= KRYON_TEXT_STYLE_CODE;
        // Set default code styling
        if (run->bg_color == 0) {
            run->bg_color = kryon_color_rgba(240, 240, 240, 255);  // Light gray
        }
        if (run->fg_color == 0) {
            run->fg_color = kryon_color_rgba(51, 51, 51, 255);     // Dark gray
        }
    } else {
        run->style_flags &= ~KRYON_TEXT_STYLE_CODE;
    }
}

void kryon_text_run_set_color(kryon_text_run_t* run, uint32_t fg, uint32_t bg) {
    if (!run) return;
    run->fg_color = fg;
    run->bg_color = bg;
}

void kryon_text_run_set_font(kryon_text_run_t* run, uint16_t font_id, uint8_t size) {
    if (!run) return;
    run->font_id = font_id;
    run->font_size = size;
}

void kryon_text_run_set_link(kryon_text_run_t* run, const char* url) {
    if (!run) return;

    // This is a simplified version - in full implementation,
    // we'd copy URL to the url_buffer like we do with text
    run->style_flags |= KRYON_TEXT_STYLE_LINK;
    run->link_url = url;  // For now, just store pointer

    // Set default link styling
    if (run->fg_color == 0) {
        run->fg_color = kryon_color_rgba(0, 102, 204, 255);  // Blue
    }
    kryon_text_run_set_underline(run, true);
}

// ============================================================================
// Paragraph-Level Properties
// ============================================================================

void kryon_rich_text_set_alignment(kryon_rich_text_t* rich_text, uint8_t align) {
    if (!rich_text) return;
    rich_text->text_align = align;
    rich_text->layout_valid = false;
}

void kryon_rich_text_set_line_height(kryon_rich_text_t* rich_text, uint16_t height) {
    if (!rich_text) return;
    rich_text->line_height = height;
    rich_text->layout_valid = false;
}

void kryon_rich_text_set_first_line_indent(kryon_rich_text_t* rich_text, uint16_t indent) {
    if (!rich_text) return;
    rich_text->first_line_indent = indent;
    rich_text->layout_valid = false;
}

void kryon_rich_text_set_max_width(kryon_rich_text_t* rich_text, kryon_fp_t max_width) {
    if (!rich_text) return;
    rich_text->max_width = max_width;
    rich_text->layout_valid = false;
}

void kryon_rich_text_invalidate_layout(kryon_rich_text_t* rich_text) {
    if (!rich_text) return;
    rich_text->layout_valid = false;
}

// ============================================================================
// Event Handling
// ============================================================================

kryon_text_run_t* kryon_rich_text_find_run_at_point(kryon_rich_text_t* rich_text,
                                                     uint16_t x, uint16_t y,
                                                     uint16_t base_x, uint16_t base_y) {
    if (!rich_text || !rich_text->layout_valid) return NULL;

    uint16_t current_y = base_y;

    for (uint16_t line_idx = 0; line_idx < rich_text->line_count; line_idx++) {
        kryon_text_line_t* line = &rich_text->lines[line_idx];
        uint16_t line_height = line->height;

        // Check if Y is within this line
        if (y >= current_y && y < current_y + line_height) {
            uint16_t current_x = base_x;

            // Find run within line
            for (uint16_t run_idx = 0; run_idx < line->run_count; run_idx++) {
                kryon_text_run_t* run = line->runs[run_idx];

                // Check if X is within this run
                if (x >= current_x && x < current_x + run->measured_width) {
                    return run;
                }

                current_x += run->measured_width;
            }
        }

        current_y += line_height;
    }

    return NULL;  // Not found
}

bool kryon_rich_text_handle_click(kryon_component_t* component,
                                   kryon_rich_text_t* rich_text,
                                   kryon_event_t* event) {
    if (!component || !rich_text || !event) return false;

    // Calculate absolute position
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(component, &abs_x, &abs_y);

    // Find run at click position
    kryon_text_run_t* clicked_run = kryon_rich_text_find_run_at_point(
        rich_text, event->x, event->y,
        KRYON_FP_TO_INT(abs_x), KRYON_FP_TO_INT(abs_y)
    );

    if (clicked_run && (clicked_run->style_flags & KRYON_TEXT_STYLE_LINK)) {
        // Trigger link callback
        if (clicked_run->link_url) {
            fprintf(stderr, "[kryon][richtext] Link clicked: %s\n", clicked_run->link_url);
            // TODO: Call user-defined link handler callback
            return true;  // Event handled
        }
    }

    return false;  // Event not handled
}

// ============================================================================
// Flatten Component Tree to Text Runs
// ============================================================================

// Helper: recursively collect text from component tree with current style context
static void flatten_component_recursive(kryon_component_t* component,
                                        kryon_rich_text_t* rich_text,
                                        uint8_t inherited_style_flags,
                                        uint32_t inherited_fg_color,
                                        uint32_t inherited_bg_color,
                                        uint8_t inherited_font_size) {
    if (!component || !rich_text) return;

    uint8_t current_style = inherited_style_flags;
    uint32_t current_fg = inherited_fg_color;
    uint32_t current_bg = inherited_bg_color;
    uint8_t current_font_size = inherited_font_size;

    // Apply component-specific styles
    if (component->ops == &kryon_bold_ops) {
        current_style |= KRYON_TEXT_STYLE_BOLD;
    } else if (component->ops == &kryon_italic_ops) {
        current_style |= KRYON_TEXT_STYLE_ITALIC;
    } else if (component->ops == &kryon_underline_ops) {
        current_style |= KRYON_TEXT_STYLE_UNDERLINE;
    } else if (component->ops == &kryon_strikethrough_ops) {
        current_style |= KRYON_TEXT_STYLE_STRIKETHROUGH;
    } else if (component->ops == &kryon_code_ops) {
        current_style |= KRYON_TEXT_STYLE_CODE;
        // Apply code styling
        kryon_code_state_t* code_state = (kryon_code_state_t*)component->state;
        if (code_state) {
            if (code_state->bg_color) current_bg = code_state->bg_color;
            else current_bg = kryon_color_rgba(240, 240, 240, 255);  // Default light gray
            if (code_state->fg_color) current_fg = code_state->fg_color;
            else current_fg = kryon_color_rgba(51, 51, 51, 255);     // Default dark gray
        }
    } else if (component->ops == &kryon_link_ops) {
        current_style |= KRYON_TEXT_STYLE_LINK | KRYON_TEXT_STYLE_UNDERLINE;
        kryon_link_state_t* link_state = (kryon_link_state_t*)component->state;
        if (link_state && link_state->color) {
            current_fg = link_state->color;
        } else {
            current_fg = kryon_color_rgba(0, 102, 204, 255);  // Default blue
        }
    } else if (component->ops == &kryon_highlight_ops) {
        kryon_highlight_state_t* hl_state = (kryon_highlight_state_t*)component->state;
        if (hl_state) {
            if (hl_state->bg_color) current_bg = hl_state->bg_color;
            else current_bg = kryon_color_rgba(255, 255, 0, 255);  // Default yellow
            if (hl_state->fg_color) current_fg = hl_state->fg_color;
        }
    } else if (component->ops == &kryon_span_ops) {
        kryon_span_state_t* span_state = (kryon_span_state_t*)component->state;
        if (span_state) {
            if (span_state->fg_color) current_fg = span_state->fg_color;
            if (span_state->bg_color) current_bg = span_state->bg_color;
            if (span_state->font_size) current_font_size = span_state->font_size;
        }
    }

    // If this component has a text_state with text, add it as a run
    // Skip for Code/Link/Highlight/Span which have special state structures without text fields
    // These components are containers - their children provide the text content
    if (component->state &&
        component->ops != &kryon_code_ops &&
        component->ops != &kryon_link_ops &&
        component->ops != &kryon_highlight_ops &&
        component->ops != &kryon_span_ops) {
        kryon_text_state_t* text_state = (kryon_text_state_t*)component->state;
        // Check if this is a text component or has text content
        if (text_state->text && strlen(text_state->text) > 0) {
            uint16_t text_len = strlen(text_state->text);

            if (getenv("KRYON_TRACE_LAYOUT")) {
                fprintf(stderr, "[kryon][flatten]   Found text: '%s' (len=%d, styles=0x%02x)\n",
                        text_state->text, text_len, current_style);
            }

            kryon_text_run_t* run = kryon_rich_text_add_run(rich_text, text_state->text, text_len);
            if (run) {
                run->style_flags = current_style;
                run->fg_color = current_fg;
                run->bg_color = current_bg;
                run->font_size = current_font_size;
                run->font_id = text_state->font_id;  // Copy font ID for monospace/code

                // If this is a link, store the URL
                if (component->ops == &kryon_link_ops) {
                    kryon_link_state_t* link_state = (kryon_link_state_t*)component->state;
                    if (link_state && link_state->url) {
                        kryon_text_run_set_link(run, link_state->url);
                    }
                }
            }
        }
    }

    // Recurse into children with inherited styles
    for (uint8_t i = 0; i < component->child_count; i++) {
        flatten_component_recursive(component->children[i], rich_text,
                                    current_style, current_fg, current_bg, current_font_size);
    }
}

void kryon_text_flatten_children_to_runs(kryon_component_t* text_component,
                                         kryon_rich_text_t* rich_text) {
    if (!text_component || !rich_text) return;

    if (getenv("KRYON_TRACE_LAYOUT")) {
        fprintf(stderr, "[kryon][flatten] Starting with %d children\n", text_component->child_count);
    }

    // Clear existing runs
    rich_text->run_count = 0;
    rich_text->text_buffer_used = 0;
    rich_text->url_buffer_used = 0;
    rich_text->layout_valid = false;

    // Get default styling from text component
    kryon_text_state_t* text_state = (kryon_text_state_t*)text_component->state;
    uint8_t base_style = 0;
    uint32_t base_fg = text_component->text_color;
    uint32_t base_bg = 0;
    uint8_t base_font_size = 16;  // Default

    if (text_state) {
        if (text_state->font_weight == KRYON_FONT_WEIGHT_BOLD) {
            base_style |= KRYON_TEXT_STYLE_BOLD;
        }
        if (text_state->font_style == KRYON_FONT_STYLE_ITALIC) {
            base_style |= KRYON_TEXT_STYLE_ITALIC;
        }
        if (text_state->font_size > 0) {
            base_font_size = text_state->font_size;
        }
    }

    // Flatten all children
    for (uint8_t i = 0; i < text_component->child_count; i++) {
        flatten_component_recursive(text_component->children[i], rich_text,
                                    base_style, base_fg, base_bg, base_font_size);
    }

    if (getenv("KRYON_TRACE_LAYOUT")) {
        fprintf(stderr, "[kryon][flatten] Finished: %d runs created\n", rich_text->run_count);
    }
}

// ============================================================================
// Line Breaking and Layout
// ============================================================================

// Helper: measure run width with font styling using renderer callback
static uint16_t measure_run_width(kryon_text_run_t* run, uint8_t default_font_size,
                                  kryon_renderer_t* renderer) {
    if (!run || !run->text || run->length == 0) return 0;

    uint8_t font_size = run->font_size > 0 ? run->font_size : default_font_size;
    uint8_t font_weight = (run->style_flags & KRYON_TEXT_STYLE_BOLD)
                         ? KRYON_FONT_WEIGHT_BOLD : KRYON_FONT_WEIGHT_NORMAL;
    uint8_t font_style = (run->style_flags & KRYON_TEXT_STYLE_ITALIC)
                        ? KRYON_FONT_STYLE_ITALIC : KRYON_FONT_STYLE_NORMAL;

    uint16_t width = 0;
    uint16_t height = 0;

    // Use renderer callback for accurate measurement if available
    if (renderer != NULL && renderer->measure_text != NULL) {
        renderer->measure_text(run->text, font_size, font_weight, font_style, run->font_id,
                              &width, &height, renderer->backend_data);

        if (getenv("KRYON_TRACE_LAYOUT")) {
            fprintf(stderr, "[kryon][measure] text='%s' font_size=%d weight=%d style=%d font_id=%d width=%d (accurate)\n",
                    run->text, font_size, font_weight, font_style, run->font_id, width);
        }
    } else {
        // Debug: why no callback?
        if (getenv("KRYON_TRACE_LAYOUT")) {
            fprintf(stderr, "[kryon][measure] text='%s' renderer=%p measure_text=%p\n",
                    run->text, (void*)renderer, (void*)(renderer ? renderer->measure_text : NULL));
        }
        // Fallback to crude approximation if no callback available
        uint16_t space_count = 0;
        uint16_t non_space_count = 0;
        for (uint16_t i = 0; i < run->length; i++) {
            if (run->text[i] == ' ') space_count++;
            else non_space_count++;
        }

        float char_width = font_size * 0.45f;
        float space_width = font_size * 0.25f;

        if (run->style_flags & KRYON_TEXT_STYLE_BOLD) {
            char_width *= 1.15f;
            space_width *= 1.15f;
        }
        if (run->style_flags & KRYON_TEXT_STYLE_ITALIC) {
            char_width *= 0.90f;
            space_width *= 0.90f;
        }

        width = (uint16_t)(non_space_count * char_width + space_count * space_width);

        if (getenv("KRYON_TRACE_LAYOUT")) {
            fprintf(stderr, "[kryon][measure] text='%s' font_size=%d width=%d (approximation)\n",
                    run->text, font_size, width);
        }
    }

    return width;
}

// Perform line breaking and layout
void kryon_rich_text_layout_lines(kryon_rich_text_t* rich_text,
                                  uint16_t max_width,
                                  uint8_t default_font_size,
                                  kryon_renderer_t* renderer) {
    if (!rich_text) return;

    // If already valid, skip
    if (rich_text->layout_valid) return;

    // Clear existing lines
    for (uint16_t i = 0; i < rich_text->line_count; i++) {
        rich_text->lines[i].run_count = 0;
    }
    rich_text->line_count = 0;

    // If no runs, nothing to layout
    if (rich_text->run_count == 0) {
        rich_text->layout_valid = true;
        return;
    }

    // Create first line
    if (rich_text->line_capacity == 0) {
        rich_text->layout_valid = true;
        return;
    }

    kryon_text_line_t* current_line = &rich_text->lines[0];
    rich_text->line_count = 1;
    current_line->run_count = 0;
    current_line->width = 0;
    current_line->height = 0;
    current_line->baseline = 0;
    current_line->ends_with_soft_break = false;
    current_line->ends_with_hard_break = false;

    uint16_t current_line_width = 0;
    uint16_t effective_max_width = max_width > 0 ? max_width : 10000;  // Effectively no limit

    // Simple greedy line breaking - add runs until they don't fit
    for (uint16_t run_idx = 0; run_idx < rich_text->run_count; run_idx++) {
        kryon_text_run_t* run = &rich_text->runs[run_idx];

        // Measure this run
        uint8_t font_size = run->font_size > 0 ? run->font_size : default_font_size;
        run->measured_width = measure_run_width(run, default_font_size, renderer);
        run->measured_height = (uint16_t)(font_size * 1.2f);  // Line height = 1.2x font size
        run->baseline = (uint16_t)(font_size * 0.8f);  // Approximate baseline

        // Check if run fits on current line
        if (current_line_width + run->measured_width > effective_max_width &&
            current_line->run_count > 0) {

            // Finalize current line
            current_line->width = current_line_width;
            current_line->ends_with_soft_break = true;

            // Create new line
            if (rich_text->line_count < rich_text->line_capacity) {
                current_line = &rich_text->lines[rich_text->line_count];
                rich_text->line_count++;
                current_line->run_count = 0;
                current_line->width = 0;
                current_line->height = 0;
                current_line->baseline = 0;
                current_line->ends_with_soft_break = false;
                current_line->ends_with_hard_break = false;
                current_line_width = 0;
            } else {
                // Line capacity exceeded - stop adding lines
                break;
            }
        }

        // Add run to current line
        if (current_line->run_count < current_line->run_capacity) {
            current_line->runs[current_line->run_count] = run;
            current_line->run_count++;
            current_line_width += run->measured_width;

            // Update line height and baseline (max of all runs)
            if (run->measured_height > current_line->height) {
                current_line->height = run->measured_height;
            }
            if (run->baseline > current_line->baseline) {
                current_line->baseline = run->baseline;
            }
        }
    }

    // Finalize last line
    if (current_line->run_count > 0) {
        current_line->width = current_line_width;
    }

    rich_text->layout_valid = true;
}

// ============================================================================
// Rich Text Rendering
// ============================================================================

void kryon_rich_text_render(kryon_component_t* component,
                            kryon_rich_text_t* rich_text,
                            kryon_cmd_buf_t* buf) {
    if (!component || !rich_text || !buf) return;

    // Ensure layout is valid
    if (!rich_text->layout_valid) {
        uint16_t max_width = KRYON_FP_TO_INT(component->width);
        kryon_renderer_t* renderer = kryon_get_global_renderer();
        kryon_rich_text_layout_lines(rich_text, max_width, 16, renderer);
    }

    // Calculate absolute position
    kryon_fp_t abs_x, abs_y;
    calculate_absolute_position(component, &abs_x, &abs_y);
    int16_t base_x = KRYON_FP_TO_INT(abs_x);
    int16_t base_y = KRYON_FP_TO_INT(abs_y);

    int16_t current_y = base_y;

    // Render each line
    for (uint16_t line_idx = 0; line_idx < rich_text->line_count; line_idx++) {
        kryon_text_line_t* line = &rich_text->lines[line_idx];

        // Calculate X offset for text alignment
        int16_t line_x_offset = 0;
        uint16_t available_width = KRYON_FP_TO_INT(component->width);

        switch (rich_text->text_align) {
            case KRYON_ALIGN_CENTER:
                line_x_offset = (int16_t)((available_width - line->width) / 2);
                break;
            case KRYON_ALIGN_END:  // Right align
                line_x_offset = (int16_t)(available_width - line->width);
                break;
            case KRYON_ALIGN_START:  // Left align
            default:
                line_x_offset = 0;
                break;
        }

        int16_t current_x = base_x + line_x_offset;

        // Apply first-line indent
        if (line_idx == 0 && rich_text->first_line_indent > 0) {
            current_x += (int16_t)rich_text->first_line_indent;
        }

        // Render each run in the line
        for (uint16_t run_idx = 0; run_idx < line->run_count; run_idx++) {
            kryon_text_run_t* run = line->runs[run_idx];

            // Calculate baseline-aligned Y position
            int16_t baseline_offset = (int16_t)(line->baseline - run->baseline);
            int16_t run_y = current_y + baseline_offset;

            // Draw background if specified
            if (run->bg_color & 0xFF) {  // Check alpha != 0
                kryon_draw_rect(buf, current_x, run_y,
                               run->measured_width, run->measured_height,
                               run->bg_color);
            }

            // Determine font weight and style
            uint8_t font_weight = (run->style_flags & KRYON_TEXT_STYLE_BOLD)
                                 ? KRYON_FONT_WEIGHT_BOLD : KRYON_FONT_WEIGHT_NORMAL;
            uint8_t font_style = (run->style_flags & KRYON_TEXT_STYLE_ITALIC)
                                ? KRYON_FONT_STYLE_ITALIC : KRYON_FONT_STYLE_NORMAL;

            // Get text color
            uint32_t text_color = run->fg_color ? run->fg_color
                                 : kryon_component_get_effective_text_color(component);
            uint8_t font_size = run->font_size > 0 ? run->font_size : 16;

            // Draw text
            kryon_draw_text(buf, run->text, current_x, run_y,
                           run->font_id, font_size, font_weight, font_style,
                           text_color);

            // Draw underline if specified
            if (run->style_flags & KRYON_TEXT_STYLE_UNDERLINE) {
                int16_t underline_y = run_y + (int16_t)run->measured_height - 2;
                kryon_draw_line(buf, current_x, underline_y,
                               current_x + (int16_t)run->measured_width, underline_y,
                               text_color);
            }

            // Draw strikethrough if specified
            if (run->style_flags & KRYON_TEXT_STYLE_STRIKETHROUGH) {
                int16_t strike_y = run_y + (int16_t)(run->measured_height / 2);
                kryon_draw_line(buf, current_x, strike_y,
                               current_x + (int16_t)run->measured_width, strike_y,
                               text_color);
            }

            // Advance X for next run
            current_x += (int16_t)run->measured_width;
        }

        // Advance Y for next line
        uint16_t line_height = rich_text->line_height > 0
                              ? rich_text->line_height
                              : line->height;
        current_y += (int16_t)line_height;
    }
}

