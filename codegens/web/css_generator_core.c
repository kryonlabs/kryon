/**
 * @file css_generator_core.c
 * @brief Core CSS generation functions implementation
 *
 * Implements the core CSS generation functionality using IRStringBuilder.
 */

#define _GNU_SOURCE
#include "css_generator_core.h"
#include "css_property_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

// ============================================================================
// CSS Generator Context
// ============================================================================

CSSGenContext* css_gen_context_create(IRStringBuilder* builder) {
    CSSGenContext* ctx = calloc(1, sizeof(CSSGenContext));
    if (!ctx) return NULL;

    if (builder) {
        ctx->builder = builder;
    } else {
        ctx->builder = ir_sb_create(4096);
        if (!ctx->builder) {
            free(ctx);
            return NULL;
        }
    }

    ctx->pretty_print = true;
    ctx->has_output = false;
    return ctx;
}

void css_gen_context_free(CSSGenContext* ctx) {
    if (!ctx) return;
    // Note: We don't free the builder here as it may be owned externally
    // The caller should free the builder if needed
    free(ctx);
}

IRStringBuilder* css_gen_get_builder(CSSGenContext* ctx) {
    return ctx ? ctx->builder : NULL;
}

bool css_gen_has_output(CSSGenContext* ctx) {
    return ctx ? ctx->has_output : false;
}

void css_gen_reset_output(CSSGenContext* ctx) {
    if (ctx) ctx->has_output = false;
}

void css_gen_set_pretty_print(CSSGenContext* ctx, bool pretty) {
    if (ctx) ctx->pretty_print = pretty;
}

// ============================================================================
// Core Write Functions
// ============================================================================

bool css_gen_write(CSSGenContext* ctx, const char* str) {
    if (!ctx || !str) return false;
    ctx->has_output = true;
    return ir_sb_append(ctx->builder, str);
}

bool css_gen_writef(CSSGenContext* ctx, const char* format, ...) {
    if (!ctx || !format) return false;

    va_list args;
    va_start(args, format);

    char* result = NULL;
    if (vasprintf(&result, format, args) < 0) {
        va_end(args);
        return false;
    }
    va_end(args);

    bool success = css_gen_write(ctx, result);
    free(result);
    return success;
}

bool css_gen_write_line(CSSGenContext* ctx, const char* line) {
    if (!ctx || !line) return false;
    ctx->has_output = true;
    return ir_sb_append_line(ctx->builder, line);
}

bool css_gen_write_linef(CSSGenContext* ctx, const char* format, ...) {
    if (!ctx || !format) return false;

    va_list args;
    va_start(args, format);

    char* result = NULL;
    if (vasprintf(&result, format, args) < 0) {
        va_end(args);
        return false;
    }
    va_end(args);

    bool success = ir_sb_append_line(ctx->builder, result);
    free(result);
    return success;
}

bool css_gen_write_indented(CSSGenContext* ctx, int level, const char* format, ...) {
    if (!ctx || !format) return false;

    ctx->has_output = true;

    // Add indentation
    ir_sb_indent(ctx->builder, level);

    va_list args;
    va_start(args, format);

    char* result = NULL;
    if (vasprintf(&result, format, args) < 0) {
        va_end(args);
        return false;
    }
    va_end(args);

    bool success = ir_sb_append(ctx->builder, result);
    free(result);
    return success;
}

bool css_gen_write_property(CSSGenContext* ctx, const char* property, const char* value) {
    if (!ctx || !property || !value) return false;
    ctx->has_output = true;
    ir_sb_indent(ctx->builder, 1);
    ir_sb_appendf(ctx->builder, "%s: %s;\n", property, value);
    return true;
}

bool css_gen_write_propertyf(CSSGenContext* ctx, const char* property, const char* format, ...) {
    if (!ctx || !property || !format) return false;

    ctx->has_output = true;
    ir_sb_indent(ctx->builder, 1);
    ir_sb_append(ctx->builder, property);
    ir_sb_append(ctx->builder, ": ");

    va_list args;
    va_start(args, format);

    char* result = NULL;
    if (vasprintf(&result, format, args) < 0) {
        va_end(args);
        return false;
    }
    va_end(args);

    ir_sb_append(ctx->builder, result);
    ir_sb_append_line(ctx->builder, ";");
    free(result);
    return true;
}

bool css_gen_write_block_open(CSSGenContext* ctx, const char* selector) {
    if (!ctx || !selector) return false;
    ctx->has_output = true;
    ir_sb_appendf(ctx->builder, "%s {\n", selector);
    return true;
}

bool css_gen_write_block_close(CSSGenContext* ctx, bool newline_after) {
    if (!ctx) return false;
    ctx->has_output = true;
    if (newline_after) {
        ir_sb_append_line(ctx->builder, "}");
    } else {
        ir_sb_append(ctx->builder, "}\n");
    }
    return true;
}

