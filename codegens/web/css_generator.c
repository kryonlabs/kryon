#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "../../ir/ir_core.h"
#include "../../ir/ir_style_vars.h"
#include "css_generator.h"

// CSS Generator Context
typedef struct CSSGenerator {
    char* output_buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    bool pretty_print;
    uint32_t component_counter;
} CSSGenerator;

// Forward declarations for animation functions
static const char* easing_to_css(IREasingType easing);
static const char* animation_property_to_css(IRAnimationProperty prop);
static void generate_keyframes(CSSGenerator* generator, IRComponent* root);

CSSGenerator* css_generator_create() {
    CSSGenerator* generator = malloc(sizeof(CSSGenerator));
    if (!generator) return NULL;

    generator->output_buffer = NULL;
    generator->buffer_size = 0;
    generator->buffer_capacity = 0;
    generator->pretty_print = true;
    generator->component_counter = 0;

    return generator;
}

void css_generator_destroy(CSSGenerator* generator) {
    if (!generator) return;

    if (generator->output_buffer) {
        free(generator->output_buffer);
    }
    free(generator);
}

void css_generator_set_pretty_print(CSSGenerator* generator, bool pretty) {
    generator->pretty_print = pretty;
}

bool css_generator_write_string(CSSGenerator* generator, const char* string) {
    if (!generator || !string) return false;

    size_t string_len = strlen(string);
    size_t needed_space = generator->buffer_size + string_len + 1;

    if (needed_space > generator->buffer_capacity) {
        size_t new_capacity = generator->buffer_capacity * 2;
        if (new_capacity < needed_space) {
            new_capacity = needed_space + 1024;
        }

        char* new_buffer = realloc(generator->output_buffer, new_capacity);
        if (!new_buffer) return false;

        generator->output_buffer = new_buffer;
        generator->buffer_capacity = new_capacity;

        // Ensure buffer is null-terminated after reallocation
        if (generator->buffer_size == 0) {
            generator->output_buffer[0] = '\0';
        }
    }

    // Use memcpy instead of strcat for better performance and safety
    memcpy(generator->output_buffer + generator->buffer_size, string, string_len + 1);
    generator->buffer_size += string_len;
    generator->output_buffer[generator->buffer_size] = '\0';

    return true;
}

bool css_generator_write_format(CSSGenerator* generator, const char* format, ...) {
    if (!generator || !format) return false;

    va_list args;
    va_start(args, format);

    // Calculate required size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return false;
    }

    size_t needed_space = generator->buffer_size + needed + 1;
    if (needed_space > generator->buffer_capacity) {
        size_t new_capacity = generator->buffer_capacity * 2;
        if (new_capacity < needed_space) {
            new_capacity = needed_space + 1024;
        }

        char* new_buffer = realloc(generator->output_buffer, new_capacity);
        if (!new_buffer) {
            va_end(args);
            return false;
        }

        generator->output_buffer = new_buffer;
        generator->buffer_capacity = new_capacity;
    }

    vsnprintf(generator->output_buffer + generator->buffer_size, needed + 1, format, args);
    generator->buffer_size += needed;
    generator->output_buffer[generator->buffer_size] = '\0';

    va_end(args);
    return true;
}

static const char* get_color_string(IRColor color) {
    static char color_buffer[512];  // Increased size for gradients

    switch (color.type) {
        case IR_COLOR_SOLID: {
            snprintf(color_buffer, sizeof(color_buffer), "rgba(%u, %u, %u, %.2f)",
                     color.data.r, color.data.g, color.data.b, color.data.a / 255.0f);
            return color_buffer;
        }

        case IR_COLOR_TRANSPARENT:
            return "transparent";

        case IR_COLOR_VAR_REF: {
            snprintf(color_buffer, sizeof(color_buffer), "var(--color-%u)", color.data.var_id);
            return color_buffer;
        }

        case IR_COLOR_GRADIENT: {
            if (!color.data.gradient || color.data.gradient->stop_count < 2) {
                return "transparent";
            }

            IRGradient* grad = color.data.gradient;

            if (grad->type == IR_GRADIENT_LINEAR) {
                // linear-gradient(angle, color1 pos1%, color2 pos2%, ...)
                int offset = snprintf(color_buffer, sizeof(color_buffer),
                                    "linear-gradient(%.0fdeg", grad->angle);

                for (int i = 0; i < grad->stop_count && offset < (int)sizeof(color_buffer) - 50; i++) {
                    IRGradientStop* stop = &grad->stops[i];
                    offset += snprintf(color_buffer + offset, sizeof(color_buffer) - offset,
                                     ", rgba(%u, %u, %u, %.2f) %.1f%%",
                                     stop->r, stop->g, stop->b, stop->a / 255.0f,
                                     stop->position * 100.0f);
                }

                if (offset < (int)sizeof(color_buffer) - 1) {
                    color_buffer[offset++] = ')';
                    color_buffer[offset] = '\0';
                }

                return color_buffer;

            } else if (grad->type == IR_GRADIENT_RADIAL) {
                // radial-gradient(circle at x% y%, color1 pos1%, color2 pos2%, ...)
                int offset = snprintf(color_buffer, sizeof(color_buffer),
                                    "radial-gradient(circle at %.1f%% %.1f%%",
                                    grad->center_x * 100.0f, grad->center_y * 100.0f);

                for (int i = 0; i < grad->stop_count && offset < (int)sizeof(color_buffer) - 50; i++) {
                    IRGradientStop* stop = &grad->stops[i];
                    offset += snprintf(color_buffer + offset, sizeof(color_buffer) - offset,
                                     ", rgba(%u, %u, %u, %.2f) %.1f%%",
                                     stop->r, stop->g, stop->b, stop->a / 255.0f,
                                     stop->position * 100.0f);
                }

                if (offset < (int)sizeof(color_buffer) - 1) {
                    color_buffer[offset++] = ')';
                    color_buffer[offset] = '\0';
                }

                return color_buffer;

            } else if (grad->type == IR_GRADIENT_CONIC) {
                // conic-gradient(from 0deg at x% y%, color1 pos1%, color2 pos2%, ...)
                int offset = snprintf(color_buffer, sizeof(color_buffer),
                                    "conic-gradient(from 0deg at %.1f%% %.1f%%",
                                    grad->center_x * 100.0f, grad->center_y * 100.0f);

                for (int i = 0; i < grad->stop_count && offset < (int)sizeof(color_buffer) - 50; i++) {
                    IRGradientStop* stop = &grad->stops[i];
                    offset += snprintf(color_buffer + offset, sizeof(color_buffer) - offset,
                                     ", rgba(%u, %u, %u, %.2f) %.1f%%",
                                     stop->r, stop->g, stop->b, stop->a / 255.0f,
                                     stop->position * 100.0f);
                }

                if (offset < (int)sizeof(color_buffer) - 1) {
                    color_buffer[offset++] = ')';
                    color_buffer[offset] = '\0';
                }

                return color_buffer;
            }

            return "transparent";
        }

        default:
            return "transparent";
    }
}

