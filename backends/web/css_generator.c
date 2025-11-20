#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "../../ir/ir_core.h"
#include "css_generator.h"

// CSS Generator Context
typedef struct CSSGenerator {
    char* output_buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    bool pretty_print;
    uint32_t component_counter;
} CSSGenerator;

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
    }

    strcat(generator->output_buffer + generator->buffer_size, string);
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
    static char color_buffer[16];
    snprintf(color_buffer, sizeof(color_buffer), "#%02X%02X%02X%02X",
             color.r, color.g, color.b, color.a);
    return color_buffer;
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

    // Background color
    if (component->style->background.type != IR_COLOR_TRANSPARENT) {
        css_generator_write_format(generator, "  background-color: %s;\n",
                                  get_color_string(component->style->background));
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

        if (component->style->font.bold) {
            css_generator_write_string(generator, "  font-weight: bold;\n");
        }

        if (component->style->font.italic) {
            css_generator_write_string(generator, "  font-style: italic;\n");
        }
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

    // Z-index
    if (component->style->z_index > 0) {
        css_generator_write_format(generator, "  z-index: %u;\n",
                                  component->style->z_index);
    }

    css_generator_write_string(generator, "}\n\n");
}

static void generate_layout_rules(CSSGenerator* generator, IRComponent* component) {
    if (!component || !component->layout) return;

    css_generator_write_format(generator, "#kryon-%u {\n", component->id);

    // Flexbox properties
    IRFlexbox* flex = &component->layout->flex;

    // Set display flex for containers with children or explicit flex settings
    if (component->child_count > 0 ||
        flex->gap > 0 ||
        flex->main_axis != IR_ALIGNMENT_START ||
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
                                  get_alignment_string(flex->main_axis));
    }

    // Positioning
    if (component->type == IR_COMPONENT_CENTER) {
        css_generator_write_string(generator, "  justify-content: center;\n");
        css_generator_write_string(generator, "  align-items: center;\n");
    }

    // Min dimensions
    if (component->layout->min_width.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  min-width: %s;\n",
                                  get_dimension_string(component->layout->min_width));
    }

    if (component->layout->min_height.type != IR_DIMENSION_AUTO) {
        css_generator_write_format(generator, "  min-height: %s;\n",
                                  get_dimension_string(component->layout->min_height));
    }

    css_generator_write_string(generator, "}\n\n");
}

static void generate_component_css(CSSGenerator* generator, IRComponent* component) {
    if (!generator || !component) return;

    // Generate style rules
    generate_style_rules(generator, component);

    // Generate layout rules
    generate_layout_rules(generator, component);

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

    // Generate base styles
    css_generator_write_string(generator, "body {\n");
    css_generator_write_string(generator, "  margin: 0;\n");
    css_generator_write_string(generator, "  padding: 0;\n");
    css_generator_write_string(generator, "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n");
    css_generator_write_string(generator, "  background-color: #f0f0f0;\n");
    css_generator_write_string(generator, "}\n\n");

    // Generate component-specific styles
    generate_component_css(generator, root);

    return generator->output_buffer;
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