bool css_gen_write_comment(CSSGenContext* ctx, const char* comment) {
    if (!ctx || !comment) return false;
    ctx->has_output = true;
    ir_sb_appendf(ctx->builder, "/* %s */\n", comment);
    return true;
}

// ============================================================================
// Property Generation Functions
// ============================================================================

bool css_gen_generate_dimension(CSSGenContext* ctx, const char* property, IRDimension* dimension) {
    if (!ctx || !property || !dimension) return false;

    if (dimension->type == IR_DIMENSION_AUTO) return false;

    const char* value = css_format_dimension(dimension);
    return css_gen_write_property(ctx, property, value);
}

bool css_gen_generate_color(CSSGenContext* ctx, const char* property, IRColor* color,
                            bool skip_fully_transparent) {
    if (!ctx || !property || !color) return false;

    if (skip_fully_transparent && color->type == IR_COLOR_TRANSPARENT) return false;

    const char* value = css_format_color(color);
    return css_gen_write_property(ctx, property, value);
}

bool css_gen_generate_alignment(CSSGenContext* ctx, const char* property, IRAlignment alignment,
                                bool skip_default) {
    if (!ctx || !property) return false;

    if (skip_default && alignment == IR_ALIGNMENT_START) return false;

    const char* value = css_format_alignment(alignment);
    return css_gen_write_property(ctx, property, value);
}

bool css_gen_generate_spacing(CSSGenContext* ctx, const char* property_base, IRSpacing* spacing) {
    if (!ctx || !property_base || !spacing) return false;

    uint8_t flags = spacing->set_flags;
    if (flags == 0) return false;

    float top = spacing->top;
    float right = spacing->right;
    float bottom = spacing->bottom;
    float left = spacing->left;

    if (flags == IR_SPACING_SET_ALL) {
        // All sides set - use shortest shorthand
        if (top == right && right == bottom && bottom == left) {
            // All same: "0" or "16px"
            if (top == 0) {
                css_gen_write_property(ctx, property_base, "0");
            } else {
                css_gen_write_propertyf(ctx, property_base, "%s", css_format_px(top));
            }
        } else if (top == bottom && right == left) {
            // top/bottom same, left/right same: "10px 20px"
            css_gen_write_propertyf(ctx, property_base, "%s %s",
                                   css_format_px(top), css_format_px(right));
        } else if (right == left) {
            // top different, left/right same: "10px 20px 30px"
            css_gen_write_propertyf(ctx, property_base, "%s %s %s",
                                   css_format_px(top), css_format_px(right), css_format_px(bottom));
        } else {
            // All different: "10px 20px 30px 40px"
            css_gen_write_propertyf(ctx, property_base, "%s %s %s %s",
                                   css_format_px(top), css_format_px(right),
                                   css_format_px(bottom), css_format_px(left));
        }
    } else {
        // Individual properties set - output only those that are set
        char prop_buffer[32];
        const size_t base_len = strlen(property_base);

        if (flags & IR_SPACING_SET_TOP) {
            snprintf(prop_buffer, sizeof(prop_buffer), "%s-top", property_base);
            if (top == IR_SPACING_AUTO) {
                css_gen_write_property(ctx, prop_buffer, "auto");
            } else {
                css_gen_write_propertyf(ctx, prop_buffer, "%s", css_format_px(top));
            }
        }
        if (flags & IR_SPACING_SET_RIGHT) {
            snprintf(prop_buffer, sizeof(prop_buffer), "%s-right", property_base);
            if (right == IR_SPACING_AUTO) {
                css_gen_write_property(ctx, prop_buffer, "auto");
            } else {
                css_gen_write_propertyf(ctx, prop_buffer, "%s", css_format_px(right));
            }
        }
        if (flags & IR_SPACING_SET_BOTTOM) {
            snprintf(prop_buffer, sizeof(prop_buffer), "%s-bottom", property_base);
            if (bottom == IR_SPACING_AUTO) {
                css_gen_write_property(ctx, prop_buffer, "auto");
            } else {
                css_gen_write_propertyf(ctx, prop_buffer, "%s", css_format_px(bottom));
            }
        }
        if (flags & IR_SPACING_SET_LEFT) {
            snprintf(prop_buffer, sizeof(prop_buffer), "%s-left", property_base);
            if (left == IR_SPACING_AUTO) {
                css_gen_write_property(ctx, prop_buffer, "auto");
            } else {
                css_gen_write_propertyf(ctx, prop_buffer, "%s", css_format_px(left));
            }
        }
    }

    return true;
}