static const char* get_dimension_string(IRDimension dimension) {
    static char dim_buffer[32];

    switch (dimension.type) {
        case IR_DIMENSION_PX:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2fpx", dimension.value);
            break;
        case IR_DIMENSION_PERCENT:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2f%%", dimension.value);
            break;
        case IR_DIMENSION_VW:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2fvw", dimension.value);
            break;
        case IR_DIMENSION_VH:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2fvh", dimension.value);
            break;
        case IR_DIMENSION_VMIN:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2fvmin", dimension.value);
            break;
        case IR_DIMENSION_VMAX:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2fvmax", dimension.value);
            break;
        case IR_DIMENSION_REM:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2frem", dimension.value);
            break;
        case IR_DIMENSION_EM:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2fem", dimension.value);
            break;
        case IR_DIMENSION_AUTO:
            strcpy(dim_buffer, "auto");
            break;
        case IR_DIMENSION_FLEX:
            snprintf(dim_buffer, sizeof(dim_buffer), "%.2fr", dimension.value);
            break;
        default:
            strcpy(dim_buffer, "auto");
            break;
    }
    return dim_buffer;
}

static const char* get_alignment_string(IRAlignment alignment) {
    switch (alignment) {
        case IR_ALIGNMENT_START: return "flex-start";
        case IR_ALIGNMENT_CENTER: return "center";
        case IR_ALIGNMENT_END: return "flex-end";
        case IR_ALIGNMENT_STRETCH: return "stretch";
        default: return "flex-start";
    }
}

static const char* get_grid_track_string(IRGridTrack* track) {
    static char track_buffer[32];

    switch (track->type) {
        case IR_GRID_TRACK_PX:
            snprintf(track_buffer, sizeof(track_buffer), "%.1fpx", track->value);
            return track_buffer;
        case IR_GRID_TRACK_PERCENT:
            snprintf(track_buffer, sizeof(track_buffer), "%.1f%%", track->value);
            return track_buffer;
        case IR_GRID_TRACK_FR:
            snprintf(track_buffer, sizeof(track_buffer), "%.1ffr", track->value);
            return track_buffer;
        case IR_GRID_TRACK_AUTO:
            return "auto";
        case IR_GRID_TRACK_MIN_CONTENT:
            return "min-content";
        case IR_GRID_TRACK_MAX_CONTENT:
            return "max-content";
        default:
            return "1fr";
    }
}

static const char* get_overflow_string(IROverflowMode overflow) {
    switch (overflow) {
        case IR_OVERFLOW_HIDDEN: return "hidden";
        case IR_OVERFLOW_SCROLL: return "scroll";
        case IR_OVERFLOW_AUTO: return "auto";
        case IR_OVERFLOW_VISIBLE:
        default: return "visible";
    }
}

