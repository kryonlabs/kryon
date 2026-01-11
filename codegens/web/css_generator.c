#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "../../ir/ir_core.h"
#include "../../ir/ir_style_vars.h"
#include "../../ir/ir_stylesheet.h"
#include "../../ir/ir_builder.h"
#include "css_generator.h"
#include "style_analyzer.h"

// Data-driven modules for CSS generation
#include "css_property_tables.h"
#include "css_generator_core.h"
#include "css_class_names.h"

// CSS Generator Context
typedef struct CSSGenerator {
    char* output_buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    bool pretty_print;
    uint32_t component_counter;

    // Track generated class names to avoid duplicates
    char** generated_classes;
    size_t generated_class_count;
    size_t generated_class_capacity;

    // CSS custom properties (variables) for :root block
    CSSVariable* css_variables;
    size_t css_variable_count;
} CSSGenerator;

// Forward declarations for animation functions
static void generate_keyframes(CSSGenerator* generator, IRComponent* root);

// Use formatter functions from css_property_tables module
#define easing_to_css css_format_easing
#define animation_property_to_css css_format_animation_property

CSSGenerator* css_generator_create() {
    CSSGenerator* generator = malloc(sizeof(CSSGenerator));
    if (!generator) return NULL;

    generator->output_buffer = NULL;
    generator->buffer_size = 0;
    generator->buffer_capacity = 0;
    generator->pretty_print = true;
    generator->component_counter = 0;

    generator->generated_classes = NULL;
    generator->generated_class_count = 0;
    generator->generated_class_capacity = 0;

    generator->css_variables = NULL;
    generator->css_variable_count = 0;

    return generator;
}

void css_generator_destroy(CSSGenerator* generator) {
    if (!generator) return;

    if (generator->output_buffer) {
        free(generator->output_buffer);
    }

    // Free generated class names
    for (size_t i = 0; i < generator->generated_class_count; i++) {
        free(generator->generated_classes[i]);
    }
    free(generator->generated_classes);

    // Free CSS variables
    if (generator->css_variables) {
        for (size_t i = 0; i < generator->css_variable_count; i++) {
            free(generator->css_variables[i].name);
            free(generator->css_variables[i].value);
        }
        free(generator->css_variables);
    }

    free(generator);
}

void css_generator_set_pretty_print(CSSGenerator* generator, bool pretty) {
    generator->pretty_print = pretty;
}

void css_generator_set_css_variables(CSSGenerator* generator, CSSVariable* variables, size_t count) {
    if (!generator) return;

    // Free existing variables
    if (generator->css_variables) {
        for (size_t i = 0; i < generator->css_variable_count; i++) {
            free(generator->css_variables[i].name);
            free(generator->css_variables[i].value);
        }
        free(generator->css_variables);
        generator->css_variables = NULL;
        generator->css_variable_count = 0;
    }

    if (!variables || count == 0) return;

    // Copy variables
    generator->css_variables = malloc(sizeof(CSSVariable) * count);
    if (!generator->css_variables) return;

    for (size_t i = 0; i < count; i++) {
        generator->css_variables[i].name = variables[i].name ? strdup(variables[i].name) : NULL;
        generator->css_variables[i].value = variables[i].value ? strdup(variables[i].value) : NULL;
    }
    generator->css_variable_count = count;
}

void css_generator_set_manifest(CSSGenerator* generator, IRReactiveManifest* manifest) {
    if (!generator || !manifest) return;

    // Count CSS variables (variables with "css:" prefix in their name)
    size_t css_var_count = 0;
    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        if (var->name && strncmp(var->name, "css:", 4) == 0) {
            css_var_count++;
        }
    }

    if (css_var_count == 0) return;

    // Allocate and populate CSS variables
    CSSVariable* css_vars = malloc(sizeof(CSSVariable) * css_var_count);
    if (!css_vars) return;

    size_t idx = 0;
    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        if (var->name && strncmp(var->name, "css:", 4) == 0) {
            // Extract variable name (skip "css:" prefix)
            const char* var_name = var->name + 4;

            // Get value from initial_value_json or value.as_string
            const char* var_value = var->initial_value_json;
            if (!var_value && var->type == IR_REACTIVE_TYPE_STRING) {
                var_value = var->value.as_string;
            }

            css_vars[idx].name = strdup(var_name);
            css_vars[idx].value = var_value ? strdup(var_value) : strdup("");
            idx++;
        }
    }

    // Use the existing setter (which will make copies)
    css_generator_set_css_variables(generator, css_vars, css_var_count);

    // Free temporary array
    for (size_t i = 0; i < css_var_count; i++) {
        free(css_vars[i].name);
        free(css_vars[i].value);
    }
    free(css_vars);
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

// Helper to lookup a color in CSS variables
// Returns variable name (e.g., "--bg") or NULL if not found
static const char* lookup_css_variable_for_color(CSSGenerator* generator, IRColor color) {
    if (!generator || !generator->css_variables || color.type != IR_COLOR_SOLID) {
        return NULL;
    }

    // Build the RGBA string to match against
    char rgba_str[64];
    snprintf(rgba_str, sizeof(rgba_str), "rgba(%u, %u, %u, %.2f)",
             color.data.r, color.data.g, color.data.b, color.data.a / 255.0f);

    // Build the hex string to match against
    char hex_str[10];
    snprintf(hex_str, sizeof(hex_str), "#%02x%02x%02x",
             color.data.r, color.data.g, color.data.b);

    for (size_t i = 0; i < generator->css_variable_count; i++) {
        CSSVariable* var = &generator->css_variables[i];
        if (var->value) {
            // Check if value matches as hex or rgba
            if (strcmp(var->value, hex_str) == 0 ||
                strcmp(var->value, rgba_str) == 0) {
                return var->name;
            }
        }
    }
    return NULL;
}

// Thread-local generator for get_color_string_with_vars
static __thread CSSGenerator* tls_css_generator = NULL;

static const char* get_color_string(IRColor color) {
    static char color_buffer[512];  // Increased size for gradients

    // Check for CSS variable name first (for roundtrip preservation)
    // var_name now stores full reference like "var(--primary)"
    if (color.var_name) {
        snprintf(color_buffer, sizeof(color_buffer), "%s", color.var_name);
        return color_buffer;
    }

    // Check if this color matches a CSS variable
    if (tls_css_generator) {
        const char* var_name = lookup_css_variable_for_color(tls_css_generator, color);
        if (var_name) {
            snprintf(color_buffer, sizeof(color_buffer), "var(--%s)", var_name);
            return color_buffer;
        }
    }

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

// Get CSS variable name for a color property type
// Returns "var(--bg-color)", "var(--text-color)", etc.
// CSS variables are always used (no mode flag)
static const char* get_color_var_name(CSSGenerator* generator, const char* var_name) {
    static char var_buffer[64];

    if (!generator || !var_name) {
        return NULL;
    }

    snprintf(var_buffer, sizeof(var_buffer), "var(--%s)", var_name);
    return var_buffer;
}

// Helper to format CSS numeric values without trailing zeros
// Outputs "0" for zero values (no unit), otherwise removes trailing .00 or .0
static void format_css_value(char* buffer, size_t size, float value, const char* unit) {
    if (value == 0.0f) {
        // Zero values don't need a unit
        strcpy(buffer, "0");
        return;
    }

    // Check if value is effectively an integer
    if (floorf(value) == value) {
        snprintf(buffer, size, "%.0f%s", value, unit);
    } else {
        // Format with enough precision, then strip trailing zeros
        snprintf(buffer, size, "%.2f%s", value, unit);
        // Find the end of the numeric part (before unit) and strip trailing zeros
        size_t len = strlen(buffer);
        size_t unit_len = strlen(unit);
        char* p = buffer + len - unit_len - 1;  // Point to last digit before unit
        while (p > buffer && *p == '0') p--;
        if (*p == '.') p--;  // Remove trailing dot too
        // Move unit (and null terminator) right after the last significant digit
        memmove(p + 1, buffer + len - unit_len, unit_len + 1);
    }
}

// Thread-local buffer for inline px value formatting (use in format strings)
static __thread char px_buffer[8][32];
static __thread int px_buffer_idx = 0;

static const char* fmt_px(float value) {
    char* buf = px_buffer[px_buffer_idx++ & 7];
    format_css_value(buf, 32, value, "px");
    return buf;
}

// Format unitless float (for line-height, etc.) without trailing zeros
static const char* fmt_float(float value) {
    char* buf = px_buffer[px_buffer_idx++ & 7];
    format_css_value(buf, 32, value, "");
    return buf;
}

static const char* get_dimension_string(IRDimension dimension) {
    static char dim_buffer[32];

    switch (dimension.type) {
        case IR_DIMENSION_PX:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "px");
            break;
        case IR_DIMENSION_PERCENT:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "%");
            break;
        case IR_DIMENSION_VW:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "vw");
            break;
        case IR_DIMENSION_VH:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "vh");
            break;
        case IR_DIMENSION_VMIN:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "vmin");
            break;
        case IR_DIMENSION_VMAX:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "vmax");
            break;
        case IR_DIMENSION_REM:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "rem");
            break;
        case IR_DIMENSION_EM:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "em");
            break;
        case IR_DIMENSION_AUTO:
            strcpy(dim_buffer, "auto");
            break;
        case IR_DIMENSION_FLEX:
            format_css_value(dim_buffer, sizeof(dim_buffer), dimension.value, "fr");
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
            snprintf(track_buffer, sizeof(track_buffer), "%.0fpx", track->value);
            return track_buffer;
        case IR_GRID_TRACK_PERCENT:
            snprintf(track_buffer, sizeof(track_buffer), "%.0f%%", track->value);
            return track_buffer;
        case IR_GRID_TRACK_FR:
            snprintf(track_buffer, sizeof(track_buffer), "%.0ffr", track->value);
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