bool css_gen_generate_border(CSSGenContext* ctx, IRBorder* border) {
    if (!ctx || !border) return false;

    // Uniform border
    if (border->width > 0) {
        const char* color_str = css_format_color(&border->color);
        css_gen_write_propertyf(ctx, "border", "%s solid %s",
                               css_format_px(border->width), color_str);
    }

    // Per-side borders
    const char* color = css_format_color(&border->color);
    if (border->width_top > 0) {
        css_gen_write_propertyf(ctx, "border-top", "%s solid %s",
                               css_format_px(border->width_top), color);
    }
    if (border->width_right > 0) {
        css_gen_write_propertyf(ctx, "border-right", "%s solid %s",
                               css_format_px(border->width_right), color);
    }
    if (border->width_bottom > 0) {
        css_gen_write_propertyf(ctx, "border-bottom", "%s solid %s",
                               css_format_px(border->width_bottom), color);
    }
    if (border->width_left > 0) {
        css_gen_write_propertyf(ctx, "border-left", "%s solid %s",
                               css_format_px(border->width_left), color);
    }

    // Border radius
    if (border->radius > 0) {
        css_gen_write_propertyf(ctx, "border-radius", "%upx", border->radius);
    }

    return true;
}

bool css_gen_generate_typography(CSSGenContext* ctx, IRTypography* font) {
    if (!ctx || !font) return false;

    bool has_output = false;

    // Font size
    if (font->size > 0) {
        css_gen_write_propertyf(ctx, "font-size", "%s", css_format_px(font->size));
        has_output = true;
    }

    // Font family
    if (font->family) {
        css_gen_write_property(ctx, "font-family", font->family);
        has_output = true;
    }

    // Font color (skip fully transparent)
    if (font->color.type == IR_COLOR_SOLID && font->color.data.a > 0) {
        const char* color_str = css_format_color(&font->color);
        css_gen_write_property(ctx, "color", color_str);
        has_output = true;
    }

    // Font weight - numeric value takes precedence over bold flag
    if (font->weight > 0) {
        css_gen_write_propertyf(ctx, "font-weight", "%u", font->weight);
        has_output = true;
    } else if (font->bold) {
        css_gen_write_property(ctx, "font-weight", "bold");
        has_output = true;
    }

    // Font style
    if (font->italic) {
        css_gen_write_property(ctx, "font-style", "italic");
        has_output = true;
    }

    // Line height (skip default 1.5)
    if (font->line_height > 0 &&
        (font->line_height < 1.49f || font->line_height > 1.51f)) {
        css_gen_write_propertyf(ctx, "line-height", "%s", css_format_float(font->line_height));
        has_output = true;
    }

    // Letter spacing
    if (font->letter_spacing != 0) {
        css_gen_write_propertyf(ctx, "letter-spacing", "%s", css_format_px(font->letter_spacing));
        has_output = true;
    }

    // Word spacing
    if (font->word_spacing != 0) {
        css_gen_write_propertyf(ctx, "word-spacing", "%s", css_format_px(font->word_spacing));
        has_output = true;
    }

    // Text alignment (skip default left)
    if (font->align != IR_TEXT_ALIGN_LEFT) {
        const char* align_str = css_format_text_align(font->align);
        css_gen_write_property(ctx, "text-align", align_str);
        has_output = true;
    }

    // Text decoration
    if (font->decoration != IR_TEXT_DECORATION_NONE) {
        char decoration_buf[64] = "";
        int pos = 0;

        if (font->decoration & IR_TEXT_DECORATION_UNDERLINE) {
            pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, "underline");
        }
        if (font->decoration & IR_TEXT_DECORATION_OVERLINE) {
            if (pos > 0) decoration_buf[pos++] = ' ';
            pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, "overline");
        }
        if (font->decoration & IR_TEXT_DECORATION_LINE_THROUGH) {
            if (pos > 0) decoration_buf[pos++] = ' ';
            pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, "line-through");
        }

        if (pos > 0) {
            css_gen_write_property(ctx, "text-decoration", decoration_buf);
            has_output = true;
        }
    }

    return has_output;
}

bool css_gen_generate_transform(CSSGenContext* ctx, IRTransform* transform) {
    if (!ctx || !transform) return false;

    bool has_transform = (transform->translate_x != 0 || transform->translate_y != 0 ||
                         transform->scale_x != 1 || transform->scale_y != 1 ||
                         transform->rotate != 0);

    if (!has_transform) return false;

    char transform_buf[256] = "";
    int pos = 0;

    if (transform->translate_x != 0 || transform->translate_y != 0) {
        pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                       "translate(%.2fpx, %.2fpx)",
                       transform->translate_x, transform->translate_y);
    }

    if (transform->scale_x != 1 || transform->scale_y != 1) {
        if (pos > 0) transform_buf[pos++] = ' ';
        pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                       "scale(%.2f, %.2f)",
                       transform->scale_x, transform->scale_y);
    }

    if (transform->rotate != 0) {
        if (pos > 0) transform_buf[pos++] = ' ';
        pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                       "rotate(%.2fdeg)", transform->rotate);
    }

    return css_gen_write_property(ctx, "transform", transform_buf);
}