static void generate_style_rules(CSSGenerator* generator, IRComponent* component) {
    if (!component || !component->style) return;

    css_generator_write_format(generator, "#kryon-%u {\n", component->id);

    // Basic dimensions
    if (component->style->width.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  width: %s;\n",
                                  get_dimension_string(component->style->width));
    }

    if (component->style->height.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  height: %s;\n",
                                  get_dimension_string(component->style->height));
    }

    // Background (color or gradient)
    if (component->style->background.type != IR_COLOR_TRANSPARENT) {
        const char* bg = get_color_string(component->style->background);
        // Use 'background' for gradients, 'background-color' for solid colors
        if (component->style->background.type == IR_COLOR_GRADIENT) {
            css_generator_write_format(generator, "  background: %s;\n", bg);
        } else {
            css_generator_write_format(generator, "  background-color: %s;\n", bg);
        }
    }

    // Border
    if (component->style->border.width > 0) {
        css_generator_write_format(generator, "  border: %.2fpx solid %s;\n",
                                  component->style->border.width,
                                  get_color_string(component->style->border.color));
        if (component->style->border.radius > 0) {
            css_generator_write_format(generator, "  border-radius: %upx;\n",
                                      component->style->border.radius);
        }
    }

    // Margin
    if (component->style->margin.top != 0 || component->style->margin.right != 0 ||
        component->style->margin.bottom != 0 || component->style->margin.left != 0) {
        css_generator_write_format(generator, "  margin: %.2fpx %.2fpx %.2fpx %.2fpx;\n",
                                  component->style->margin.top,
                                  component->style->margin.right,
                                  component->style->margin.bottom,
                                  component->style->margin.left);
    }

    // Padding
    if (component->style->padding.top != 0 || component->style->padding.right != 0 ||
        component->style->padding.bottom != 0 || component->style->padding.left != 0) {
        css_generator_write_format(generator, "  padding: %.2fpx %.2fpx %.2fpx %.2fpx;\n",
                                  component->style->padding.top,
                                  component->style->padding.right,
                                  component->style->padding.bottom,
                                  component->style->padding.left);
    }

    // Typography
    if (component->style->font.family || component->style->font.size > 0 ||
        component->style->font.color.type != IR_COLOR_TRANSPARENT) {

        if (component->style->font.size > 0) {
            css_generator_write_format(generator, "  font-size: %.2fpx;\n",
                                      component->style->font.size);
        }

        if (component->style->font.family) {
            css_generator_write_format(generator, "  font-family: %s;\n",
                                      component->style->font.family);
        }

        if (component->style->font.color.type != IR_COLOR_TRANSPARENT) {
            css_generator_write_format(generator, "  color: %s;\n",
                                      get_color_string(component->style->font.color));
        }

        // Font weight - numeric value takes precedence over bold flag
        if (component->style->font.weight > 0) {
            css_generator_write_format(generator, "  font-weight: %u;\n",
                                      component->style->font.weight);
        } else if (component->style->font.bold) {
            css_generator_write_string(generator, "  font-weight: bold;\n");
        }

        if (component->style->font.italic) {
            css_generator_write_string(generator, "  font-style: italic;\n");
        }

        // Extended typography (Phase 3)
        if (component->style->font.line_height > 0) {
            css_generator_write_format(generator, "  line-height: %.2f;\n",
                                      component->style->font.line_height);
        }

        if (component->style->font.letter_spacing != 0) {
            css_generator_write_format(generator, "  letter-spacing: %.2fpx;\n",
                                      component->style->font.letter_spacing);
        }

        if (component->style->font.word_spacing != 0) {
            css_generator_write_format(generator, "  word-spacing: %.2fpx;\n",
                                      component->style->font.word_spacing);
        }

        // Text alignment
        switch (component->style->font.align) {
            case IR_TEXT_ALIGN_LEFT:
                css_generator_write_string(generator, "  text-align: left;\n");
                break;
            case IR_TEXT_ALIGN_RIGHT:
                css_generator_write_string(generator, "  text-align: right;\n");
                break;
            case IR_TEXT_ALIGN_CENTER:
                css_generator_write_string(generator, "  text-align: center;\n");
                break;
            case IR_TEXT_ALIGN_JUSTIFY:
                css_generator_write_string(generator, "  text-align: justify;\n");
                break;
        }

        // Text decoration
        if (component->style->font.decoration != IR_TEXT_DECORATION_NONE) {
            css_generator_write_string(generator, "  text-decoration:");
            bool needs_space = false;

            if (component->style->font.decoration & IR_TEXT_DECORATION_UNDERLINE) {
                css_generator_write_string(generator, " underline");
                needs_space = true;
            }
            if (component->style->font.decoration & IR_TEXT_DECORATION_OVERLINE) {
                css_generator_write_string(generator, needs_space ? " " : " ");
                css_generator_write_string(generator, "overline");
                needs_space = true;
            }
            if (component->style->font.decoration & IR_TEXT_DECORATION_LINE_THROUGH) {
                css_generator_write_string(generator, needs_space ? " " : " ");
                css_generator_write_string(generator, "line-through");
            }
            css_generator_write_string(generator, ";\n");
        }
    }

    // Text shadow
    if (component->style->text_effect.shadow.enabled) {
        IRTextShadow* shadow = &component->style->text_effect.shadow;
        const char* shadow_color = get_color_string(shadow->color);

        // CSS text-shadow: offset-x offset-y blur-radius color
        css_generator_write_format(generator, "  text-shadow: %.1fpx %.1fpx %.1fpx %s;\n",
                                  shadow->offset_x, shadow->offset_y,
                                  shadow->blur_radius, shadow_color);
    }

    // Text overflow (ellipsis)
    if (component->style->text_effect.overflow == IR_TEXT_OVERFLOW_ELLIPSIS) {
        css_generator_write_string(generator, "  white-space: nowrap;\n");
        css_generator_write_string(generator, "  overflow: hidden;\n");
        css_generator_write_string(generator, "  text-overflow: ellipsis;\n");
    } else if (component->style->text_effect.overflow == IR_TEXT_OVERFLOW_CLIP) {
        css_generator_write_string(generator, "  overflow: hidden;\n");
    }

    // Text fade effect (using CSS mask)
    if (component->style->text_effect.fade_type != IR_TEXT_FADE_NONE &&
        component->style->text_effect.fade_length > 0) {

        float fade_len = component->style->text_effect.fade_length;

        switch (component->style->text_effect.fade_type) {
            case IR_TEXT_FADE_HORIZONTAL:
                // Fade left and right edges
                css_generator_write_format(generator,
                    "  -webkit-mask-image: linear-gradient(to right, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent);\n",
                    fade_len, fade_len);
                css_generator_write_format(generator,
                    "  mask-image: linear-gradient(to right, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent);\n",
                    fade_len, fade_len);
                break;

            case IR_TEXT_FADE_VERTICAL:
                // Fade top and bottom edges
                css_generator_write_format(generator,
                    "  -webkit-mask-image: linear-gradient(to bottom, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent);\n",
                    fade_len, fade_len);
                css_generator_write_format(generator,
                    "  mask-image: linear-gradient(to bottom, transparent, black %.1fpx, black calc(100%% - %.1fpx), transparent);\n",
                    fade_len, fade_len);
                break;

            case IR_TEXT_FADE_RADIAL:
                // Fade from center outward
                css_generator_write_format(generator,
                    "  -webkit-mask-image: radial-gradient(circle, black calc(100%% - %.1fpx), transparent);\n",
                    fade_len);
                css_generator_write_format(generator,
                    "  mask-image: radial-gradient(circle, black calc(100%% - %.1fpx), transparent);\n",
                    fade_len);
                break;

            default:
                break;
        }
    }

    // Box shadow
    if (component->style->box_shadow.enabled) {
        IRBoxShadow* shadow = &component->style->box_shadow;
        const char* shadow_color = get_color_string(shadow->color);

        // CSS box-shadow: [inset] offset-x offset-y blur-radius spread-radius color
        if (shadow->inset) {
            css_generator_write_string(generator, "  box-shadow: inset ");
        } else {
            css_generator_write_string(generator, "  box-shadow: ");
        }

        css_generator_write_format(generator, "%.1fpx %.1fpx %.1fpx %.1fpx %s;\n",
                                  shadow->offset_x, shadow->offset_y,
                                  shadow->blur_radius, shadow->spread_radius,
                                  shadow_color);
    }

    // CSS Filters
    if (component->style->filter_count > 0) {
        css_generator_write_string(generator, "  filter:");

        for (uint8_t i = 0; i < component->style->filter_count; i++) {
            IRFilter* filter = &component->style->filters[i];

            switch (filter->type) {
                case IR_FILTER_BLUR:
                    css_generator_write_format(generator, " blur(%.1fpx)", filter->value);
                    break;
                case IR_FILTER_BRIGHTNESS:
                    css_generator_write_format(generator, " brightness(%.2f)", filter->value);
                    break;
                case IR_FILTER_CONTRAST:
                    css_generator_write_format(generator, " contrast(%.2f)", filter->value);
                    break;
                case IR_FILTER_GRAYSCALE:
                    css_generator_write_format(generator, " grayscale(%.2f)", filter->value);
                    break;
                case IR_FILTER_HUE_ROTATE:
                    css_generator_write_format(generator, " hue-rotate(%.0fdeg)", filter->value);
                    break;
                case IR_FILTER_INVERT:
                    css_generator_write_format(generator, " invert(%.2f)", filter->value);
                    break;
                case IR_FILTER_OPACITY:
                    css_generator_write_format(generator, " opacity(%.2f)", filter->value);
                    break;
                case IR_FILTER_SATURATE:
                    css_generator_write_format(generator, " saturate(%.2f)", filter->value);
                    break;
                case IR_FILTER_SEPIA:
                    css_generator_write_format(generator, " sepia(%.2f)", filter->value);
                    break;
            }
        }

        css_generator_write_string(generator, ";\n");
    }

    // Transform
    if (component->style->transform.translate_x != 0 ||
        component->style->transform.translate_y != 0 ||
        component->style->transform.scale_x != 1 ||
        component->style->transform.scale_y != 1 ||
        component->style->transform.rotate != 0) {

        css_generator_write_string(generator, "  transform: ");

        bool first_transform = true;

        if (component->style->transform.translate_x != 0 ||
            component->style->transform.translate_y != 0) {
            if (!first_transform) css_generator_write_string(generator, " ");
            css_generator_write_format(generator, "translate(%.2fpx, %.2fpx)",
                                      component->style->transform.translate_x,
                                      component->style->transform.translate_y);
            first_transform = false;
        }

        if (component->style->transform.scale_x != 1 ||
            component->style->transform.scale_y != 1) {
            if (!first_transform) css_generator_write_string(generator, " ");
            css_generator_write_format(generator, "scale(%.2f, %.2f)",
                                      component->style->transform.scale_x,
                                      component->style->transform.scale_y);
            first_transform = false;
        }

        if (component->style->transform.rotate != 0) {
            if (!first_transform) css_generator_write_string(generator, " ");
            css_generator_write_format(generator, "rotate(%.2fdeg)",
                                      component->style->transform.rotate);
        }

        css_generator_write_string(generator, ";\n");
    }

    // Opacity and visibility
    if (component->style->opacity < 1.0f) {
        css_generator_write_format(generator, "  opacity: %.2f;\n",
                                  component->style->opacity);
    }

    if (!component->style->visible) {
        css_generator_write_string(generator, "  display: none;\n");
    }

    // Positioning mode
    switch (component->style->position_mode) {
        case IR_POSITION_ABSOLUTE:
            css_generator_write_string(generator, "  position: absolute;\n");
            css_generator_write_format(generator, "  left: %.1fpx;\n", component->style->absolute_x);
            css_generator_write_format(generator, "  top: %.1fpx;\n", component->style->absolute_y);
            break;
        case IR_POSITION_FIXED:
            css_generator_write_string(generator, "  position: fixed;\n");
            css_generator_write_format(generator, "  left: %.1fpx;\n", component->style->absolute_x);
            css_generator_write_format(generator, "  top: %.1fpx;\n", component->style->absolute_y);
            break;
        case IR_POSITION_RELATIVE:
        default:
            // Relative is default, no need to output
            break;
    }

    // Z-index
    if (component->style->z_index > 0) {
        css_generator_write_format(generator, "  z-index: %u;\n",
                                  component->style->z_index);
    }

    // Grid item placement (Phase 5)
    IRGridItem* grid_item = &component->style->grid_item;
    if (grid_item->row_start >= 0 || grid_item->column_start >= 0) {
        // grid-row: start / end
        if (grid_item->row_start >= 0) {
            if (grid_item->row_end >= 0) {
                css_generator_write_format(generator, "  grid-row: %d / %d;\n",
                                          grid_item->row_start + 1,  // CSS grid is 1-indexed
                                          grid_item->row_end + 1);
            } else {
                css_generator_write_format(generator, "  grid-row: %d;\n",
                                          grid_item->row_start + 1);
            }
        }

        // grid-column: start / end
        if (grid_item->column_start >= 0) {
            if (grid_item->column_end >= 0) {
                css_generator_write_format(generator, "  grid-column: %d / %d;\n",
                                          grid_item->column_start + 1,  // CSS grid is 1-indexed
                                          grid_item->column_end + 1);
            } else {
                css_generator_write_format(generator, "  grid-column: %d;\n",
                                          grid_item->column_start + 1);
            }
        }
    }

    // Grid item alignment
    if (grid_item->justify_self != IR_ALIGNMENT_START) {
        css_generator_write_format(generator, "  justify-self: %s;\n",
                                  get_alignment_string(grid_item->justify_self));
    }
    if (grid_item->align_self != IR_ALIGNMENT_START) {
        css_generator_write_format(generator, "  align-self: %s;\n",
                                  get_alignment_string(grid_item->align_self));
    }

    // Overflow
    if (component->style->overflow_x != IR_OVERFLOW_VISIBLE || component->style->overflow_y != IR_OVERFLOW_VISIBLE) {
        const char* overflow_x_str = get_overflow_string(component->style->overflow_x);
        const char* overflow_y_str = get_overflow_string(component->style->overflow_y);

        if (component->style->overflow_x == component->style->overflow_y) {
            // Same for both axes
            css_generator_write_format(generator, "  overflow: %s;\n", overflow_x_str);
        } else {
            // Different for each axis
            css_generator_write_format(generator, "  overflow-x: %s;\n", overflow_x_str);
            css_generator_write_format(generator, "  overflow-y: %s;\n", overflow_y_str);
        }
    }

    // Animations
    if (component->style->animations && component->style->animation_count > 0) {
        css_generator_write_string(generator, "  animation:");

        for (uint32_t i = 0; i < component->style->animation_count; i++) {
            IRAnimation* anim = component->style->animations[i];
            if (!anim || !anim->name) continue;

            if (i > 0) css_generator_write_string(generator, ",");

            // animation-name duration timing-function delay iteration-count direction
            css_generator_write_format(generator, " %s %.2fs %s %.2fs",
                                      anim->name,
                                      anim->duration,
                                      easing_to_css(anim->default_easing),
                                      anim->delay);

            // Iteration count
            if (anim->iteration_count < 0) {
                css_generator_write_string(generator, " infinite");
            } else if (anim->iteration_count != 1) {
                css_generator_write_format(generator, " %d", anim->iteration_count);
            }

            // Alternate direction
            if (anim->alternate) {
                css_generator_write_string(generator, " alternate");
            }
        }

        css_generator_write_string(generator, ";\n");
    }

    // Transitions
    if (component->style->transitions && component->style->transition_count > 0) {
        css_generator_write_string(generator, "  transition:");

        for (uint32_t i = 0; i < component->style->transition_count; i++) {
            IRTransition* trans = component->style->transitions[i];
            if (!trans) continue;

            if (i > 0) css_generator_write_string(generator, ",");

            const char* prop = animation_property_to_css(trans->property);
            if (prop) {
                // property duration timing-function delay
                css_generator_write_format(generator, " %s %.2fs %s %.2fs",
                                          prop,
                                          trans->duration,
                                          easing_to_css(trans->easing),
                                          trans->delay);
            }
        }

        css_generator_write_string(generator, ";\n");
    }

    css_generator_write_string(generator, "}\n\n");
}