// Get track string for minmax min or max value
static const char* get_grid_track_string_minmax(IRGridTrackType type, float value, char* buffer, size_t size) {
    switch (type) {
        case IR_GRID_TRACK_PX:
            snprintf(buffer, size, "%.0fpx", value);
            break;
        case IR_GRID_TRACK_PERCENT:
            snprintf(buffer, size, "%.0f%%", value);
            break;
        case IR_GRID_TRACK_FR:
            snprintf(buffer, size, "%.0ffr", value);
            break;
        case IR_GRID_TRACK_AUTO:
            strncpy(buffer, "auto", size);
            break;
        case IR_GRID_TRACK_MIN_CONTENT:
            strncpy(buffer, "min-content", size);
            break;
        case IR_GRID_TRACK_MAX_CONTENT:
            strncpy(buffer, "max-content", size);
            break;
        default:
            strncpy(buffer, "auto", size);
            break;
    }
    return buffer;
}

// Generate grid-template-columns or grid-template-rows
static void generate_grid_template(CSSGenerator* generator, IRGrid* grid, bool is_columns) {
    IRGridRepeat* repeat = is_columns ? &grid->column_repeat : &grid->row_repeat;
    bool use_repeat = is_columns ? grid->use_column_repeat : grid->use_row_repeat;
    IRGridTrack* tracks = is_columns ? grid->columns : grid->rows;
    uint8_t count = is_columns ? grid->column_count : grid->row_count;

    const char* prop = is_columns ? "grid-template-columns" : "grid-template-rows";

    if (use_repeat && repeat->mode != IR_GRID_REPEAT_NONE) {
        // Output repeat() syntax
        css_generator_write_format(generator, "  %s: repeat(", prop);

        switch (repeat->mode) {
            case IR_GRID_REPEAT_AUTO_FIT:
                css_generator_write_string(generator, "auto-fit");
                break;
            case IR_GRID_REPEAT_AUTO_FILL:
                css_generator_write_string(generator, "auto-fill");
                break;
            case IR_GRID_REPEAT_COUNT:
                css_generator_write_format(generator, "%d", repeat->count);
                break;
            default:
                break;
        }

        css_generator_write_string(generator, ", ");

        if (repeat->has_minmax) {
            char min_str[32], max_str[32];
            get_grid_track_string_minmax(repeat->minmax.min_type, repeat->minmax.min_value, min_str, sizeof(min_str));
            get_grid_track_string_minmax(repeat->minmax.max_type, repeat->minmax.max_value, max_str, sizeof(max_str));
            css_generator_write_format(generator, "minmax(%s, %s)", min_str, max_str);
        } else {
            css_generator_write_string(generator, get_grid_track_string(&repeat->track));
        }

        css_generator_write_string(generator, ");\n");
    } else if (count > 0) {
        // Explicit tracks
        css_generator_write_format(generator, "  %s:", prop);
        for (uint8_t i = 0; i < count; i++) {
            css_generator_write_format(generator, " %s", get_grid_track_string(&tracks[i]));
        }
        css_generator_write_string(generator, ";\n");
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

// ============================================================================
// Class Deduplication Helpers
// ============================================================================

static bool has_generated_class(CSSGenerator* generator, const char* class_name) {
    if (!generator || !class_name) return false;

    for (size_t i = 0; i < generator->generated_class_count; i++) {
        if (strcmp(generator->generated_classes[i], class_name) == 0) {
            return true;
        }
    }
    return false;
}

static void mark_class_generated(CSSGenerator* generator, const char* class_name) {
    if (!generator || !class_name) return;
    if (has_generated_class(generator, class_name)) return;

    // Expand array if needed
    if (generator->generated_class_count >= generator->generated_class_capacity) {
        size_t new_capacity = generator->generated_class_capacity == 0 ? 16 : generator->generated_class_capacity * 2;
        char** new_array = realloc(generator->generated_classes, new_capacity * sizeof(char*));
        if (!new_array) return;

        generator->generated_classes = new_array;
        generator->generated_class_capacity = new_capacity;
    }

    generator->generated_classes[generator->generated_class_count++] = strdup(class_name);
}

// Convert space-separated class names to compound CSS selector
// e.g., "btn btn-primary" -> ".btn.btn-primary"
static char* classes_to_selector(const char* class_names) {
    if (!class_names || class_names[0] == '\0') return NULL;

    // Count spaces to determine number of classes
    size_t len = strlen(class_names);
    size_t space_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (class_names[i] == ' ') space_count++;
    }

    // Allocate buffer: original length + dots for each class + leading dot + null
    size_t selector_len = len + space_count + 2;  // +1 for leading dot, +1 for null
    char* selector = malloc(selector_len);
    if (!selector) return NULL;

    char* out = selector;
    *out++ = '.';  // Leading dot

    for (size_t i = 0; i < len; i++) {
        if (class_names[i] == ' ') {
            *out++ = '.';  // Replace space with dot
        } else {
            *out++ = class_names[i];
        }
    }
    *out = '\0';

    return selector;
}

static void generate_style_rules(CSSGenerator* generator, IRComponent* component) {
    if (!component) return;

    // Get natural class name
    char* class_name = generate_natural_class_name(component);
    if (!class_name) return;

    // For element selectors, check if the TAG was already generated (from stylesheet)
    // For class selectors, check if the CLASS was already generated
    bool is_element_selector = (component->selector_type == IR_SELECTOR_ELEMENT && component->tag);
    const char* dedup_key = is_element_selector ? component->tag : class_name;

    // Skip element selector rules from component tree for common HTML elements
    // These should only come from the stylesheet, not from individual component styles
    // This prevents generating extra h1, h2, h3, p, etc. rules at the end
    if (is_element_selector) {
        const char* tag = component->tag;
        // Skip common text/semantic elements that shouldn't have global element rules from components
        if (strcmp(tag, "h1") == 0 || strcmp(tag, "h2") == 0 || strcmp(tag, "h3") == 0 ||
            strcmp(tag, "h4") == 0 || strcmp(tag, "h5") == 0 || strcmp(tag, "h6") == 0 ||
            strcmp(tag, "p") == 0 || strcmp(tag, "span") == 0 || strcmp(tag, "code") == 0 ||
            strcmp(tag, "li") == 0 || strcmp(tag, "ul") == 0 || strcmp(tag, "ol") == 0 ||
            strcmp(tag, "div") == 0) {
            free(class_name);
            return;
        }
    }

    // Skip if we've already generated CSS for this selector (deduplication)
    if (has_generated_class(generator, dedup_key)) {
        free(class_name);
        return;
    }

    // Check if component has ANY styles or layout to output
    bool has_custom_styles = component->style && ir_style_has_custom_values(component->style);
    bool has_layout = component->layout != NULL;

    // Skip if no CSS to generate
    if (!has_custom_styles && !has_layout) {
        free(class_name);
        return;
    }

    // Mark this selector as generated
    mark_class_generated(generator, dedup_key);

    // Track buffer size before writing selector
    size_t start_size = generator->buffer_size;

    // Generate selector based on selector_type for accurate roundtrip
    if (is_element_selector) {
        // Element selector: header { }, nav { }, etc.
        css_generator_write_format(generator, "%s {\n", component->tag);
    } else {
        // Class selector - handle space-separated classes as compound selector
        // e.g., "btn btn-primary" -> ".btn.btn-primary"
        char* selector = classes_to_selector(class_name);
        if (selector) {
            css_generator_write_format(generator, "%s {\n", selector);
            free(selector);
        } else {
            css_generator_write_format(generator, ".%s {\n", class_name);
        }
    }
    free(class_name);

    // Track size after selector (for property checking)
    size_t after_selector_size = generator->buffer_size;

    // NOTE: width/height are NOT output to CSS classes because they're typically
    // unique per component instance. They're applied as inline styles instead.
    // This prevents the root container's dimensions from affecting all containers.

    // Background (color or gradient) - skip fully transparent
    // First check for background_image (gradient string like "linear-gradient(...)")
    if (component->style->background_image && component->style->background_image[0] != '\0') {
        css_generator_write_format(generator, "  background: %s;\n", component->style->background_image);
    } else if (component->style->background.type == IR_COLOR_SOLID) {
        // Only output if not fully transparent
        if (component->style->background.data.a > 0) {
            const char* bg_var = get_color_var_name(generator, "bg-color");
            const char* bg = bg_var ? bg_var : get_color_string(component->style->background);
            css_generator_write_format(generator, "  background-color: %s;\n", bg);
        }
    } else if (component->style->background.type == IR_COLOR_GRADIENT) {
        const char* bg = get_color_string(component->style->background);
        css_generator_write_format(generator, "  background: %s;\n", bg);
    } else if (component->style->background.type == IR_COLOR_VAR_REF) {
        const char* bg = get_color_string(component->style->background);
        css_generator_write_format(generator, "  background: %s;\n", bg);
    }

    // Background clip (for gradient text effects)
    if (component->style->background_clip == IR_BACKGROUND_CLIP_TEXT) {
        css_generator_write_string(generator, "  -webkit-background-clip: text;\n");
        css_generator_write_string(generator, "  background-clip: text;\n");
        // Also output text-fill-color if transparent (common for gradient text)
        if (component->style->text_fill_color.type == IR_COLOR_TRANSPARENT ||
            (component->style->text_fill_color.type == IR_COLOR_SOLID &&
             component->style->text_fill_color.data.a == 0)) {
            css_generator_write_string(generator, "  -webkit-text-fill-color: transparent;\n");
            css_generator_write_string(generator, "  color: transparent;\n");
        } else if (component->style->text_fill_color.type == IR_COLOR_SOLID) {
            const char* fill_color = get_color_string(component->style->text_fill_color);
            css_generator_write_format(generator, "  -webkit-text-fill-color: %s;\n", fill_color);
        }
    } else if (component->style->background_clip == IR_BACKGROUND_CLIP_CONTENT_BOX) {
        css_generator_write_string(generator, "  background-clip: content-box;\n");
    } else if (component->style->background_clip == IR_BACKGROUND_CLIP_PADDING_BOX) {
        css_generator_write_string(generator, "  background-clip: padding-box;\n");
    }

    // Border
    if (component->style->border.width > 0) {
        // Uniform border
        const char* border_var = get_color_var_name(generator, "border-color");
        const char* border_color = border_var ? border_var : get_color_string(component->style->border.color);
        css_generator_write_format(generator, "  border: %s solid %s;\n",
                                  fmt_px(component->style->border.width),
                                  border_color);
    } else {
        // Per-side borders
        const char* border_var = get_color_var_name(generator, "border-color");
        const char* color = border_var ? border_var : get_color_string(component->style->border.color);
        if (component->style->border.width_top > 0) {
            css_generator_write_format(generator, "  border-top: %s solid %s;\n",
                                      fmt_px(component->style->border.width_top), color);
        }
        if (component->style->border.width_right > 0) {
            css_generator_write_format(generator, "  border-right: %s solid %s;\n",
                                      fmt_px(component->style->border.width_right), color);
        }
        if (component->style->border.width_bottom > 0) {
            css_generator_write_format(generator, "  border-bottom: %s solid %s;\n",
                                      fmt_px(component->style->border.width_bottom), color);
        }
        if (component->style->border.width_left > 0) {
            css_generator_write_format(generator, "  border-left: %s solid %s;\n",
                                      fmt_px(component->style->border.width_left), color);
        }
    }
    // Border radius (separate from border)
    if (component->style->border.radius > 0) {
        css_generator_write_format(generator, "  border-radius: %upx;\n",
                                  component->style->border.radius);
    }

    // Margin (with auto support) - use set_flags to detect explicit values
    uint8_t margin_flags = component->style->margin.set_flags;
    if (margin_flags != 0) {
        float top = component->style->margin.top;
        float right = component->style->margin.right;
        float bottom = component->style->margin.bottom;
        float left = component->style->margin.left;

        if (margin_flags == IR_SPACING_SET_ALL) {
            // All sides set - use shorthand
            bool right_auto = (right == IR_SPACING_AUTO);
            bool left_auto = (left == IR_SPACING_AUTO);

            if (top == bottom && right_auto && left_auto) {
                // Common centering pattern: "0 auto" or "16px auto"
                css_generator_write_format(generator, "  margin: %s auto;\n", fmt_px(top));
            } else if (top == right && right == bottom && bottom == left) {
                // All same value: "0" or "16px"
                css_generator_write_format(generator, "  margin: %s;\n", fmt_px(top));
            } else {
                // General case - output each value
                const char* top_str = (top == IR_SPACING_AUTO) ? "auto" : fmt_px(top);
                const char* right_str = (right == IR_SPACING_AUTO) ? "auto" : fmt_px(right);
                const char* bottom_str = (bottom == IR_SPACING_AUTO) ? "auto" : fmt_px(bottom);
                const char* left_str = (left == IR_SPACING_AUTO) ? "auto" : fmt_px(left);

                css_generator_write_format(generator, "  margin: %s %s %s %s;\n",
                                          top_str, right_str, bottom_str, left_str);
            }
        } else {
            // Individual properties set - output only those that are set
            if (margin_flags & IR_SPACING_SET_TOP) {
                if (top == IR_SPACING_AUTO) css_generator_write_string(generator, "  margin-top: auto;\n");
                else css_generator_write_format(generator, "  margin-top: %s;\n", fmt_px(top));
            }
            if (margin_flags & IR_SPACING_SET_RIGHT) {
                if (right == IR_SPACING_AUTO) css_generator_write_string(generator, "  margin-right: auto;\n");
                else css_generator_write_format(generator, "  margin-right: %s;\n", fmt_px(right));
            }
            if (margin_flags & IR_SPACING_SET_BOTTOM) {
                if (bottom == IR_SPACING_AUTO) css_generator_write_string(generator, "  margin-bottom: auto;\n");
                else css_generator_write_format(generator, "  margin-bottom: %s;\n", fmt_px(bottom));
            }
            if (margin_flags & IR_SPACING_SET_LEFT) {
                if (left == IR_SPACING_AUTO) css_generator_write_string(generator, "  margin-left: auto;\n");
                else css_generator_write_format(generator, "  margin-left: %s;\n", fmt_px(left));
            }
        }
    }

    // Padding - use set_flags to detect explicit values
    uint8_t padding_flags = component->style->padding.set_flags;
    if (padding_flags != 0) {
        float top = component->style->padding.top;
        float right = component->style->padding.right;
        float bottom = component->style->padding.bottom;
        float left = component->style->padding.left;

        if (padding_flags == IR_SPACING_SET_ALL) {
            // All sides set - use shortest shorthand
            if (top == right && right == bottom && bottom == left) {
                // All same: "10px" or "0"
                css_generator_write_format(generator, "  padding: %s;\n", fmt_px(top));
            } else if (top == bottom && right == left) {
                // top/bottom same, left/right same: "10px 20px" or "0 24px"
                css_generator_write_format(generator, "  padding: %s %s;\n", fmt_px(top), fmt_px(right));
            } else if (right == left) {
                // top different, left/right same: "10px 20px 30px"
                css_generator_write_format(generator, "  padding: %s %s %s;\n", fmt_px(top), fmt_px(right), fmt_px(bottom));
            } else {
                // All different: "10px 20px 30px 40px"
                css_generator_write_format(generator, "  padding: %s %s %s %s;\n",
                                          fmt_px(top), fmt_px(right), fmt_px(bottom), fmt_px(left));
            }
        } else {
            // Individual properties set - output only those that are set
            if (padding_flags & IR_SPACING_SET_TOP)
                css_generator_write_format(generator, "  padding-top: %s;\n", fmt_px(top));
            if (padding_flags & IR_SPACING_SET_RIGHT)
                css_generator_write_format(generator, "  padding-right: %s;\n", fmt_px(right));
            if (padding_flags & IR_SPACING_SET_BOTTOM)
                css_generator_write_format(generator, "  padding-bottom: %s;\n", fmt_px(bottom));
            if (padding_flags & IR_SPACING_SET_LEFT)
                css_generator_write_format(generator, "  padding-left: %s;\n", fmt_px(left));
        }
    }

    // Typography
    if (component->style->font.family || component->style->font.size > 0 ||
        component->style->font.color.type != IR_COLOR_TRANSPARENT) {

        if (component->style->font.size > 0) {
            css_generator_write_format(generator, "  font-size: %s;\n",
                                      fmt_px(component->style->font.size));
        }

        if (component->style->font.family) {
            css_generator_write_format(generator, "  font-family: %s;\n",
                                      component->style->font.family);
        }

        // Skip fully transparent text color
        if (component->style->font.color.type == IR_COLOR_SOLID &&
            component->style->font.color.data.a > 0) {
            const char* color_var = get_color_var_name(generator, "text-color");
            const char* color = color_var ? color_var : get_color_string(component->style->font.color);
            css_generator_write_format(generator, "  color: %s;\n", color);
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

        // Extended typography (Phase 3) - skip default line-height (1.5)
        if (component->style->font.line_height > 0 &&
            (component->style->font.line_height < 1.49f || component->style->font.line_height > 1.51f)) {
            css_generator_write_format(generator, "  line-height: %s;\n",
                                      fmt_float(component->style->font.line_height));
        }

        if (component->style->font.letter_spacing != 0) {
            css_generator_write_format(generator, "  letter-spacing: %s;\n",
                                      fmt_px(component->style->font.letter_spacing));
        }

        if (component->style->font.word_spacing != 0) {
            css_generator_write_format(generator, "  word-spacing: %s;\n",
                                      fmt_px(component->style->font.word_spacing));
        }

        // Text alignment - skip default (left)
        switch (component->style->font.align) {
            case IR_TEXT_ALIGN_RIGHT:
                css_generator_write_string(generator, "  text-align: right;\n");
                break;
            case IR_TEXT_ALIGN_CENTER:
                css_generator_write_string(generator, "  text-align: center;\n");
                break;
            case IR_TEXT_ALIGN_JUSTIFY:
                css_generator_write_string(generator, "  text-align: justify;\n");
                break;
            case IR_TEXT_ALIGN_LEFT:
            default:
                // Skip default left alignment
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

    // Add layout properties if present
    if (component->layout) {
        IRLayout* layout = component->layout;

        // Flexbox layout - only output if explicitly set in original CSS
        if (layout->display_explicit && (layout->mode == IR_LAYOUT_MODE_FLEX || layout->mode == IR_LAYOUT_MODE_INLINE_FLEX)) {
            IRFlexbox* flex = &layout->flex;

            if (layout->mode == IR_LAYOUT_MODE_INLINE_FLEX) {
                css_generator_write_string(generator, "  display: inline-flex;\n");
            } else {
                css_generator_write_string(generator, "  display: flex;\n");
            }

            // Flex direction based on component type
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

            // Justify content - skip default (flex-start)
            if (flex->justify_content != IR_ALIGNMENT_START) {
                css_generator_write_format(generator, "  justify-content: %s;\n",
                                          get_alignment_string(flex->justify_content));
            }

            // Align items - skip default (stretch for column, flex-start for row)
            if (flex->cross_axis != IR_ALIGNMENT_START && flex->cross_axis != IR_ALIGNMENT_STRETCH) {
                css_generator_write_format(generator, "  align-items: %s;\n",
                                          get_alignment_string(flex->cross_axis));
            }

            // Flex grow/shrink - only if explicitly set (non-zero)
            if (flex->grow > 0) {
                css_generator_write_format(generator, "  flex-grow: %u;\n", flex->grow);
            }
            // Only output flex-shrink if it's not the CSS default (1)
            // flex-shrink: 0 should be output, flex-shrink: 1 is default (skip)
            if (flex->shrink != 1) {
                css_generator_write_format(generator, "  flex-shrink: %u;\n", flex->shrink);
            }

            // Center component special case
            if (component->type == IR_COMPONENT_CENTER) {
                css_generator_write_string(generator, "  justify-content: center;\n");
                css_generator_write_string(generator, "  align-items: center;\n");
            }
        } else if (layout->display_explicit && (layout->mode == IR_LAYOUT_MODE_GRID || layout->mode == IR_LAYOUT_MODE_INLINE_GRID)) {
            // Grid mode - only if explicitly set
            IRGrid* grid = &layout->grid;

            if (layout->mode == IR_LAYOUT_MODE_INLINE_GRID) {
                css_generator_write_string(generator, "  display: inline-grid;\n");
            } else {
                css_generator_write_string(generator, "  display: grid;\n");
            }

            // Grid template columns
            generate_grid_template(generator, grid, true);

            // Grid template rows (if explicit)
            if (grid->row_count > 0 || grid->use_row_repeat) {
                generate_grid_template(generator, grid, false);
            }

            // Gap
            if (grid->row_gap > 0 || grid->column_gap > 0) {
                if (grid->row_gap == grid->column_gap) {
                    css_generator_write_format(generator, "  gap: %s;\n", fmt_px(grid->row_gap));
                } else {
                    if (grid->row_gap > 0) {
                        css_generator_write_format(generator, "  row-gap: %s;\n", fmt_px(grid->row_gap));
                    }
                    if (grid->column_gap > 0) {
                        css_generator_write_format(generator, "  column-gap: %s;\n", fmt_px(grid->column_gap));
                    }
                }
            }

            // Justify/align items (if non-default)
            if (grid->justify_items != IR_ALIGNMENT_START) {
                css_generator_write_format(generator, "  justify-items: %s;\n",
                                          get_alignment_string(grid->justify_items));
            }
            if (grid->align_items != IR_ALIGNMENT_START) {
                css_generator_write_format(generator, "  align-items: %s;\n",
                                          get_alignment_string(grid->align_items));
            }
        } else if (layout->display_explicit) {
            // Other display modes
            switch (layout->mode) {
                case IR_LAYOUT_MODE_BLOCK:
                    css_generator_write_string(generator, "  display: block;\n");
                    break;
                case IR_LAYOUT_MODE_INLINE:
                    css_generator_write_string(generator, "  display: inline;\n");
                    break;
                case IR_LAYOUT_MODE_INLINE_BLOCK:
                    css_generator_write_string(generator, "  display: inline-block;\n");
                    break;
                case IR_LAYOUT_MODE_NONE:
                    css_generator_write_string(generator, "  display: none;\n");
                    break;
                default:
                    break;
            }
        }

        // Gap can also apply to non-flex layouts if explicitly set
        if (!layout->display_explicit && layout->flex.gap > 0) {
            css_generator_write_format(generator, "  gap: %upx;\n", layout->flex.gap);
        }

        // Min/max dimensions
        if (layout->min_width.type != IR_DIMENSION_AUTO) {
            css_generator_write_format(generator, "  min-width: %s;\n",
                                      get_dimension_string(layout->min_width));
        }
        if (layout->max_width.type != IR_DIMENSION_AUTO) {
            css_generator_write_format(generator, "  max-width: %s;\n",
                                      get_dimension_string(layout->max_width));
        }
        if (layout->min_height.type != IR_DIMENSION_AUTO) {
            css_generator_write_format(generator, "  min-height: %s;\n",
                                      get_dimension_string(layout->min_height));
        }
        if (layout->max_height.type != IR_DIMENSION_AUTO) {
            css_generator_write_format(generator, "  max-height: %s;\n",
                                      get_dimension_string(layout->max_height));
        }
    }

    // Check if we actually wrote any properties
    // If buffer size didn't change after selector (no properties), remove the empty block
    if (generator->buffer_size == after_selector_size) {
        // No properties were written, truncate buffer to remove the entire block
        generator->buffer_size = start_size;
        if (generator->output_buffer) {
            generator->output_buffer[start_size] = '\0';
        }
        return;
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

        // Get natural class name
        char* class_name = generate_natural_class_name(component);
        if (!class_name) continue;

        char* selector = classes_to_selector(class_name);
        css_generator_write_format(generator, "  %s {\n", selector ? selector : class_name);
        free(selector);
        free(class_name);

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
                case IR_LAYOUT_MODE_FLEX:
                    css_generator_write_string(generator, "    display: flex;\n");
                    break;
                case IR_LAYOUT_MODE_INLINE_FLEX:
                    css_generator_write_string(generator, "    display: inline-flex;\n");
                    break;
                case IR_LAYOUT_MODE_GRID:
                    css_generator_write_string(generator, "    display: grid;\n");
                    break;
                case IR_LAYOUT_MODE_INLINE_GRID:
                    css_generator_write_string(generator, "    display: inline-grid;\n");
                    break;
                case IR_LAYOUT_MODE_BLOCK:
                    css_generator_write_string(generator, "    display: block;\n");
                    break;
                case IR_LAYOUT_MODE_INLINE:
                    css_generator_write_string(generator, "    display: inline;\n");
                    break;
                case IR_LAYOUT_MODE_INLINE_BLOCK:
                    css_generator_write_string(generator, "    display: inline-block;\n");
                    break;
                case IR_LAYOUT_MODE_NONE:
                    css_generator_write_string(generator, "    display: none;\n");
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

    // Get natural class name
    char* class_name = generate_natural_class_name(component);
    if (!class_name) return;

    // Check for deduplication (using -container suffix)
    char* container_key = NULL;
    if (asprintf(&container_key, "%s-container", class_name) < 0) {
        free(class_name);
        return;
    }

    if (has_generated_class(generator, container_key)) {
        free(class_name);
        free(container_key);
        return;
    }

    mark_class_generated(generator, container_key);
    free(container_key);

    // Generate container-type declaration with correct selector type
    if (component->selector_type == IR_SELECTOR_ELEMENT && component->tag) {
        css_generator_write_format(generator, "%s {\n", component->tag);
    } else {
        char* selector = classes_to_selector(class_name);
        css_generator_write_format(generator, "%s {\n", selector ? selector : class_name);
        free(selector);
    }
    free(class_name);

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

    // Get natural class name (once for all pseudo-classes)
    char* class_name = generate_natural_class_name(component);
    if (!class_name) return;

    // Generate CSS for each pseudo-class style
    for (uint8_t i = 0; i < component->style->pseudo_style_count; i++) {
        IRPseudoStyle* pseudo = &component->style->pseudo_styles[i];

        // Get the pseudo-class name
        const char* pseudo_name = get_pseudo_class_name(pseudo->state);
        if (!pseudo_name) continue;

        // Check for deduplication (using -pseudo-{state} suffix)
        char* pseudo_key = NULL;
        if (asprintf(&pseudo_key, "%s-pseudo-%s", class_name, pseudo_name) < 0) {
            continue;
        }

        if (has_generated_class(generator, pseudo_key)) {
            free(pseudo_key);
            continue;
        }

        mark_class_generated(generator, pseudo_key);
        free(pseudo_key);

        // Start pseudo-class selector with correct selector type
        if (component->selector_type == IR_SELECTOR_ELEMENT && component->tag) {
            css_generator_write_format(generator, "%s:%s {\n", component->tag, pseudo_name);
        } else {
            char* selector = classes_to_selector(class_name);
            css_generator_write_format(generator, "%s:%s {\n", selector ? selector : class_name, pseudo_name);
            free(selector);
        }

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

    free(class_name);
}

static void generate_component_css(CSSGenerator* generator, IRComponent* component) {
    if (!generator || !component) return;

    // Generate style rules (now includes layout properties)
    generate_style_rules(generator, component);

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

// ============================================================================
// Global Stylesheet Rules Generation
// ============================================================================

/**
 * Helper to get color string for IRColor
 */
static const char* get_stylesheet_color_string(IRColor color) {
    static char buffer[64];
    if (color.type == IR_COLOR_SOLID) {
        if (color.data.a == 255) {
            snprintf(buffer, sizeof(buffer), "rgba(%d, %d, %d, 1.00)",
                     color.data.r, color.data.g, color.data.b);
        } else {
            snprintf(buffer, sizeof(buffer), "rgba(%d, %d, %d, %.2f)",
                     color.data.r, color.data.g, color.data.b, color.data.a / 255.0f);
        }
        return buffer;
    } else if (color.type == IR_COLOR_VAR_REF && color.var_name) {
        // var_name now stores full reference like "var(--primary)"
        snprintf(buffer, sizeof(buffer), "%s", color.var_name);
        return buffer;
    }
    return "transparent";
}

/**
 * Generate CSS rules from global IRStylesheet
 * These are rules with complex selectors like "header .container" or ".btn.primary"
 */
static void generate_stylesheet_rules(CSSGenerator* generator, IRStylesheet* stylesheet) {
    if (!generator || !stylesheet || stylesheet->rule_count == 0) return;

    css_generator_write_string(generator, "/* Global Stylesheet Rules */\n");

    for (uint32_t i = 0; i < stylesheet->rule_count; i++) {
        IRStyleRule* rule = &stylesheet->rules[i];
        if (!rule->selector || !rule->selector->original_selector) continue;

        IRStyleProperties* props = &rule->properties;
        if (props->set_flags == 0) continue;

        // Output selector and opening brace
        css_generator_write_format(generator, "%s {\n", rule->selector->original_selector);

        // Output properties
        if (props->set_flags & IR_PROP_DISPLAY) {
            const char* display_str = "block";
            switch (props->display) {
                case IR_LAYOUT_MODE_FLEX: display_str = "flex"; break;
                case IR_LAYOUT_MODE_INLINE_FLEX: display_str = "inline-flex"; break;
                case IR_LAYOUT_MODE_GRID: display_str = "grid"; break;
                case IR_LAYOUT_MODE_INLINE_GRID: display_str = "inline-grid"; break;
                case IR_LAYOUT_MODE_BLOCK: display_str = "block"; break;
                case IR_LAYOUT_MODE_INLINE: display_str = "inline"; break;
                case IR_LAYOUT_MODE_INLINE_BLOCK: display_str = "inline-block"; break;
                case IR_LAYOUT_MODE_NONE: display_str = "none"; break;
            }
            css_generator_write_format(generator, "  display: %s;\n", display_str);
        }

        if (props->set_flags & IR_PROP_FLEX_DIRECTION) {
            css_generator_write_format(generator, "  flex-direction: %s;\n",
                                        props->flex_direction ? "row" : "column");
        }

        if (props->set_flags & IR_PROP_FLEX_WRAP) {
            css_generator_write_format(generator, "  flex-wrap: %s;\n",
                                        props->flex_wrap ? "wrap" : "nowrap");
        }

        if (props->set_flags & IR_PROP_JUSTIFY_CONTENT) {
            const char* jc_str = "flex-start";
            switch (props->justify_content) {
                case IR_ALIGNMENT_CENTER: jc_str = "center"; break;
                case IR_ALIGNMENT_END: jc_str = "flex-end"; break;
                case IR_ALIGNMENT_SPACE_BETWEEN: jc_str = "space-between"; break;
                case IR_ALIGNMENT_SPACE_AROUND: jc_str = "space-around"; break;
                case IR_ALIGNMENT_SPACE_EVENLY: jc_str = "space-evenly"; break;
                default: break;
            }
            css_generator_write_format(generator, "  justify-content: %s;\n", jc_str);
        }

        if (props->set_flags & IR_PROP_ALIGN_ITEMS) {
            const char* ai_str = "flex-start";
            switch (props->align_items) {
                case IR_ALIGNMENT_CENTER: ai_str = "center"; break;
                case IR_ALIGNMENT_END: ai_str = "flex-end"; break;
                case IR_ALIGNMENT_STRETCH: ai_str = "stretch"; break;
                default: break;
            }
            css_generator_write_format(generator, "  align-items: %s;\n", ai_str);
        }

        if (props->set_flags & IR_PROP_GAP) {
            css_generator_write_format(generator, "  gap: %s;\n", fmt_px(props->gap));
        }

        // Grid template (raw CSS strings for roundtrip)
        if (props->set_flags & IR_PROP_GRID_TEMPLATE_COLUMNS) {
            css_generator_write_format(generator, "  grid-template-columns: %s;\n", props->grid_template_columns);
        }
        if (props->set_flags & IR_PROP_GRID_TEMPLATE_ROWS) {
            css_generator_write_format(generator, "  grid-template-rows: %s;\n", props->grid_template_rows);
        }

        if (props->set_flags & IR_PROP_WIDTH) {
            css_generator_write_format(generator, "  width: %s;\n", get_dimension_string(props->width));
        }

        if (props->set_flags & IR_PROP_HEIGHT) {
            css_generator_write_format(generator, "  height: %s;\n", get_dimension_string(props->height));
        }

        if (props->set_flags & IR_PROP_MAX_WIDTH) {
            css_generator_write_format(generator, "  max-width: %s;\n", get_dimension_string(props->max_width));
        }

        if (props->set_flags & IR_PROP_PADDING) {
            float top = props->padding.top;
            float right = props->padding.right;
            float bottom = props->padding.bottom;
            float left = props->padding.left;

            if (top == right && right == bottom && bottom == left) {
                // All same: "0" or "16px"
                css_generator_write_format(generator, "  padding: %s;\n", fmt_px(top));
            } else if (top == bottom && right == left) {
                // top/bottom same, left/right same: "10px 20px"
                css_generator_write_format(generator, "  padding: %s %s;\n", fmt_px(top), fmt_px(right));
            } else if (right == left) {
                // left/right same: "10px 20px 30px"
                css_generator_write_format(generator, "  padding: %s %s %s;\n", fmt_px(top), fmt_px(right), fmt_px(bottom));
            } else {
                // All different: "10px 20px 30px 40px"
                css_generator_write_format(generator, "  padding: %s %s %s %s;\n",
                                            fmt_px(top), fmt_px(right), fmt_px(bottom), fmt_px(left));
            }
        }

        if (props->set_flags & IR_PROP_MARGIN) {
            float top = props->margin.top;
            float right = props->margin.right;
            float bottom = props->margin.bottom;
            float left = props->margin.left;

            // Handle IR_SPACING_AUTO for centering (margin: 0 auto)
            bool right_auto = (right == IR_SPACING_AUTO);
            bool left_auto = (left == IR_SPACING_AUTO);

            if (top == bottom && right_auto && left_auto) {
                css_generator_write_format(generator, "  margin: %s auto;\n", fmt_px(top));
            } else if (top == right && right == bottom && bottom == left && !right_auto) {
                // All same value: "0" or "16px"
                css_generator_write_format(generator, "  margin: %s;\n", fmt_px(top));
            } else if (top == bottom && right == left && !right_auto && !left_auto) {
                // top/bottom same, left/right same: "10px 20px"
                css_generator_write_format(generator, "  margin: %s %s;\n", fmt_px(top), fmt_px(right));
            } else if (right == left && !right_auto && !left_auto) {
                // left/right same: "10px 20px 30px"
                css_generator_write_format(generator, "  margin: %s %s %s;\n", fmt_px(top), fmt_px(right), fmt_px(bottom));
            } else {
                // General case - output each value, handling auto
                const char* top_str = (top == IR_SPACING_AUTO) ? "auto" : fmt_px(top);
                const char* right_str = (right == IR_SPACING_AUTO) ? "auto" : fmt_px(right);
                const char* bottom_str = (bottom == IR_SPACING_AUTO) ? "auto" : fmt_px(bottom);
                const char* left_str = (left == IR_SPACING_AUTO) ? "auto" : fmt_px(left);

                css_generator_write_format(generator, "  margin: %s %s %s %s;\n",
                                          top_str, right_str, bottom_str, left_str);
            }
        }

        if (props->set_flags & IR_PROP_BACKGROUND) {
            // Skip fully transparent colors
            if (props->background.type != IR_COLOR_SOLID || props->background.data.a > 0) {
                const char* color_str = get_stylesheet_color_string(props->background);
                css_generator_write_format(generator, "  background-color: %s;\n", color_str);
            }
        }

        if (props->set_flags & IR_PROP_COLOR) {
            // Skip fully transparent colors
            if (props->color.type != IR_COLOR_SOLID || props->color.data.a > 0) {
                const char* color_str = get_stylesheet_color_string(props->color);
                css_generator_write_format(generator, "  color: %s;\n", color_str);
            }
        }

        if (props->set_flags & IR_PROP_FONT_SIZE) {
            css_generator_write_format(generator, "  font-size: %s;\n",
                                       get_dimension_string(props->font_size));
        }

        if (props->set_flags & IR_PROP_FONT_WEIGHT) {
            if (props->font_bold) {
                css_generator_write_string(generator, "  font-weight: bold;\n");
            } else {
                css_generator_write_format(generator, "  font-weight: %u;\n", props->font_weight);
            }
        }

        if (props->set_flags & IR_PROP_FONT_FAMILY && props->font_family) {
            css_generator_write_format(generator, "  font-family: %s;\n", props->font_family);
        }

        if (props->set_flags & IR_PROP_TEXT_ALIGN) {
            const char* ta_str = "left";
            switch (props->text_align) {
                case IR_TEXT_ALIGN_CENTER: ta_str = "center"; break;
                case IR_TEXT_ALIGN_RIGHT: ta_str = "right"; break;
                case IR_TEXT_ALIGN_JUSTIFY: ta_str = "justify"; break;
                default: break;
            }
            css_generator_write_format(generator, "  text-align: %s;\n", ta_str);
        }

        if (props->set_flags & IR_PROP_BORDER) {
            // Full border with color
            const char* color_str = get_stylesheet_color_string(props->border_color);
            css_generator_write_format(generator, "  border: %s solid %s;\n", fmt_px(props->border_width), color_str);
        } else {
            // Individual border sides
            const char* color_str = get_stylesheet_color_string(props->border_color);
            if (props->set_flags & IR_PROP_BORDER_TOP) {
                css_generator_write_format(generator, "  border-top: %s solid %s;\n",
                                          fmt_px(props->border_width_top), color_str);
            }
            if (props->set_flags & IR_PROP_BORDER_RIGHT) {
                css_generator_write_format(generator, "  border-right: %s solid %s;\n",
                                          fmt_px(props->border_width_right), color_str);
            }
            if (props->set_flags & IR_PROP_BORDER_BOTTOM) {
                css_generator_write_format(generator, "  border-bottom: %s solid %s;\n",
                                          fmt_px(props->border_width_bottom), color_str);
            }
            if (props->set_flags & IR_PROP_BORDER_LEFT) {
                css_generator_write_format(generator, "  border-left: %s solid %s;\n",
                                          fmt_px(props->border_width_left), color_str);
            }
        }

        if (props->set_flags & IR_PROP_BORDER_RADIUS) {
            css_generator_write_format(generator, "  border-radius: %upx;\n", props->border_radius);
        }

        // Border color only (for hover states, etc.)
        if (props->set_flags & IR_PROP_BORDER_COLOR) {
            const char* color_str = get_stylesheet_color_string(props->border_color);
            css_generator_write_format(generator, "  border-color: %s;\n", color_str);
        }

        if (props->set_flags & IR_PROP_LINE_HEIGHT) {
            css_generator_write_format(generator, "  line-height: %s;\n", fmt_float(props->line_height));
        }

        // Background image (gradient string)
        if (props->set_flags & IR_PROP_BACKGROUND_IMAGE) {
            if (props->background_image) {
                css_generator_write_format(generator, "  background: %s;\n", props->background_image);
            }
        }

        // Background clip (for gradient text effects)
        // Track if we're using background-clip: text for automatic text-fill-color
        bool has_background_clip_text = false;
        if (props->set_flags & IR_PROP_BACKGROUND_CLIP) {
            if (props->background_clip == IR_BACKGROUND_CLIP_TEXT) {
                css_generator_write_string(generator, "  -webkit-background-clip: text;\n");
                css_generator_write_string(generator, "  background-clip: text;\n");
                has_background_clip_text = true;
            } else if (props->background_clip == IR_BACKGROUND_CLIP_CONTENT_BOX) {
                css_generator_write_string(generator, "  background-clip: content-box;\n");
            } else if (props->background_clip == IR_BACKGROUND_CLIP_PADDING_BOX) {
                css_generator_write_string(generator, "  background-clip: padding-box;\n");
            }
        }

        // Text fill color (for gradient text effects)
        // If background-clip: text is set, automatically add transparent text-fill-color
        // unless explicitly specified otherwise
        if (props->set_flags & IR_PROP_TEXT_FILL_COLOR) {
            if (props->text_fill_color.type == IR_COLOR_TRANSPARENT ||
                (props->text_fill_color.type == IR_COLOR_SOLID && props->text_fill_color.data.a == 0)) {
                css_generator_write_string(generator, "  -webkit-text-fill-color: transparent;\n");
                css_generator_write_string(generator, "  color: transparent;\n");
            } else if (props->text_fill_color.type == IR_COLOR_SOLID) {
                const char* color_str = get_stylesheet_color_string(props->text_fill_color);
                css_generator_write_format(generator, "  -webkit-text-fill-color: %s;\n", color_str);
            }
        } else if (has_background_clip_text) {
            // Auto-add transparent text-fill-color for gradient text effect
            css_generator_write_string(generator, "  -webkit-text-fill-color: transparent;\n");
            css_generator_write_string(generator, "  color: transparent;\n");
        }

        // Transition - convert IRTransition to CSS
        if (props->set_flags & IR_PROP_TRANSITION) {
            if (props->transitions && props->transition_count > 0) {
                IRTransition* t = &props->transitions[0];

                // Get easing function string
                const char* easing_str = "linear";
                switch (t->easing) {
                    case IR_EASING_EASE_IN: easing_str = "ease-in"; break;
                    case IR_EASING_EASE_OUT: easing_str = "ease-out"; break;
                    case IR_EASING_EASE_IN_OUT: easing_str = "ease"; break;
                    case IR_EASING_EASE_IN_QUAD: easing_str = "ease-in"; break;
                    case IR_EASING_EASE_OUT_QUAD: easing_str = "ease-out"; break;
                    case IR_EASING_EASE_IN_OUT_QUAD: easing_str = "ease-in-out"; break;
                    default: break;
                }

                // "all" for IR_ANIM_PROP_CUSTOM (generic transition)
                css_generator_write_format(generator, "  transition: all %.1fs %s;\n",
                                           t->duration, easing_str);
            }
        }

        // Transform - convert IRTransform to CSS
        if (props->set_flags & IR_PROP_TRANSFORM) {
            IRTransform* tr = &props->transform;
            bool has_any = false;
            char transform_buf[256] = "";
            int pos = 0;

            // Check if scale is non-identity
            if (tr->scale_x != 1.0f || tr->scale_y != 1.0f) {
                if (tr->scale_x == tr->scale_y) {
                    pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                                    "scale(%.2f)", tr->scale_x);
                } else {
                    pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                                    "scale(%.2f, %.2f)", tr->scale_x, tr->scale_y);
                }
                has_any = true;
            }

            // Check if translate is non-zero
            if (tr->translate_x != 0 || tr->translate_y != 0) {
                if (has_any) pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos, " ");
                if (tr->translate_y == 0) {
                    pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                                    "translateX(%.0fpx)", tr->translate_x);
                } else if (tr->translate_x == 0) {
                    pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                                    "translateY(%.0fpx)", tr->translate_y);
                } else {
                    pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                                    "translate(%.0fpx, %.0fpx)", tr->translate_x, tr->translate_y);
                }
                has_any = true;
            }

            // Check if rotate is non-zero
            if (tr->rotate != 0) {
                if (has_any) pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos, " ");
                pos += snprintf(transform_buf + pos, sizeof(transform_buf) - pos,
                                "rotate(%.0fdeg)", tr->rotate);
                has_any = true;
            }

            if (has_any) {
                css_generator_write_format(generator, "  transform: %s;\n", transform_buf);
            }
        }

        // Text decoration
        if (props->set_flags & IR_PROP_TEXT_DECORATION) {
            if (props->text_decoration == IR_TEXT_DECORATION_NONE) {
                css_generator_write_string(generator, "  text-decoration: none;\n");
            } else {
                char decoration_buf[64] = "";
                int pos = 0;
                if (props->text_decoration & IR_TEXT_DECORATION_UNDERLINE) {
                    pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, "underline");
                }
                if (props->text_decoration & IR_TEXT_DECORATION_OVERLINE) {
                    if (pos > 0) pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, " ");
                    pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, "overline");
                }
                if (props->text_decoration & IR_TEXT_DECORATION_LINE_THROUGH) {
                    if (pos > 0) pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, " ");
                    pos += snprintf(decoration_buf + pos, sizeof(decoration_buf) - pos, "line-through");
                }
                if (pos > 0) {
                    css_generator_write_format(generator, "  text-decoration: %s;\n", decoration_buf);
                }
            }
        }

        // Box sizing
        if (props->set_flags & IR_PROP_BOX_SIZING) {
            css_generator_write_format(generator, "  box-sizing: %s;\n",
                                       props->box_sizing ? "border-box" : "content-box");
        }

        // Object fit (for images/videos)
        if (props->set_flags & IR_PROP_OBJECT_FIT) {
            const char* value = "fill";
            switch (props->object_fit) {
                case IR_OBJECT_FIT_COVER: value = "cover"; break;
                case IR_OBJECT_FIT_CONTAIN: value = "contain"; break;
                case IR_OBJECT_FIT_NONE: value = "none"; break;
                case IR_OBJECT_FIT_SCALE_DOWN: value = "scale-down"; break;
                default: value = "fill"; break;
            }
            css_generator_write_format(generator, "  object-fit: %s;\n", value);
        }

        css_generator_write_string(generator, "}\n\n");

        // Track generated selectors to prevent duplicate rules from components
        const char* selector = rule->selector->original_selector;

        // Simple class selector = starts with '.', no spaces, no additional dots, no pseudo-classes
        if (selector[0] == '.' &&
            strchr(selector + 1, '.') == NULL &&
            strchr(selector + 1, ' ') == NULL &&
            strchr(selector + 1, ':') == NULL) {
            // Simple class selector like ".feature-card" - mark as generated
            mark_class_generated(generator, selector + 1);  // Skip leading dot
        }
        // Simple element selector = no '.', no '#', no spaces, no pseudo-classes (like body, a, p)
        else if (selector[0] != '.' && selector[0] != '#' &&
                 strchr(selector, ' ') == NULL &&
                 strchr(selector, ':') == NULL &&
                 strchr(selector, '.') == NULL) {
            // Simple element selector like "body", "a", "p" - mark as generated
            mark_class_generated(generator, selector);
        }
    }

    // NOTE: Media queries are output at the end of css_generator_generate()
    // to ensure they come after all base styles
}