bool css_gen_generate_box_shadow(CSSGenContext* ctx, IRBoxShadow* shadow) {
    if (!ctx || !shadow || !shadow->enabled) return false;

    const char* color_str = css_format_color(&shadow->color);

    if (shadow->inset) {
        return css_gen_write_propertyf(ctx, "box-shadow", "inset %.1fpx %.1fpx %.1fpx %.1fpx %s",
                                      shadow->offset_x, shadow->offset_y,
                                      shadow->blur_radius, shadow->spread_radius, color_str);
    } else {
        return css_gen_write_propertyf(ctx, "box-shadow", "%.1fpx %.1fpx %.1fpx %.1fpx %s",
                                      shadow->offset_x, shadow->offset_y,
                                      shadow->blur_radius, shadow->spread_radius, color_str);
    }
}

bool css_gen_generate_text_shadow(CSSGenContext* ctx, IRTextShadow* shadow) {
    if (!ctx || !shadow || !shadow->enabled) return false;

    const char* color_str = css_format_color(&shadow->color);
    return css_gen_write_propertyf(ctx, "text-shadow", "%.1fpx %.1fpx %.1fpx %s",
                                  shadow->offset_x, shadow->offset_y,
                                  shadow->blur_radius, color_str);
}

bool css_gen_generate_filters(CSSGenContext* ctx, IRFilter* filters, uint8_t count) {
    if (!ctx || !filters || count == 0) return false;

    ir_sb_indent(ctx->builder, 1);
    ir_sb_append(ctx->builder, "filter:");

    for (uint8_t i = 0; i < count; i++) {
        IRFilter* filter = &filters[i];

        switch (filter->type) {
            case IR_FILTER_BLUR:
                ir_sb_appendf(ctx->builder, " blur(%.1fpx)", filter->value);
                break;
            case IR_FILTER_BRIGHTNESS:
                ir_sb_appendf(ctx->builder, " brightness(%.2f)", filter->value);
                break;
            case IR_FILTER_CONTRAST:
                ir_sb_appendf(ctx->builder, " contrast(%.2f)", filter->value);
                break;
            case IR_FILTER_GRAYSCALE:
                ir_sb_appendf(ctx->builder, " grayscale(%.2f)", filter->value);
                break;
            case IR_FILTER_HUE_ROTATE:
                ir_sb_appendf(ctx->builder, " hue-rotate(%.0fdeg)", filter->value);
                break;
            case IR_FILTER_INVERT:
                ir_sb_appendf(ctx->builder, " invert(%.2f)", filter->value);
                break;
            case IR_FILTER_OPACITY:
                ir_sb_appendf(ctx->builder, " opacity(%.2f)", filter->value);
                break;
            case IR_FILTER_SATURATE:
                ir_sb_appendf(ctx->builder, " saturate(%.2f)", filter->value);
                break;
            case IR_FILTER_SEPIA:
                ir_sb_appendf(ctx->builder, " sepia(%.2f)", filter->value);
                break;
        }
    }

    ir_sb_append_line(ctx->builder, ";");
    ctx->has_output = true;
    return true;
}

bool css_gen_generate_flexbox(CSSGenContext* ctx, IRFlexbox* flex, IRComponentType component_type) {
    if (!ctx || !flex) return false;

    bool has_output = false;

    // Flex direction based on component type
    if (component_type == IR_COMPONENT_ROW) {
        css_gen_write_property(ctx, "flex-direction", "row");
        has_output = true;
    } else if (component_type == IR_COMPONENT_COLUMN) {
        css_gen_write_property(ctx, "flex-direction", "column");
        has_output = true;
    }

    // Flex wrap
    if (flex->wrap) {
        css_gen_write_property(ctx, "flex-wrap", "wrap");
        has_output = true;
    }

    // Gap
    if (flex->gap > 0) {
        css_gen_write_propertyf(ctx, "gap", "%upx", flex->gap);
        has_output = true;
    }

    // Justify content
    if (flex->justify_content != IR_ALIGNMENT_START) {
        const char* jc = css_format_alignment(flex->justify_content);
        css_gen_write_property(ctx, "justify-content", jc);
        has_output = true;
    }

    // Align items (skip defaults: stretch for column, flex-start for row)
    if (flex->cross_axis != IR_ALIGNMENT_START && flex->cross_axis != IR_ALIGNMENT_STRETCH) {
        const char* ai = css_format_alignment(flex->cross_axis);
        css_gen_write_property(ctx, "align-items", ai);
        has_output = true;
    }

    // Flex grow
    if (flex->grow > 0) {
        css_gen_write_propertyf(ctx, "flex-grow", "%u", flex->grow);
        has_output = true;
    }

    // Flex shrink (only output if not default of 1)
    if (flex->shrink != 1) {
        css_gen_write_propertyf(ctx, "flex-shrink", "%u", flex->shrink);
        has_output = true;
    }

    // Direction property (CSS direction, not flex-direction)
    if (flex->base_direction != IR_DIRECTION_AUTO) {
        const char* dir = css_format_direction((IRDirection)flex->base_direction);
        css_gen_write_property(ctx, "direction", dir);
        has_output = true;
    }

    // Unicode bidi
    if (flex->unicode_bidi != IR_UNICODE_BIDI_NORMAL) {
        const char* bidi = css_format_unicode_bidi((IRUnicodeBidi)flex->unicode_bidi);
        css_gen_write_property(ctx, "unicode-bidi", bidi);
        has_output = true;
    }

    return has_output;
}