static void generate_layout_rules(CSSGenerator* generator, IRComponent* component) {
    if (!component || !component->layout) return;

    css_generator_write_format(generator, "#kryon-%u {\n", component->id);

    IRLayout* layout = component->layout;

    // Layout mode determines display type
    switch (layout->mode) {
        case IR_LAYOUT_MODE_GRID: {
            // CSS Grid layout
            IRGrid* grid = &layout->grid;
            css_generator_write_string(generator, "  display: grid;\n");

            // Grid template columns
            if (grid->column_count > 0) {
                css_generator_write_string(generator, "  grid-template-columns:");
                for (uint8_t i = 0; i < grid->column_count; i++) {
                    IRGridTrack* track = &grid->columns[i];
                    css_generator_write_string(generator, " ");
                    css_generator_write_string(generator, get_grid_track_string(track));
                }
                css_generator_write_string(generator, ";\n");
            }

            // Grid template rows
            if (grid->row_count > 0) {
                css_generator_write_string(generator, "  grid-template-rows:");
                for (uint8_t i = 0; i < grid->row_count; i++) {
                    IRGridTrack* track = &grid->rows[i];
                    css_generator_write_string(generator, " ");
                    css_generator_write_string(generator, get_grid_track_string(track));
                }
                css_generator_write_string(generator, ";\n");
            }

            // Grid gaps
            if (grid->row_gap > 0 || grid->column_gap > 0) {
                css_generator_write_format(generator, "  gap: %.1fpx %.1fpx;\n",
                                          grid->row_gap, grid->column_gap);
            }

            // Grid alignment
            css_generator_write_format(generator, "  justify-items: %s;\n",
                                      get_alignment_string(grid->justify_items));
            css_generator_write_format(generator, "  align-items: %s;\n",
                                      get_alignment_string(grid->align_items));
            css_generator_write_format(generator, "  justify-content: %s;\n",
                                      get_alignment_string(grid->justify_content));
            css_generator_write_format(generator, "  align-content: %s;\n",
                                      get_alignment_string(grid->align_content));

            // Grid auto flow
            if (!grid->auto_flow_row) {
                css_generator_write_string(generator, "  grid-auto-flow: column");
                if (grid->auto_flow_dense) {
                    css_generator_write_string(generator, " dense");
                }
                css_generator_write_string(generator, ";\n");
            } else if (grid->auto_flow_dense) {
                css_generator_write_string(generator, "  grid-auto-flow: row dense;\n");
            }
            break;
        }

        case IR_LAYOUT_MODE_FLEX:
        default: {
            // Flexbox properties
            IRFlexbox* flex = &layout->flex;

            // Set display flex for containers with children or explicit flex settings
            if (component->child_count > 0 ||
                flex->gap > 0 ||
                flex->justify_content != IR_ALIGNMENT_START ||
                flex->cross_axis != IR_ALIGNMENT_START) {

                css_generator_write_string(generator, "  display: flex;\n");

                // Flex direction
                if (component->type == IR_COMPONENT_ROW) {
                    css_generator_write_string(generator, "  flex-direction: row;\n");
                } else if (component->type == IR_COMPONENT_COLUMN) {
                    css_generator_write_string(generator, "  flex-direction: column;\n");
                }

                // Flex wrap
                if (flex->wrap) {
                    css_generator_write_string(generator, "  flex-wrap: wrap;\n");
                }

                // Gap
                if (flex->gap > 0) {
                    css_generator_write_format(generator, "  gap: %upx;\n", flex->gap);
                }

                // Main axis alignment (justify-content)
                css_generator_write_format(generator, "  justify-content: %s;\n",
                                          get_alignment_string(flex->justify_content));

                // Cross axis alignment (align-items)
                css_generator_write_format(generator, "  align-items: %s;\n",
                                          get_alignment_string(flex->cross_axis));

                // Main axis alignment (align-self for single item)
                css_generator_write_format(generator, "  align-self: %s;\n",
                                          get_alignment_string(flex->justify_content));
            }

            // Positioning
            if (component->type == IR_COMPONENT_CENTER) {
                css_generator_write_string(generator, "  justify-content: center;\n");
                css_generator_write_string(generator, "  align-items: center;\n");
            }
            break;
        }
    }

    // Min/max dimensions
    if (layout->min_width.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  min-width: %s;\n",
                                  get_dimension_string(layout->min_width));
    }

    if (layout->min_height.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  min-height: %s;\n",
                                  get_dimension_string(layout->min_height));
    }

    if (layout->max_width.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  max-width: %s;\n",
                                  get_dimension_string(layout->max_width));
    }

    if (layout->max_height.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  max-height: %s;\n",
                                  get_dimension_string(layout->max_height));
    }

    css_generator_write_string(generator, "}\n\n");
}