const char* css_generator_generate(CSSGenerator* generator, IRComponent* root) {
    if (!generator || !root) return NULL;

    // Set thread-local generator for CSS variable lookups in get_color_string
    tls_css_generator = generator;

    // Clear buffer
    generator->buffer_size = 0;
    if (generator->output_buffer) {
        generator->output_buffer[0] = '\0';
    }

    // Clear generated classes list for fresh deduplication
    for (size_t i = 0; i < generator->generated_class_count; i++) {
        free(generator->generated_classes[i]);
    }
    generator->generated_class_count = 0;

    // Generate CSS header
    css_generator_write_string(generator, "/* Kryon Generated CSS */\n");
    css_generator_write_string(generator, "/* Auto-generated from IR format */\n\n");

    // Generate CSS custom properties from manifest CSS variables (preferred)
    // or fall back to legacy ir_style_vars
    if (generator->css_variables && generator->css_variable_count > 0) {
        css_generator_write_string(generator, ":root {\n");
        for (size_t i = 0; i < generator->css_variable_count; i++) {
            CSSVariable* var = &generator->css_variables[i];
            if (var->name && var->value) {
                css_generator_write_format(generator, "  --%s: %s;\n", var->name, var->value);
            }
        }
        css_generator_write_string(generator, "}\n\n");
    } else {
        // Legacy: Export defined style variables as CSS custom properties
        bool has_style_vars = false;
        for (int i = 0; i < IR_MAX_STYLE_VARS; i++) {
            if (ir_style_vars[i].defined) {
                has_style_vars = true;
                break;
            }
        }
        if (has_style_vars) {
            css_generator_write_string(generator, ":root {\n");
            css_generator_write_string(generator, "  /* Kryon Theme Variables */\n");
            for (int i = 0; i < IR_MAX_STYLE_VARS; i++) {
                if (ir_style_vars[i].defined) {
                    css_generator_write_format(generator, "  --color-%d: rgba(%u, %u, %u, %.2f);\n",
                                              i,
                                              ir_style_vars[i].r,
                                              ir_style_vars[i].g,
                                              ir_style_vars[i].b,
                                              ir_style_vars[i].a / 255.0f);
                }
            }
            css_generator_write_string(generator, "}\n\n");
        }
    }

    // Add default CSS variables for per-instance color customization (always output)
    css_generator_write_string(generator, ":root {\n");
    // Default background color (dark gray)
    css_generator_write_string(generator, "  --bg-color: #3d3d3d;\n");
    // Default text color (white)
    css_generator_write_string(generator, "  --text-color: #ffffff;\n");
    // Default border color (medium gray)
    css_generator_write_string(generator, "  --border-color: #4c5057;\n");
    css_generator_write_string(generator, "}\n\n");

    // Generate global stylesheet rules if available (descendant selectors, etc.)
    if (g_ir_context && g_ir_context->stylesheet) {
        generate_stylesheet_rules(generator, g_ir_context->stylesheet);
    }

    // Generate base styles for body
    // If root is a Body component, merge its styles here instead of generating a .Body class
    // BUT skip if stylesheet already has a body rule (prevent duplicates)
    bool root_is_body = root && root->tag && strcmp(root->tag, "Body") == 0;
    bool stylesheet_has_body = has_generated_class(generator, "body");  // Check if body was in stylesheet

    if (!stylesheet_has_body) {
        css_generator_write_string(generator, "body {\n");
        css_generator_write_string(generator, "  margin: 0;\n");
        css_generator_write_string(generator, "  padding: 0;\n");

        // Use Body component's font-family if set, otherwise default
        if (root_is_body && root->style && root->style->font.family) {
            css_generator_write_format(generator, "  font-family: %s;\n", root->style->font.family);
        } else {
            css_generator_write_string(generator, "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n");
        }

        // Use root component's background color if available
        if (root && root->style) {
            IRColor* bg = &root->style->background;
            if (bg->type == IR_COLOR_SOLID && bg->data.a > 0) {
                const char* color_str = get_color_string(*bg);
                css_generator_write_format(generator, "  background-color: %s;\n", color_str);
            } else if (bg->type == IR_COLOR_VAR_REF) {
                const char* color_str = get_color_string(*bg);
                css_generator_write_format(generator, "  background: %s;\n", color_str);
            }
        }

        // Text color - use Body's color if set, otherwise default based on background
        bool has_text_color = false;
        if (root_is_body && root->style) {
            if (root->style->font.color.type == IR_COLOR_SOLID && root->style->font.color.data.a > 0) {
                const char* color_str = get_color_string(root->style->font.color);
                css_generator_write_format(generator, "  color: %s;\n", color_str);
                has_text_color = true;
            } else if (root->style->font.color.type == IR_COLOR_VAR_REF) {
                const char* color_str = get_color_string(root->style->font.color);
                css_generator_write_format(generator, "  color: %s;\n", color_str);
                has_text_color = true;
            }
        }

        // Default text color if none specified - use white for dark backgrounds
        if (!has_text_color && root && root->style) {
            IRColor* bg = &root->style->background;
            if (bg->type == IR_COLOR_SOLID) {
                // Check if background is dark (simple luminance check)
                float luminance = (0.299f * bg->data.r + 0.587f * bg->data.g + 0.114f * bg->data.b) / 255.0f;
                if (luminance < 0.5f) {
                    css_generator_write_string(generator, "  color: #ffffff;\n");
                } else {
                    css_generator_write_string(generator, "  color: #000000;\n");
                }
                has_text_color = true;
            }
        }

        // Final fallback: white text (most apps use dark themes)
        if (!has_text_color) {
            css_generator_write_string(generator, "  color: #ffffff;\n");
        }

        // If root is Body, add its additional styles
        if (root_is_body && root->style) {

            // Line height
            if (root->style->font.line_height > 0) {
                css_generator_write_format(generator, "  line-height: %s;\n", fmt_float(root->style->font.line_height));
            }
        }

        css_generator_write_string(generator, "}\n\n");
    }

    // Mark Body class as generated so it's skipped later
    if (root_is_body) {
        mark_class_generated(generator, "Body");
    }

    // Generate @keyframes for animations
    generate_keyframes(generator, root);

    // Generate component-specific styles
    generate_component_css(generator, root);

    // Add Modal dialog styles (HTML <dialog> element)
    css_generator_write_string(generator, "/* Modal Dialog Styles */\n");
    css_generator_write_string(generator, "dialog.kryon-modal {\n");
    css_generator_write_string(generator, "  position: fixed;\n");  // Remove from flex layout
    css_generator_write_string(generator, "  inset: 0;\n");
    css_generator_write_string(generator, "  margin: auto;\n");
    css_generator_write_string(generator, "  border: none;\n");
    css_generator_write_string(generator, "  border-radius: 8px;\n");
    css_generator_write_string(generator, "  padding: 0;\n");
    css_generator_write_string(generator, "  max-width: 90vw;\n");
    css_generator_write_string(generator, "  max-height: 90vh;\n");
    css_generator_write_string(generator, "  overflow: hidden;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, "dialog.kryon-modal::backdrop {\n");
    css_generator_write_string(generator, "  background: rgba(0, 0, 0, 0.5);\n");
    css_generator_write_string(generator, "}\n\n");

    // Ensure closed dialogs don't take space (browser compatibility)
    css_generator_write_string(generator, "dialog:not([open]) {\n");
    css_generator_write_string(generator, "  display: none;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".modal-title-bar {\n");
    css_generator_write_string(generator, "  display: flex;\n");
    css_generator_write_string(generator, "  justify-content: space-between;\n");
    css_generator_write_string(generator, "  align-items: center;\n");
    css_generator_write_string(generator, "  padding: 12px 16px;\n");
    css_generator_write_string(generator, "  background: #3d3d3d;\n");
    css_generator_write_string(generator, "  border-radius: 8px 8px 0 0;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".modal-title {\n");
    css_generator_write_string(generator, "  color: white;\n");
    css_generator_write_string(generator, "  font-weight: bold;\n");
    css_generator_write_string(generator, "  font-size: 16px;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".modal-close {\n");
    css_generator_write_string(generator, "  background: none;\n");
    css_generator_write_string(generator, "  border: none;\n");
    css_generator_write_string(generator, "  color: white;\n");
    css_generator_write_string(generator, "  font-size: 20px;\n");
    css_generator_write_string(generator, "  cursor: pointer;\n");
    css_generator_write_string(generator, "  padding: 0 8px;\n");
    css_generator_write_string(generator, "  line-height: 1;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".modal-close:hover {\n");
    css_generator_write_string(generator, "  color: #ff6b6b;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".modal-content {\n");
    css_generator_write_string(generator, "  padding: 24px;\n");
    css_generator_write_string(generator, "}\n\n");


    // Tab component styles
    css_generator_write_string(generator, "/* Tab Component Styles */\n");
    css_generator_write_string(generator, ".kryon-tabs {\n");
    css_generator_write_string(generator, "  display: flex;\n");
    css_generator_write_string(generator, "  flex-direction: column;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-bar {\n");
    css_generator_write_string(generator, "  display: flex;\n");
    css_generator_write_string(generator, "  gap: 4px;\n");
    css_generator_write_string(generator, "  border-bottom: 1px solid #333;\n");
    css_generator_write_string(generator, "  padding: 0 8px;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-bar [role=\"tab\"] {\n");
    css_generator_write_string(generator, "  padding: 8px 16px;\n");
    css_generator_write_string(generator, "  border: none;\n");
    css_generator_write_string(generator, "  background: transparent;\n");
    css_generator_write_string(generator, "  cursor: pointer;\n");
    css_generator_write_string(generator, "  border-bottom: 2px solid transparent;\n");
    css_generator_write_string(generator, "  transition: border-color 0.2s, color 0.2s;\n");
    css_generator_write_string(generator, "  font-size: inherit;\n");
    css_generator_write_string(generator, "  font-family: inherit;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-bar [role=\"tab\"][aria-selected=\"true\"] {\n");
    css_generator_write_string(generator, "  border-bottom-color: #5c6bc0;\n");
    css_generator_write_string(generator, "  background-color: rgba(92, 107, 192, 0.2);\n");
    css_generator_write_string(generator, "  color: #ffffff;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-bar [role=\"tab\"][aria-selected=\"false\"] {\n");
    css_generator_write_string(generator, "  color: #888888;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-bar [role=\"tab\"]:hover {\n");
    css_generator_write_string(generator, "  color: #ffffff;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-bar [role=\"tab\"]:focus {\n");
    css_generator_write_string(generator, "  outline: 2px solid #5c6bc0;\n");
    css_generator_write_string(generator, "  outline-offset: -2px;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-content {\n");
    css_generator_write_string(generator, "  flex: 1;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-content [role=\"tabpanel\"] {\n");
    css_generator_write_string(generator, "  display: block;\n");
    css_generator_write_string(generator, "}\n\n");

    css_generator_write_string(generator, ".kryon-tab-content [role=\"tabpanel\"][hidden] {\n");
    css_generator_write_string(generator, "  display: none;\n");
    css_generator_write_string(generator, "}\n\n");

    // ForEach component styles (dynamic list rendering)
    css_generator_write_string(generator, "/* ForEach Component Styles */\n");
    css_generator_write_string(generator, ".kryon-forEach {\n");
    css_generator_write_string(generator, "  display: contents;\n");
    css_generator_write_string(generator, "}\n\n");

    // Output media queries at the very end (after all base styles)
    if (g_ir_context && g_ir_context->stylesheet) {
        IRStylesheet* stylesheet = g_ir_context->stylesheet;
        if (stylesheet->media_query_count > 0) {
            css_generator_write_string(generator, "/* Media Queries */\n");
            for (uint32_t i = 0; i < stylesheet->media_query_count; i++) {
                IRMediaQuery* mq = &stylesheet->media_queries[i];
                if (mq->condition && mq->css_content) {
                    css_generator_write_format(generator, "@media (%s) {\n", mq->condition);
                    css_generator_write_string(generator, mq->css_content);
                    css_generator_write_string(generator, "}\n\n");
                }
            }
        }
    }

    return generator->output_buffer;
}

// ============================================================================
// Animation and Keyframe Generation
// ============================================================================

// easing_to_css and animation_property_to_css are now from css_property_tables module

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
    if (component->children) {
        for (uint32_t i = 0; i < component->child_count; i++) {
            collect_animations_recursive(generator, component->children[i]);
        }
    }
}

static bool has_animations_recursive(IRComponent* component) {
    if (!component) return false;

    // Check this component for animations
    if (component->style && component->style->animations && component->style->animation_count > 0) {
        return true;
    }

    // Check children recursively
    if (component->children) {
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (has_animations_recursive(component->children[i])) {
                return true;
            }
        }
    }
    return false;
}

static void generate_keyframes(CSSGenerator* generator, IRComponent* root) {
    if (!generator || !root) return;

    // Only output comment and animations if there are actually animations
    if (has_animations_recursive(root)) {
        css_generator_write_string(generator, "/* Keyframe Animations */\n");
        collect_animations_recursive(generator, root);
    }
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