bool css_gen_generate_grid(CSSGenContext* ctx, IRGrid* grid, bool is_inline) {
    if (!ctx || !grid) return false;

    bool has_output = false;

    // Display
    if (is_inline) {
        css_gen_write_property(ctx, "display", "inline-grid");
    } else {
        css_gen_write_property(ctx, "display", "grid");
    }
    has_output = true;

    // Grid template columns
    if (grid->use_column_repeat && grid->column_repeat.mode != IR_GRID_REPEAT_NONE) {
        ir_sb_indent(ctx->builder, 1);
        ir_sb_append(ctx->builder, "grid-template-columns: repeat(");

        switch (grid->column_repeat.mode) {
            case IR_GRID_REPEAT_AUTO_FIT:
                ir_sb_append(ctx->builder, "auto-fit");
                break;
            case IR_GRID_REPEAT_AUTO_FILL:
                ir_sb_append(ctx->builder, "auto-fill");
                break;
            case IR_GRID_REPEAT_COUNT:
                ir_sb_appendf(ctx->builder, "%d", grid->column_repeat.count);
                break;
            default:
                break;
        }

        ir_sb_append(ctx->builder, ", ");

        if (grid->column_repeat.has_minmax) {
            char min_str[32], max_str[32];
            css_format_grid_track_minmax(grid->column_repeat.minmax.min_type,
                                        grid->column_repeat.minmax.min_value,
                                        min_str, sizeof(min_str));
            css_format_grid_track_minmax(grid->column_repeat.minmax.max_type,
                                        grid->column_repeat.minmax.max_value,
                                        max_str, sizeof(max_str));
            ir_sb_appendf(ctx->builder, "minmax(%s, %s)", min_str, max_str);
        } else {
            const char* track = css_format_grid_track(&grid->column_repeat.track);
            ir_sb_append(ctx->builder, track);
        }

        ir_sb_append_line(ctx->builder, ");");
        ctx->has_output = true;
        has_output = true;
    } else if (grid->column_count > 0) {
        ir_sb_indent(ctx->builder, 1);
        ir_sb_append(ctx->builder, "grid-template-columns:");

        for (uint8_t i = 0; i < grid->column_count; i++) {
            const char* track = css_format_grid_track(&grid->columns[i]);
            ir_sb_appendf(ctx->builder, " %s", track);
        }

        ir_sb_append_line(ctx->builder, ";");
        ctx->has_output = true;
        has_output = true;
    }

    // Grid template rows
    if (grid->use_row_repeat && grid->row_repeat.mode != IR_GRID_REPEAT_NONE) {
        ir_sb_indent(ctx->builder, 1);
        ir_sb_append(ctx->builder, "grid-template-rows: repeat(");

        switch (grid->row_repeat.mode) {
            case IR_GRID_REPEAT_AUTO_FIT:
                ir_sb_append(ctx->builder, "auto-fit");
                break;
            case IR_GRID_REPEAT_AUTO_FILL:
                ir_sb_append(ctx->builder, "auto-fill");
                break;
            case IR_GRID_REPEAT_COUNT:
                ir_sb_appendf(ctx->builder, "%d", grid->row_repeat.count);
                break;
            default:
                break;
        }

        ir_sb_append(ctx->builder, ", ");

        if (grid->row_repeat.has_minmax) {
            char min_str[32], max_str[32];
            css_format_grid_track_minmax(grid->row_repeat.minmax.min_type,
                                        grid->row_repeat.minmax.min_value,
                                        min_str, sizeof(min_str));
            css_format_grid_track_minmax(grid->row_repeat.minmax.max_type,
                                        grid->row_repeat.minmax.max_value,
                                        max_str, sizeof(max_str));
            ir_sb_appendf(ctx->builder, "minmax(%s, %s)", min_str, max_str);
        } else {
            const char* track = css_format_grid_track(&grid->row_repeat.track);
            ir_sb_append(ctx->builder, track);
        }

        ir_sb_append_line(ctx->builder, ");");
        ctx->has_output = true;
        has_output = true;
    } else if (grid->row_count > 0) {
        ir_sb_indent(ctx->builder, 1);
        ir_sb_append(ctx->builder, "grid-template-rows:");

        for (uint8_t i = 0; i < grid->row_count; i++) {
            const char* track = css_format_grid_track(&grid->rows[i]);
            ir_sb_appendf(ctx->builder, " %s", track);
        }

        ir_sb_append_line(ctx->builder, ";");
        ctx->has_output = true;
        has_output = true;
    }

    // Gap
    if (grid->row_gap > 0 || grid->column_gap > 0) {
        if (grid->row_gap == grid->column_gap) {
            css_gen_write_propertyf(ctx, "gap", "%s", css_format_px(grid->row_gap));
        } else {
            if (grid->row_gap > 0) {
                css_gen_write_propertyf(ctx, "row-gap", "%s", css_format_px(grid->row_gap));
            }
            if (grid->column_gap > 0) {
                css_gen_write_propertyf(ctx, "column-gap", "%s", css_format_px(grid->column_gap));
            }
        }
        has_output = true;
    }

    // Justify items
    if (grid->justify_items != IR_ALIGNMENT_START) {
        const char* ji = css_format_alignment(grid->justify_items);
        css_gen_write_property(ctx, "justify-items", ji);
        has_output = true;
    }

    // Align items
    if (grid->align_items != IR_ALIGNMENT_START) {
        const char* ai = css_format_alignment(grid->align_items);
        css_gen_write_property(ctx, "align-items", ai);
        has_output = true;
    }

    return has_output;
}