static void generate_responsive_rules(CSSGenerator* generator, IRComponent* component) {
    if (!generator || !component || !component->style) return;
    if (component->style->breakpoint_count == 0) return;

    // Generate media queries for each breakpoint
    for (uint8_t i = 0; i < component->style->breakpoint_count; i++) {
        IRBreakpoint* bp = &component->style->breakpoints[i];
        if (bp->condition_count == 0) continue;

        // Start media query or container query
        bool is_container_query = (component->parent && component->parent->style &&
                                   component->parent->style->container_type != IR_CONTAINER_TYPE_NORMAL);

        if (is_container_query) {
            // Container query
            css_generator_write_string(generator, "@container");
            if (component->parent->style->container_name) {
                css_generator_write_format(generator, " %s", component->parent->style->container_name);
            }
            css_generator_write_string(generator, " (");
        } else {
            // Media query
            css_generator_write_string(generator, "@media (");
        }

        // Write conditions
        for (uint8_t j = 0; j < bp->condition_count; j++) {
            if (j > 0) css_generator_write_string(generator, " and ");

            IRQueryCondition* cond = &bp->conditions[j];
            switch (cond->type) {
                case IR_QUERY_MIN_WIDTH:
                    css_generator_write_format(generator, "min-width: %.0fpx", cond->value);
                    break;
                case IR_QUERY_MAX_WIDTH:
                    css_generator_write_format(generator, "max-width: %.0fpx", cond->value);
                    break;
                case IR_QUERY_MIN_HEIGHT:
                    css_generator_write_format(generator, "min-height: %.0fpx", cond->value);
                    break;
                case IR_QUERY_MAX_HEIGHT:
                    css_generator_write_format(generator, "max-height: %.0fpx", cond->value);
                    break;
            }
        }

        css_generator_write_string(generator, ") {\n");
        css_generator_write_format(generator, "  #kryon-%u {\n", component->id);

        // Apply breakpoint overrides
        if (bp->width.type != IR_DIMENSION_AUTO) {
            css_generator_write_format(generator, "    width: %s;\n",
                                      get_dimension_string(bp->width));
        }

        if (bp->height.type != IR_DIMENSION_AUTO) {
            css_generator_write_format(generator, "    height: %s;\n",
                                      get_dimension_string(bp->height));
        }

        if (!bp->visible) {
            css_generator_write_string(generator, "    display: none;\n");
        }

        if (bp->opacity >= 0 && bp->opacity <= 1.0f) {
            css_generator_write_format(generator, "    opacity: %.2f;\n", bp->opacity);
        }

        if (bp->has_layout_mode) {
            switch (bp->layout_mode) {
                case IR_LAYOUT_MODE_GRID:
                    css_generator_write_string(generator, "    display: grid;\n");
                    break;
                case IR_LAYOUT_MODE_FLEX:
                    css_generator_write_string(generator, "    display: flex;\n");
                    break;
                case IR_LAYOUT_MODE_BLOCK:
                    css_generator_write_string(generator, "    display: block;\n");
                    break;
            }
        }

        css_generator_write_string(generator, "  }\n");
        css_generator_write_string(generator, "}\n\n");
    }
}

static void generate_container_context(CSSGenerator* generator, IRComponent* component) {
    if (!generator || !component || !component->style) return;
    if (component->style->container_type == IR_CONTAINER_TYPE_NORMAL) return;

    // Generate container-type declaration
    css_generator_write_format(generator, "#kryon-%u {\n", component->id);

    switch (component->style->container_type) {
        case IR_CONTAINER_TYPE_SIZE:
            css_generator_write_string(generator, "  container-type: size;\n");
            break;
        case IR_CONTAINER_TYPE_INLINE_SIZE:
            css_generator_write_string(generator, "  container-type: inline-size;\n");
            break;
        default:
            break;
    }

    if (component->style->container_name) {
        css_generator_write_format(generator, "  container-name: %s;\n",
                                  component->style->container_name);
    }

    css_generator_write_string(generator, "}\n\n");
}

static const char* get_pseudo_class_name(IRPseudoState state) {
    switch (state) {
        case IR_PSEUDO_HOVER: return "hover";
        case IR_PSEUDO_ACTIVE: return "active";
        case IR_PSEUDO_FOCUS: return "focus";
        case IR_PSEUDO_DISABLED: return "disabled";
        case IR_PSEUDO_CHECKED: return "checked";
        case IR_PSEUDO_FIRST_CHILD: return "first-child";
        case IR_PSEUDO_LAST_CHILD: return "last-child";
        case IR_PSEUDO_VISITED: return "visited";
        default: return NULL;
    }
}