bool css_gen_generate_grid_item(CSSGenContext* ctx, IRGridItem* item) {
    if (!ctx || !item) return false;

    bool has_output = false;

    if (item->row_start >= 0) {
        if (item->row_end >= 0) {
            css_gen_write_propertyf(ctx, "grid-row", "%d / %d",
                                   item->row_start + 1, item->row_end + 1);
        } else {
            css_gen_write_propertyf(ctx, "grid-row", "%d", item->row_start + 1);
        }
        has_output = true;
    }

    if (item->column_start >= 0) {
        if (item->column_end >= 0) {
            css_gen_write_propertyf(ctx, "grid-column", "%d / %d",
                                   item->column_start + 1, item->column_end + 1);
        } else {
            css_gen_write_propertyf(ctx, "grid-column", "%d", item->column_start + 1);
        }
        has_output = true;
    }

    if (item->justify_self != IR_ALIGNMENT_START) {
        const char* js = css_format_alignment(item->justify_self);
        css_gen_write_property(ctx, "justify-self", js);
        has_output = true;
    }

    if (item->align_self != IR_ALIGNMENT_START) {
        const char* as = css_format_alignment(item->align_self);
        css_gen_write_property(ctx, "align-self", as);
        has_output = true;
    }

    return has_output;
}

bool css_gen_generate_overflow(CSSGenContext* ctx, const char* property, IROverflowMode overflow,
                               bool skip_default) {
    if (!ctx || !property) return false;

    if (skip_default && overflow == IR_OVERFLOW_VISIBLE) return false;

    const char* value = css_format_overflow(overflow);
    return css_gen_write_property(ctx, property, value);
}

bool css_gen_generate_layout_mode(CSSGenContext* ctx, IRLayoutMode mode) {
    if (!ctx) return false;

    const char* value = css_format_layout_mode(mode);
    return css_gen_write_property(ctx, "display", value);
}

bool css_gen_generate_animations(CSSGenContext* ctx, IRAnimation** animations, uint32_t count) {
    if (!ctx || !animations || count == 0) return false;

    ir_sb_indent(ctx->builder, 1);
    ir_sb_append(ctx->builder, "animation:");

    for (uint32_t i = 0; i < count; i++) {
        IRAnimation* anim = animations[i];
        if (!anim || !anim->name) continue;

        if (i > 0) ir_sb_append(ctx->builder, ",");

        const char* easing = css_format_easing(anim->default_easing);
        ir_sb_appendf(ctx->builder, " %s %.2fs %s %.2fs",
                     anim->name, anim->duration, easing, anim->delay);

        if (anim->iteration_count < 0) {
            ir_sb_append(ctx->builder, " infinite");
        } else if (anim->iteration_count != 1) {
            ir_sb_appendf(ctx->builder, " %d", anim->iteration_count);
        }

        if (anim->alternate) {
            ir_sb_append(ctx->builder, " alternate");
        }
    }

    ir_sb_append_line(ctx->builder, ";");
    ctx->has_output = true;
    return true;
}

bool css_gen_generate_transitions(CSSGenContext* ctx, IRTransition** transitions, uint32_t count) {
    if (!ctx || !transitions || count == 0) return false;

    ir_sb_indent(ctx->builder, 1);
    ir_sb_append(ctx->builder, "transition:");

    for (uint32_t i = 0; i < count; i++) {
        IRTransition* trans = transitions[i];
        if (!trans) continue;

        if (i > 0) ir_sb_append(ctx->builder, ",");

        const char* prop = css_format_animation_property(trans->property);
        const char* easing = css_format_easing(trans->easing);
        if (prop) {
            ir_sb_appendf(ctx->builder, " %s %.2fs %s %.2fs",
                         prop, trans->duration, easing, trans->delay);
        }
    }

    ir_sb_append_line(ctx->builder, ";");
    ctx->has_output = true;
    return true;
}