static void generate_pseudo_class_rules(CSSGenerator* generator, IRComponent* component) {
    if (!generator || !component || !component->style) return;
    if (component->style->pseudo_style_count == 0) return;

    // Generate CSS for each pseudo-class style
    for (uint8_t i = 0; i < component->style->pseudo_style_count; i++) {
        IRPseudoStyle* pseudo = &component->style->pseudo_styles[i];

        // Get the pseudo-class name
        const char* pseudo_name = get_pseudo_class_name(pseudo->state);
        if (!pseudo_name) continue;

        // Start pseudo-class selector
        css_generator_write_format(generator, "#kryon-%u:%s {\n", component->id, pseudo_name);

        // Apply style overrides
        if (pseudo->has_background) {
            if (pseudo->background.type == IR_COLOR_GRADIENT) {
                css_generator_write_format(generator, "  background: %s;\n",
                                          get_color_string(pseudo->background));
            } else {
                css_generator_write_format(generator, "  background-color: %s;\n",
                                          get_color_string(pseudo->background));
            }
        }

        if (pseudo->has_text_color) {
            css_generator_write_format(generator, "  color: %s;\n",
                                      get_color_string(pseudo->text_color));
        }

        if (pseudo->has_border_color) {
            css_generator_write_format(generator, "  border-color: %s;\n",
                                      get_color_string(pseudo->border_color));
        }

        if (pseudo->has_opacity) {
            css_generator_write_format(generator, "  opacity: %.2f;\n", pseudo->opacity);
        }

        if (pseudo->has_transform) {
            bool has_any_transform = (pseudo->transform.translate_x != 0 ||
                                     pseudo->transform.translate_y != 0 ||
                                     pseudo->transform.scale_x != 1 ||
                                     pseudo->transform.scale_y != 1 ||
                                     pseudo->transform.rotate != 0);

            if (has_any_transform) {
                css_generator_write_string(generator, "  transform:");

                if (pseudo->transform.translate_x != 0 || pseudo->transform.translate_y != 0) {
                    css_generator_write_format(generator, " translate(%.2fpx, %.2fpx)",
                                              pseudo->transform.translate_x,
                                              pseudo->transform.translate_y);
                }

                if (pseudo->transform.scale_x != 1 || pseudo->transform.scale_y != 1) {
                    css_generator_write_format(generator, " scale(%.2f, %.2f)",
                                              pseudo->transform.scale_x,
                                              pseudo->transform.scale_y);
                }

                if (pseudo->transform.rotate != 0) {
                    css_generator_write_format(generator, " rotate(%.2fdeg)", pseudo->transform.rotate);
                }

                css_generator_write_string(generator, ";\n");
            }
        }

        css_generator_write_string(generator, "}\n\n");
    }
}

static void generate_component_css(CSSGenerator* generator, IRComponent* component) {
    if (!generator || !component) return;

    // Generate style rules
    generate_style_rules(generator, component);

    // Generate layout rules
    generate_layout_rules(generator, component);

    // Generate container context if applicable
    generate_container_context(generator, component);

    // Generate pseudo-class rules (:hover, :focus, :active, etc.)
    generate_pseudo_class_rules(generator, component);

    // Generate responsive rules (media queries / container queries)
    generate_responsive_rules(generator, component);

    // Recursively generate CSS for children
    for (uint32_t i = 0; i < component->child_count; i++) {
        generate_component_css(generator, component->children[i]);
    }
}

const char* css_generator_generate(CSSGenerator* generator, IRComponent* root) {
    if (!generator || !root) return NULL;

    // Clear buffer
    generator->buffer_size = 0;
    if (generator->output_buffer) {
        generator->output_buffer[0] = '\0';
    }

    // Generate CSS header
    css_generator_write_string(generator, "/* Kryon Generated CSS */\n");
    css_generator_write_string(generator, "/* Auto-generated from IR format */\n\n");

    // Generate CSS custom properties from style variables
    css_generator_write_string(generator, ":root {\n");
    css_generator_write_string(generator, "  /* Kryon Theme Variables */\n");

    // Export all defined style variables as CSS custom properties
    for (int i = 0; i < IR_MAX_STYLE_VARS; i++) {
        if (ir_style_vars[i].defined) {
            // Generate CSS custom property: --color-N: rgba(r, g, b, a);
            css_generator_write_format(generator, "  --color-%d: rgba(%u, %u, %u, %.2f);\n",
                                      i,
                                      ir_style_vars[i].r,
                                      ir_style_vars[i].g,
                                      ir_style_vars[i].b,
                                      ir_style_vars[i].a / 255.0f);
        }
    }

    css_generator_write_string(generator, "}\n\n");

    // Generate base styles
    css_generator_write_string(generator, "body {\n");
    css_generator_write_string(generator, "  margin: 0;\n");
    css_generator_write_string(generator, "  padding: 0;\n");
    css_generator_write_string(generator, "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n");
    css_generator_write_string(generator, "  background-color: #f0f0f0;\n");
    css_generator_write_string(generator, "}\n\n");

    // Generate @keyframes for animations
    generate_keyframes(generator, root);

    // Generate component-specific styles
    generate_component_css(generator, root);

    return generator->output_buffer;
}

// ============================================================================
// Animation and Keyframe Generation
// ============================================================================

static const char* easing_to_css(IREasingType easing) {
    switch (easing) {
        case IR_EASING_LINEAR: return "linear";
        case IR_EASING_EASE_IN: return "ease-in";
        case IR_EASING_EASE_OUT: return "ease-out";
        case IR_EASING_EASE_IN_OUT: return "ease-in-out";
        case IR_EASING_EASE_IN_QUAD: return "cubic-bezier(0.55, 0.085, 0.68, 0.53)";
        case IR_EASING_EASE_OUT_QUAD: return "cubic-bezier(0.25, 0.46, 0.45, 0.94)";
        case IR_EASING_EASE_IN_OUT_QUAD: return "cubic-bezier(0.455, 0.03, 0.515, 0.955)";
        case IR_EASING_EASE_IN_CUBIC: return "cubic-bezier(0.55, 0.055, 0.675, 0.19)";
        case IR_EASING_EASE_OUT_CUBIC: return "cubic-bezier(0.215, 0.61, 0.355, 1)";
        case IR_EASING_EASE_IN_OUT_CUBIC: return "cubic-bezier(0.645, 0.045, 0.355, 1)";
        default: return "ease";
    }
}

static const char* animation_property_to_css(IRAnimationProperty prop) {
    switch (prop) {
        case IR_ANIM_PROP_OPACITY: return "opacity";
        case IR_ANIM_PROP_TRANSLATE_X: return "transform";  // Will need special handling
        case IR_ANIM_PROP_TRANSLATE_Y: return "transform";
        case IR_ANIM_PROP_SCALE_X: return "transform";
        case IR_ANIM_PROP_SCALE_Y: return "transform";
        case IR_ANIM_PROP_ROTATE: return "transform";
        case IR_ANIM_PROP_WIDTH: return "width";
        case IR_ANIM_PROP_HEIGHT: return "height";
        case IR_ANIM_PROP_BACKGROUND_COLOR: return "background-color";
        default: return NULL;
    }
}