bool css_gen_generate_text_effect(CSSGenContext* ctx, IRTextEffect* effect) {
    if (!ctx || !effect) return false;

    bool has_output = false;

    // Text overflow (ellipsis)
    if (effect->overflow == IR_TEXT_OVERFLOW_ELLIPSIS) {
        css_gen_write_property(ctx, "white-space", "nowrap");
        css_gen_write_property(ctx, "overflow", "hidden");
        css_gen_write_property(ctx, "text-overflow", "ellipsis");
        has_output = true;
    } else if (effect->overflow == IR_TEXT_OVERFLOW_CLIP) {
        css_gen_write_property(ctx, "overflow", "hidden");
        has_output = true;
    }

    // Text shadow
    if (effect->shadow.enabled) {
        css_gen_generate_text_shadow(ctx, &effect->shadow);
        has_output = true;
    }

    // Text fade effect (using CSS mask)
    if (effect->fade_type != IR_TEXT_FADE_NONE && effect->fade_length > 0) {
        float fade_len = effect->fade_length;

        switch (effect->fade_type) {
            case IR_TEXT_FADE_HORIZONTAL:
                css_gen_write_propertyf(ctx, "-webkit-mask-image",
                                       "linear-gradient(to right, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent)",
                                       fade_len, fade_len);
                css_gen_write_propertyf(ctx, "mask-image",
                                       "linear-gradient(to right, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent)",
                                       fade_len, fade_len);
                break;
            case IR_TEXT_FADE_VERTICAL:
                css_gen_write_propertyf(ctx, "-webkit-mask-image",
                                       "linear-gradient(to bottom, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent)",
                                       fade_len, fade_len);
                css_gen_write_propertyf(ctx, "mask-image",
                                       "linear-gradient(to bottom, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent)",
                                       fade_len, fade_len);
                break;
            case IR_TEXT_FADE_RADIAL:
                css_gen_write_propertyf(ctx, "-webkit-mask-image",
                                       "radial-gradient(circle, black calc(100%% - %.1fpx), transparent)",
                                       fade_len);
                css_gen_write_propertyf(ctx, "mask-image",
                                       "radial-gradient(circle, black calc(100%% - %.1fpx), transparent)",
                                       fade_len);
                break;
            default:
                break;
        }
        has_output = true;
    }

    return has_output;
}

bool css_gen_generate_media_query(CSSGenContext* ctx, IRQueryCondition* conditions, uint8_t count,
                                   CSSMediaQueryContentFn content_callback, void* user_data) {
    if (!ctx || !conditions || count == 0 || !content_callback) return false;

    ir_sb_append(ctx->builder, "@media (");

    for (uint8_t i = 0; i < count; i++) {
        if (i > 0) ir_sb_append(ctx->builder, " and ");

        IRQueryCondition* cond = &conditions[i];
        switch (cond->type) {
            case IR_QUERY_MIN_WIDTH:
                ir_sb_appendf(ctx->builder, "min-width: %.0fpx", cond->value);
                break;
            case IR_QUERY_MAX_WIDTH:
                ir_sb_appendf(ctx->builder, "max-width: %.0fpx", cond->value);
                break;
            case IR_QUERY_MIN_HEIGHT:
                ir_sb_appendf(ctx->builder, "min-height: %.0fpx", cond->value);
                break;
            case IR_QUERY_MAX_HEIGHT:
                ir_sb_appendf(ctx->builder, "max-height: %.0fpx", cond->value);
                break;
        }
    }

    ir_sb_append_line(ctx->builder, ") {");

    content_callback(ctx, user_data);

    ir_sb_append_line(ctx->builder, "}");
    ctx->has_output = true;
    return true;
}

bool css_gen_generate_container_query(CSSGenContext* ctx, IRQueryCondition* conditions, uint8_t count,
                                       const char* container_name,
                                       CSSMediaQueryContentFn content_callback, void* user_data) {
    if (!ctx || !conditions || count == 0 || !content_callback) return false;

    ir_sb_append(ctx->builder, "@container");
    if (container_name) {
        ir_sb_appendf(ctx->builder, " %s", container_name);
    }
    ir_sb_append(ctx->builder, " (");

    for (uint8_t i = 0; i < count; i++) {
        if (i > 0) ir_sb_append(ctx->builder, " and ");

        IRQueryCondition* cond = &conditions[i];
        switch (cond->type) {
            case IR_QUERY_MIN_WIDTH:
                ir_sb_appendf(ctx->builder, "min-width: %.0fpx", cond->value);
                break;
            case IR_QUERY_MAX_WIDTH:
                ir_sb_appendf(ctx->builder, "max-width: %.0fpx", cond->value);
                break;
            case IR_QUERY_MIN_HEIGHT:
                ir_sb_appendf(ctx->builder, "min-height: %.0fpx", cond->value);
                break;
            case IR_QUERY_MAX_HEIGHT:
                ir_sb_appendf(ctx->builder, "max-height: %.0fpx", cond->value);
                break;
        }
    }

    ir_sb_append_line(ctx->builder, ") {");

    content_callback(ctx, user_data);

    ir_sb_append_line(ctx->builder, "}");
    ctx->has_output = true;
    return true;
}

bool css_gen_generate_pseudo_class(CSSGenContext* ctx, const char* selector, const char* pseudo_class,
                                    CSSPseudoStyleFn style_callback, void* user_data) {
    if (!ctx || !selector || !pseudo_class || !style_callback) return false;

    ir_sb_appendf(ctx->builder, "%s:%s {\n", selector, pseudo_class);

    style_callback(ctx, user_data);

    ir_sb_append_line(ctx->builder, "}");
    ctx->has_output = true;
    return true;
}

bool css_gen_generate_keyframes(CSSGenContext* ctx, IRAnimation* animation) {
    if (!ctx || !animation || !animation->name || animation->keyframe_count == 0) return false;

    ir_sb_appendf(ctx->builder, "@keyframes %s {\n", animation->name);

    for (uint8_t i = 0; i < animation->keyframe_count; i++) {
        IRKeyframe* kf = &animation->keyframes[i];

        ir_sb_indent(ctx->builder, 1);
        ir_sb_appendf(ctx->builder, "%.1f%% {\n", kf->offset * 100.0f);

        bool generated_transform = false;

        for (uint8_t j = 0; j < kf->property_count; j++) {
            if (!kf->properties[j].is_set) continue;

            IRAnimationProperty prop = kf->properties[j].property;

            if (!generated_transform &&
                (prop == IR_ANIM_PROP_TRANSLATE_X || prop == IR_ANIM_PROP_TRANSLATE_Y ||
                 prop == IR_ANIM_PROP_SCALE_X || prop == IR_ANIM_PROP_SCALE_Y ||
                 prop == IR_ANIM_PROP_ROTATE)) {

                // Collect transform values
                float tx = 0, ty = 0, sx = 1, sy = 1, rot = 0;
                for (uint8_t k = 0; k < kf->property_count; k++) {
                    if (!kf->properties[k].is_set) continue;
                    switch (kf->properties[k].property) {
                        case IR_ANIM_PROP_TRANSLATE_X:
                            tx = kf->properties[k].value;
                            break;
                        case IR_ANIM_PROP_TRANSLATE_Y:
                            ty = kf->properties[k].value;
                            break;
                        case IR_ANIM_PROP_SCALE_X:
                            sx = kf->properties[k].value;
                            break;
                        case IR_ANIM_PROP_SCALE_Y:
                            sy = kf->properties[k].value;
                            break;
                        case IR_ANIM_PROP_ROTATE:
                            rot = kf->properties[k].value;
                            break;
                        default:
                            break;
                    }
                }

                ir_sb_indent(ctx->builder, 2);
                ir_sb_appendf(ctx->builder, "transform: translate(%.1fpx, %.1fpx) scale(%.2f, %.2f)",
                             tx, ty, sx, sy);
                if (rot != 0) {
                    ir_sb_appendf(ctx->builder, " rotate(%.1fdeg)", rot);
                }
                ir_sb_append_line(ctx->builder, ";");
                generated_transform = true;
            } else if (prop == IR_ANIM_PROP_OPACITY) {
                ir_sb_indent(ctx->builder, 2);
                ir_sb_appendf(ctx->builder, "opacity: %.2f;\n", kf->properties[j].value);
            } else if (prop == IR_ANIM_PROP_BACKGROUND_COLOR) {
                IRColor color = kf->properties[j].color_value;
                if (color.type == IR_COLOR_SOLID) {
                    ir_sb_indent(ctx->builder, 2);
                    ir_sb_appendf(ctx->builder, "background-color: rgba(%d, %d, %d, %.2f);\n",
                                 color.data.r, color.data.g, color.data.b,
                                 color.data.a / 255.0f);
                }
            }
        }

        ir_sb_indent(ctx->builder, 1);
        ir_sb_append_line(ctx->builder, "}");
    }

    ir_sb_append_line(ctx->builder, "}");
    ctx->has_output = true;
    return true;
}

bool css_gen_generate_root_variables(CSSGenContext* ctx, CSSVariableEntry* variables, size_t count) {
    if (!ctx || !variables || count == 0) return false;

    ir_sb_append_line(ctx->builder, ":root {");

    for (size_t i = 0; i < count; i++) {
        if (variables[i].name && variables[i].value) {
            ir_sb_indent(ctx->builder, 1);
            ir_sb_appendf(ctx->builder, "--%s: %s;\n", variables[i].name, variables[i].value);
        }
    }

    ir_sb_append_line(ctx->builder, "}");
    ctx->has_output = true;
    return true;
}