static void generate_keyframe_property(CSSGenerator* generator, IRKeyframe* kf, int prop_idx) {
    if (!kf->properties[prop_idx].is_set) return;

    IRAnimationProperty prop = kf->properties[prop_idx].property;

    switch (prop) {
        case IR_ANIM_PROP_OPACITY:
            css_generator_write_format(generator, "    opacity: %.2f;\n",
                                        kf->properties[prop_idx].value);
            break;

        case IR_ANIM_PROP_TRANSLATE_X:
        case IR_ANIM_PROP_TRANSLATE_Y:
        case IR_ANIM_PROP_SCALE_X:
        case IR_ANIM_PROP_SCALE_Y:
        case IR_ANIM_PROP_ROTATE:
            // Transform properties need to be combined
            css_generator_write_string(generator, "    transform:");

            // Check all transform properties in this keyframe
            for (int i = 0; i < kf->property_count; i++) {
                if (!kf->properties[i].is_set) continue;

                switch (kf->properties[i].property) {
                    case IR_ANIM_PROP_TRANSLATE_X:
                    case IR_ANIM_PROP_TRANSLATE_Y: {
                        float tx = 0, ty = 0;
                        for (int j = 0; j < kf->property_count; j++) {
                            if (kf->properties[j].property == IR_ANIM_PROP_TRANSLATE_X)
                                tx = kf->properties[j].value;
                            if (kf->properties[j].property == IR_ANIM_PROP_TRANSLATE_Y)
                                ty = kf->properties[j].value;
                        }
                        css_generator_write_format(generator, " translate(%.1fpx, %.1fpx)", tx, ty);
                        i++;  // Skip Y since we handled both
                        break;
                    }
                    case IR_ANIM_PROP_SCALE_X:
                    case IR_ANIM_PROP_SCALE_Y: {
                        float sx = 1, sy = 1;
                        for (int j = 0; j < kf->property_count; j++) {
                            if (kf->properties[j].property == IR_ANIM_PROP_SCALE_X)
                                sx = kf->properties[j].value;
                            if (kf->properties[j].property == IR_ANIM_PROP_SCALE_Y)
                                sy = kf->properties[j].value;
                        }
                        css_generator_write_format(generator, " scale(%.2f, %.2f)", sx, sy);
                        i++;  // Skip Y since we handled both
                        break;
                    }
                    case IR_ANIM_PROP_ROTATE:
                        css_generator_write_format(generator, " rotate(%.1fdeg)",
                                                    kf->properties[i].value);
                        break;
                    default:
                        break;
                }
            }
            css_generator_write_string(generator, ";\n");
            return;  // Don't process other transform props

        case IR_ANIM_PROP_BACKGROUND_COLOR: {
            IRColor color = kf->properties[prop_idx].color_value;
            if (color.type == IR_COLOR_SOLID) {
                css_generator_write_format(generator, "    background-color: rgba(%d, %d, %d, %.2f);\n",
                                            color.data.r, color.data.g, color.data.b,
                                            color.data.a / 255.0f);
            }
            break;
        }

        default:
            break;
    }
}

static void generate_animation_keyframes(CSSGenerator* generator, IRAnimation* anim) {
    if (!anim || !anim->name || anim->keyframe_count == 0) return;

    // @keyframes animationName {
    css_generator_write_format(generator, "@keyframes %s {\n", anim->name);

    // Generate each keyframe
    for (uint8_t i = 0; i < anim->keyframe_count; i++) {
        IRKeyframe* kf = &anim->keyframes[i];

        // Keyframe selector (percentage)
        css_generator_write_format(generator, "  %.1f%% {\n", kf->offset * 100.0f);

        // Track which properties we've generated
        bool generated_transform = false;
        bool generated_bg_color = false;

        // Generate properties
        for (uint8_t j = 0; j < kf->property_count; j++) {
            if (!kf->properties[j].is_set) continue;

            IRAnimationProperty prop = kf->properties[j].property;

            // Handle transform properties (only once)
            if (!generated_transform && (prop == IR_ANIM_PROP_TRANSLATE_X || prop == IR_ANIM_PROP_TRANSLATE_Y ||
                prop == IR_ANIM_PROP_SCALE_X || prop == IR_ANIM_PROP_SCALE_Y || prop == IR_ANIM_PROP_ROTATE)) {
                generate_keyframe_property(generator, kf, j);
                generated_transform = true;
            }
            // Handle opacity
            else if (prop == IR_ANIM_PROP_OPACITY) {
                css_generator_write_format(generator, "    opacity: %.2f;\n", kf->properties[j].value);
            }
            // Handle background color (only once)
            else if (!generated_bg_color && prop == IR_ANIM_PROP_BACKGROUND_COLOR) {
                IRColor color = kf->properties[j].color_value;
                if (color.type == IR_COLOR_SOLID) {
                    css_generator_write_format(generator, "    background-color: rgba(%d, %d, %d, %.2f);\n",
                                                color.data.r, color.data.g, color.data.b,
                                                color.data.a / 255.0f);
                }
                generated_bg_color = true;
            }
        }

        css_generator_write_string(generator, "  }\n");
    }

    css_generator_write_string(generator, "}\n\n");
}

static void collect_animations_recursive(CSSGenerator* generator, IRComponent* component) {
    if (!component) return;

    // Generate keyframes for this component's animations
    if (component->style && component->style->animations) {
        for (uint32_t i = 0; i < component->style->animation_count; i++) {
            generate_animation_keyframes(generator, component->style->animations[i]);
        }
    }

    // Recursively process children
    for (uint32_t i = 0; i < component->child_count; i++) {
        collect_animations_recursive(generator, component->children[i]);
    }
}

static void generate_keyframes(CSSGenerator* generator, IRComponent* root) {
    if (!generator || !root) return;

    css_generator_write_string(generator, "/* Keyframe Animations */\n");
    collect_animations_recursive(generator, root);
}

bool css_generator_write_to_file(CSSGenerator* generator, IRComponent* root, const char* filename) {
    const char* css = css_generator_generate(generator, root);
    if (!css) return false;

    FILE* file = fopen(filename, "w");
    if (!file) return false;

    bool success = (fprintf(file, "%s", css) >= 0);
    fclose(file);

    return success;
}

size_t css_generator_get_size(CSSGenerator* generator) {
    return generator ? generator->buffer_size : 0;
}

const char* css_generator_get_buffer(CSSGenerator* generator) {
    return generator ? generator->output_buffer : NULL;
}