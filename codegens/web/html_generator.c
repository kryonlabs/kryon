/**
 * HTML/CSS/JS Transpiler Implementation
 *
 * This is a CODEGEN FRONTEND, not a rendering backend.
 * See html_generator.h for architectural explanation.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../ir/ir_core.h"
#include "../../ir/ir_plugin.h"
#include "html_generator.h"
#include "css_generator.h"
#include "svg_generator.h"

// HTML tag mapping
typedef struct {
    IRComponentType ir_type;
    const char* html_tag;
    const char* css_class;
} HTMLTagMapping;

static const HTMLTagMapping tag_mappings[] = {
    {IR_COMPONENT_CONTAINER, "div", "kryon-container"},
    {IR_COMPONENT_TEXT, "span", "kryon-text"},
    {IR_COMPONENT_BUTTON, "button", "kryon-button"},
    {IR_COMPONENT_INPUT, "input", "kryon-input"},
    {IR_COMPONENT_CHECKBOX, "input", "kryon-checkbox"},
    {IR_COMPONENT_ROW, "div", "kryon-row"},
    {IR_COMPONENT_COLUMN, "div", "kryon-column"},
    {IR_COMPONENT_CENTER, "div", "kryon-center"},
    {IR_COMPONENT_IMAGE, "img", "kryon-image"},
    {IR_COMPONENT_CANVAS, "canvas", "kryon-canvas"},
    {IR_COMPONENT_CUSTOM, "div", "kryon-custom"},
    // Table components
    {IR_COMPONENT_TABLE, "table", "kryon-table"},
    {IR_COMPONENT_TABLE_HEAD, "thead", "kryon-thead"},
    {IR_COMPONENT_TABLE_BODY, "tbody", "kryon-tbody"},
    {IR_COMPONENT_TABLE_FOOT, "tfoot", "kryon-tfoot"},
    {IR_COMPONENT_TABLE_ROW, "tr", "kryon-tr"},
    {IR_COMPONENT_TABLE_CELL, "td", "kryon-td"},
    {IR_COMPONENT_TABLE_HEADER_CELL, "th", "kryon-th"},
    // Markdown components
    {IR_COMPONENT_HEADING, "div", "kryon-heading"},
    {IR_COMPONENT_PARAGRAPH, "p", "kryon-paragraph"},
    {IR_COMPONENT_BLOCKQUOTE, "blockquote", "kryon-blockquote"},
    {IR_COMPONENT_CODE_BLOCK, "pre", "kryon-code-block"},
    {IR_COMPONENT_HORIZONTAL_RULE, "hr", "kryon-hr"},
    {IR_COMPONENT_LIST, "div", "kryon-list"},
    {IR_COMPONENT_LIST_ITEM, "li", "kryon-list-item"},
    {IR_COMPONENT_LINK, "a", "kryon-link"},
    {IR_COMPONENT_CONTAINER, "div", "kryon-container"}  // Default fallback
};

static const char* get_html_tag(IRComponentType type) {
    for (size_t i = 0; i < sizeof(tag_mappings) / sizeof(tag_mappings[0]); i++) {
        if (tag_mappings[i].ir_type == type) {
            return tag_mappings[i].html_tag;
        }
    }
    return "div";  // Default fallback
}

static const char* get_css_class(IRComponent* component) {
    if (component && component->tag) {
        return component->tag;
    }

    for (size_t i = 0; i < sizeof(tag_mappings) / sizeof(tag_mappings[0]); i++) {
        if (tag_mappings[i].ir_type == component->type) {
            return tag_mappings[i].css_class;
        }
    }
    return "kryon-component";  // Default fallback
}

static const char* get_component_type_name(IRComponentType type) {
    switch (type) {
        case IR_COMPONENT_CONTAINER: return "CONTAINER";
        case IR_COMPONENT_TEXT: return "TEXT";
        case IR_COMPONENT_BUTTON: return "BUTTON";
        case IR_COMPONENT_INPUT: return "INPUT";
        case IR_COMPONENT_CHECKBOX: return "CHECKBOX";
        case IR_COMPONENT_ROW: return "ROW";
        case IR_COMPONENT_COLUMN: return "COLUMN";
        case IR_COMPONENT_CENTER: return "CENTER";
        case IR_COMPONENT_IMAGE: return "IMAGE";
        case IR_COMPONENT_CANVAS: return "CANVAS";
        case IR_COMPONENT_TABLE: return "TABLE";
        case IR_COMPONENT_TABLE_HEAD: return "TABLE_HEAD";
        case IR_COMPONENT_TABLE_BODY: return "TABLE_BODY";
        case IR_COMPONENT_TABLE_FOOT: return "TABLE_FOOT";
        case IR_COMPONENT_TABLE_ROW: return "TABLE_ROW";
        case IR_COMPONENT_TABLE_CELL: return "TABLE_CELL";
        case IR_COMPONENT_TABLE_HEADER_CELL: return "TABLE_HEADER_CELL";
        case IR_COMPONENT_HEADING: return "HEADING";
        case IR_COMPONENT_PARAGRAPH: return "PARAGRAPH";
        case IR_COMPONENT_BLOCKQUOTE: return "BLOCKQUOTE";
        case IR_COMPONENT_CODE_BLOCK: return "CODE_BLOCK";
        case IR_COMPONENT_HORIZONTAL_RULE: return "HORIZONTAL_RULE";
        case IR_COMPONENT_LIST: return "LIST";
        case IR_COMPONENT_LIST_ITEM: return "LIST_ITEM";
        case IR_COMPONENT_LINK: return "LINK";
        case IR_COMPONENT_TAB_GROUP: return "TAB_GROUP";
        case IR_COMPONENT_TAB_BAR: return "TAB_BAR";
        case IR_COMPONENT_TAB: return "TAB";
        case IR_COMPONENT_TAB_CONTENT: return "TAB_CONTENT";
        case IR_COMPONENT_TAB_PANEL: return "TAB_PANEL";
        case IR_COMPONENT_DROPDOWN: return "DROPDOWN";
        case IR_COMPONENT_CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

// Helper to get color as CSS string (borrowed from css_generator.c logic)
static void append_color_string(char* buffer, size_t size, IRColor color) {
    switch (color.type) {
        case IR_COLOR_SOLID:
            snprintf(buffer, size, "rgba(%u, %u, %u, %.2f)",
                    color.data.r, color.data.g, color.data.b, color.data.a / 255.0f);
            break;
        case IR_COLOR_TRANSPARENT:
            snprintf(buffer, size, "transparent");
            break;
        default:
            snprintf(buffer, size, "transparent");
            break;
    }
}

// Helper to get dimension as CSS string
static void append_dimension_string(char* buffer, size_t size, IRDimension dim) {
    switch (dim.type) {
        case IR_DIMENSION_PX:
            snprintf(buffer, size, "%.2fpx", dim.value);
            break;
        case IR_DIMENSION_PERCENT:
            snprintf(buffer, size, "%.2f%%", dim.value);
            break;
        case IR_DIMENSION_AUTO:
            snprintf(buffer, size, "auto");
            break;
        default:
            snprintf(buffer, size, "auto");
            break;
    }
}

// Convert IRAlignment to CSS flexbox value
static const char* alignment_to_css_value(IRAlignment alignment) {
    switch (alignment) {
        case IR_ALIGNMENT_START: return "flex-start";
        case IR_ALIGNMENT_CENTER: return "center";
        case IR_ALIGNMENT_END: return "flex-end";
        case IR_ALIGNMENT_SPACE_BETWEEN: return "space-between";
        case IR_ALIGNMENT_SPACE_AROUND: return "space-around";
        case IR_ALIGNMENT_SPACE_EVENLY: return "space-evenly";
        case IR_ALIGNMENT_STRETCH: return "stretch";
        default: return "flex-start";
    }
}

// Generate inline style attribute for transpilation mode
static void generate_inline_styles(HTMLGenerator* generator, IRComponent* component) {
    if (!component || !component->style) return;

    char style_buffer[4096] = "";
    char temp[256];

    // Width
    if (component->style->width.type != IR_DIMENSION_AUTO) {
        append_dimension_string(temp, sizeof(temp), component->style->width);
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "width: %s; ", temp);
    }

    // Height
    if (component->style->height.type != IR_DIMENSION_AUTO) {
        append_dimension_string(temp, sizeof(temp), component->style->height);
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "height: %s; ", temp);
    }

    // Size constraints (min/max width/height) - supports both PX and PERCENT
    if (component->layout) {
        // Min width
        if (component->layout->min_width.type != IR_DIMENSION_AUTO) {
            append_dimension_string(temp, sizeof(temp), component->layout->min_width);
            snprintf(style_buffer + strlen(style_buffer),
                    sizeof(style_buffer) - strlen(style_buffer),
                    "min-width: %s; ", temp);
        }

        // Max width
        if (component->layout->max_width.type != IR_DIMENSION_AUTO) {
            append_dimension_string(temp, sizeof(temp), component->layout->max_width);
            snprintf(style_buffer + strlen(style_buffer),
                    sizeof(style_buffer) - strlen(style_buffer),
                    "max-width: %s; ", temp);
        }

        // Min height
        if (component->layout->min_height.type != IR_DIMENSION_AUTO) {
            append_dimension_string(temp, sizeof(temp), component->layout->min_height);
            snprintf(style_buffer + strlen(style_buffer),
                    sizeof(style_buffer) - strlen(style_buffer),
                    "min-height: %s; ", temp);
        }

        // Max height
        if (component->layout->max_height.type != IR_DIMENSION_AUTO) {
            append_dimension_string(temp, sizeof(temp), component->layout->max_height);
            snprintf(style_buffer + strlen(style_buffer),
                    sizeof(style_buffer) - strlen(style_buffer),
                    "max-height: %s; ", temp);
        }
    }

    // Background
    if (component->style->background.type != IR_COLOR_TRANSPARENT) {
        append_color_string(temp, sizeof(temp), component->style->background);
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "background-color: %s; ", temp);
    }

    // Border
    if (component->style->border.width > 0) {
        append_color_string(temp, sizeof(temp), component->style->border.color);
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "border: %.2fpx solid %s; ", component->style->border.width, temp);
        if (component->style->border.radius > 0) {
            snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                    "border-radius: %upx; ", component->style->border.radius);
        }
    }

    // Padding
    if (component->style->padding.top != 0 || component->style->padding.right != 0 ||
        component->style->padding.bottom != 0 || component->style->padding.left != 0) {
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "padding: %.2fpx %.2fpx %.2fpx %.2fpx; ",
                component->style->padding.top, component->style->padding.right,
                component->style->padding.bottom, component->style->padding.left);
    }

    // Margin
    if (component->style->margin.top != 0 || component->style->margin.right != 0 ||
        component->style->margin.bottom != 0 || component->style->margin.left != 0) {
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "margin: %.2fpx %.2fpx %.2fpx %.2fpx; ",
                component->style->margin.top, component->style->margin.right,
                component->style->margin.bottom, component->style->margin.left);
    }

    // Font size
    if (component->style->font.size > 0) {
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "font-size: %.2fpx; ", component->style->font.size);
    }

    // Font color
    if (component->style->font.color.type != IR_COLOR_TRANSPARENT) {
        append_color_string(temp, sizeof(temp), component->style->font.color);
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "color: %s; ", temp);
    }

    // Font weight
    if (component->style->font.weight > 0) {
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "font-weight: %u; ", component->style->font.weight);
    } else if (component->style->font.bold) {
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "font-weight: bold; ");
    }

    // Font style (italic)
    if (component->style->font.italic) {
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "font-style: italic; ");
    }

    // Text align
    const char* align_str = NULL;
    switch (component->style->font.align) {
        case IR_TEXT_ALIGN_LEFT: align_str = "left"; break;
        case IR_TEXT_ALIGN_RIGHT: align_str = "right"; break;
        case IR_TEXT_ALIGN_CENTER: align_str = "center"; break;
        case IR_TEXT_ALIGN_JUSTIFY: align_str = "justify"; break;
        default: break;
    }
    if (align_str) {
        snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                "text-align: %s; ", align_str);
    }

    // Flexbox layout properties
    if (component->layout) {
        IRFlexbox* flex = &component->layout->flex;

        // Add display:flex if component has children or explicit flex properties
        if (component->child_count > 0 || flex->gap > 0 || flex->wrap) {
            snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                    "display: flex; ");

            // Flex direction (0=column, 1=row)
            if (flex->direction == 1) {
                snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                        "flex-direction: row; ");
            } else {
                snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                        "flex-direction: column; ");
            }

            // Justify content (main axis alignment)
            if (flex->justify_content != IR_ALIGNMENT_START) {
                const char* justify = alignment_to_css_value(flex->justify_content);
                if (justify) {
                    snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                            "justify-content: %s; ", justify);
                }
            }

            // Align items (cross axis alignment)
            if (flex->cross_axis != IR_ALIGNMENT_STRETCH) {
                const char* align = alignment_to_css_value(flex->cross_axis);
                if (align) {
                    snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                            "align-items: %s; ", align);
                }
            }

            // Gap
            if (flex->gap > 0) {
                snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                        "gap: %upx; ", flex->gap);
            }

            // Flex wrap
            if (flex->wrap) {
                snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                        "flex-wrap: wrap; ");
            }

            // Flex grow/shrink (for child flex items)
            if (flex->grow > 0) {
                snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                        "flex-grow: %u; ", flex->grow);
            }
            if (flex->shrink > 0) {
                snprintf(style_buffer + strlen(style_buffer), sizeof(style_buffer) - strlen(style_buffer),
                        "flex-shrink: %u; ", flex->shrink);
            }
        }
    }

    // Only write style attribute if we have styles
    if (strlen(style_buffer) > 0) {
        html_generator_write_format(generator, " style=\"%s\"", style_buffer);
    }
}

static void escape_html_text(const char* text, char* buffer, size_t buffer_size) {
    if (!text || !buffer || buffer_size == 0) return;

    size_t pos = 0;
    for (const char* p = text; *p && pos < buffer_size - 1; p++) {
        switch (*p) {
            case '&':
                if (pos < buffer_size - 6) {
                    strcpy(buffer + pos, "&amp;");
                    pos += 5;
                }
                break;
            case '<':
                if (pos < buffer_size - 5) {
                    strcpy(buffer + pos, "&lt;");
                    pos += 4;
                }
                break;
            case '>':
                if (pos < buffer_size - 5) {
                    strcpy(buffer + pos, "&gt;");
                    pos += 4;
                }
                break;
            case '"':
                if (pos < buffer_size - 7) {
                    strcpy(buffer + pos, "&quot;");
                    pos += 6;
                }
                break;
            case '\'':
                if (pos < buffer_size - 7) {
                    strcpy(buffer + pos, "&#x27;");
                    pos += 6;
                }
                break;
            default:
                buffer[pos++] = *p;
                break;
        }
    }
    buffer[pos] = '\0';
}


HtmlGeneratorOptions html_generator_default_options(void) {
    HtmlGeneratorOptions opts = {
        .mode = HTML_MODE_DISPLAY,
        .minify = false,
        .inline_css = true,
        .preserve_ids = false
    };
    return opts;
}

HTMLGenerator* html_generator_create() {
    return html_generator_create_with_options(html_generator_default_options());
}

HTMLGenerator* html_generator_create_with_options(HtmlGeneratorOptions options) {
    HTMLGenerator* generator = malloc(sizeof(HTMLGenerator));
    if (!generator) return NULL;

    generator->output_buffer = NULL;
    generator->buffer_size = 0;
    generator->buffer_capacity = 0;
    generator->indent_level = 0;
    generator->pretty_print = !options.minify;
    generator->options = options;

    return generator;
}

void html_generator_destroy(HTMLGenerator* generator) {
    if (!generator) return;

    if (generator->output_buffer) {
        free(generator->output_buffer);
    }
    free(generator);
}

void html_generator_set_pretty_print(HTMLGenerator* generator, bool pretty) {
    generator->pretty_print = pretty;
}

static void html_generator_write_indent(HTMLGenerator* generator) {
    if (!generator->pretty_print) return;

    for (int i = 0; i < generator->indent_level; i++) {
        html_generator_write_string(generator, "  ");
    }
}

bool html_generator_write_string(HTMLGenerator* generator, const char* string) {
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
        // CRITICAL: Ensure buffer is null-terminated at current position
        // so strcpy knows where to write
        generator->output_buffer[generator->buffer_size] = '\0';
    }

    // Use strcpy instead of strcat - we already know the offset
    // This avoids strcat searching for '\0' in uninitialized memory
    strcpy(generator->output_buffer + generator->buffer_size, string);
    generator->buffer_size += string_len;
    // Null terminator already added by strcpy

    return true;
}

bool html_generator_write_format(HTMLGenerator* generator, const char* format, ...) {
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

static bool generate_component_html(HTMLGenerator* generator, IRComponent* component) {
    if (!generator || !component) return false;

    // Check if a plugin has registered a web renderer for this component type
    if (ir_plugin_has_web_renderer(component->type)) {
        // Use plugin renderer - get HTML from plugin
        char* plugin_html = ir_plugin_render_web_component(component, "light");
        if (plugin_html) {
            html_generator_write_indent(generator);
            html_generator_write_string(generator, plugin_html);
            html_generator_write_string(generator, "\n");
            free(plugin_html);
            return true;
        }
        // Fall through to default if plugin returns NULL
    }

    const char* tag = get_html_tag(component->type);

    // CRITICAL: Override tag for semantic HTML elements BEFORE writing opening tag
    // This prevents mismatched tags like <div>...</h1> or <div>...</ul>
    switch (component->type) {
        case IR_COMPONENT_HEADING: {
            IRHeadingData* data = (IRHeadingData*)component->custom_data;
            if (data) {
                tag = (data->level == 1) ? "h1" :
                      (data->level == 2) ? "h2" :
                      (data->level == 3) ? "h3" :
                      (data->level == 4) ? "h4" :
                      (data->level == 5) ? "h5" : "h6";
            }
            break;
        }
        case IR_COMPONENT_LIST: {
            IRListData* data = (IRListData*)component->custom_data;
            if (data) {
                tag = (data->type == IR_LIST_ORDERED) ? "ol" : "ul";
            }
            break;
        }
        default:
            break;
    }

    bool is_self_closing = (component->type == IR_COMPONENT_INPUT ||
                          component->type == IR_COMPONENT_CHECKBOX ||
                          component->type == IR_COMPONENT_IMAGE ||
                          component->type == IR_COMPONENT_HORIZONTAL_RULE);

    html_generator_write_indent(generator);
    html_generator_write_format(generator, "<%s", tag);

    // Generate attributes directly using the generator
    html_generator_write_format(generator, " id=\"kryon-%u\"", component->id);
    const char* css_class = get_css_class(component);

    // DEBUG: Log code block rendering
    if (component->type == IR_COMPONENT_CODE_BLOCK) {
        fprintf(stderr, "[HTML_GEN] CODE_BLOCK: id=%u, tag='%s', css_class='%s'\n",
                component->id, component->tag ? component->tag : "NULL", css_class);
    }

    html_generator_write_format(generator, " class=\"%s\"", css_class);

    // In transpilation mode, add metadata attributes for roundtrip
    if (generator->options.mode == HTML_MODE_TRANSPILE) {
        // Add component type for reconstruction
        html_generator_write_format(generator, " data-ir-type=\"%s\"",
                                    get_component_type_name(component->type));

        // Add component ID for debugging/tracking
        if (generator->options.preserve_ids) {
            html_generator_write_format(generator, " data-ir-id=\"%u\"", component->id);
        }

        // Add inline styles for transpilation mode
        if (generator->options.inline_css) {
            generate_inline_styles(generator, component);
        }
    }

    // Add custom tag if specified
    if (component->tag && strcmp(component->tag, "") != 0) {
        html_generator_write_format(generator, " data-tag=\"%s\"", component->tag);
    }

    // Component-specific attributes
    switch (component->type) {
        case IR_COMPONENT_BUTTON:
            if (component->text_content) {
                char escaped_text[1024];
                escape_html_text(component->text_content, escaped_text, sizeof(escaped_text));
                html_generator_write_format(generator, " data-text=\"%s\"", escaped_text);
            }
            break;

        case IR_COMPONENT_INPUT:
            if (component->custom_data) {  // placeholder
                html_generator_write_format(generator, " placeholder=\"%s\"", component->custom_data);
            }
            html_generator_write_string(generator, " type=\"text\"");
            break;

        case IR_COMPONENT_CHECKBOX:
            html_generator_write_string(generator, " type=\"checkbox\"");
            if (component->text_content) {
                html_generator_write_format(generator, " data-label=\"%s\"", component->text_content);
            }
            break;

        case IR_COMPONENT_IMAGE:
            fprintf(stderr, "[HTML_GEN] IMAGE component id=%u, custom_data=%s, text_content=%s\n",
                    component->id,
                    component->custom_data ? component->custom_data : "NULL",
                    component->text_content ? component->text_content : "NULL");
            if (component->custom_data) {  // src URL
                html_generator_write_format(generator, " src=\"%s\"", component->custom_data);
            }
            if (component->text_content) {  // alt text
                char escaped_alt[1024];
                escape_html_text(component->text_content, escaped_alt, sizeof(escaped_alt));
                html_generator_write_format(generator, " alt=\"%s\"", escaped_alt);
            }
            break;

        case IR_COMPONENT_CANVAS:
            if (component->style) {
                html_generator_write_format(generator, " width=\"%u\"", (uint32_t)component->style->width.value);
                html_generator_write_format(generator, " height=\"%u\"", (uint32_t)component->style->height.value);
            }
            break;

        case IR_COMPONENT_TABLE_CELL:
        case IR_COMPONENT_TABLE_HEADER_CELL: {
            IRTableCellData* cell_data = (IRTableCellData*)component->custom_data;
            if (cell_data) {
                if (cell_data->colspan > 1) {
                    html_generator_write_format(generator, " colspan=\"%u\"", cell_data->colspan);
                }
                if (cell_data->rowspan > 1) {
                    html_generator_write_format(generator, " rowspan=\"%u\"", cell_data->rowspan);
                }
                // Emit alignment as inline style
                if (cell_data->alignment == IR_ALIGNMENT_CENTER) {
                    html_generator_write_string(generator, " style=\"text-align: center;\"");
                } else if (cell_data->alignment == IR_ALIGNMENT_END) {
                    html_generator_write_string(generator, " style=\"text-align: right;\"");
                }
            }
            break;
        }

        // Markdown components
        case IR_COMPONENT_HEADING: {
            IRHeadingData* data = (IRHeadingData*)component->custom_data;
            if (data) {
                // Note: tag override now happens before opening tag (see lines 247-268)
                html_generator_write_format(generator, " data-level=\"%u\"", data->level);
                if (data->id) {
                    html_generator_write_format(generator, " id=\"%s\"", data->id);
                }
            }
            break;
        }

        case IR_COMPONENT_CODE_BLOCK: {
            IRCodeBlockData* data = (IRCodeBlockData*)component->custom_data;
            if (data && data->language) {
                html_generator_write_format(generator, " data-language=\"%s\"", data->language);
            }
            break;
        }

        case IR_COMPONENT_LIST: {
            IRListData* data = (IRListData*)component->custom_data;
            if (data) {
                // Note: tag override now happens before opening tag (see lines 247-268)
                if (data->type == IR_LIST_ORDERED && data->start > 1) {
                    html_generator_write_format(generator, " start=\"%u\"", data->start);
                }
            }
            break;
        }

        case IR_COMPONENT_LIST_ITEM: {
            IRListItemData* data = (IRListItemData*)component->custom_data;
            if (data && data->number > 0) {
                html_generator_write_format(generator, " value=\"%u\"", data->number);
            }
            break;
        }

        case IR_COMPONENT_LINK: {
            IRLinkData* data = (IRLinkData*)component->custom_data;
            if (data) {
                if (data->url) {
                    char escaped_url[2048];
                    escape_html_text(data->url, escaped_url, sizeof(escaped_url));
                    html_generator_write_format(generator, " href=\"%s\"", escaped_url);
                }
                if (data->title) {
                    char escaped_title[1024];
                    escape_html_text(data->title, escaped_title, sizeof(escaped_title));
                    html_generator_write_format(generator, " title=\"%s\"", escaped_title);
                }
            }
            break;
        }

        default:
            break;
    }

    // Add event handlers (skip in transpilation mode - no JS needed)
    if (generator->options.mode == HTML_MODE_DISPLAY) {
        for (IREvent* event = component->events; event; event = event->next) {
            switch (event->type) {
                case IR_EVENT_CLICK:
                    html_generator_write_format(generator, " onclick=\"kryon_handle_click(%u, '%s')\"",
                            component->id, event->logic_id ? event->logic_id : "");
                    break;
                case IR_EVENT_HOVER:
                    html_generator_write_format(generator, " onmouseover=\"kryon_handle_hover(%u, '%s')\"",
                            component->id, event->logic_id ? event->logic_id : "");
                    html_generator_write_format(generator, " onmouseout=\"kryon_handle_leave(%u)\"", component->id);
                    break;
                case IR_EVENT_FOCUS:
                    html_generator_write_format(generator, " onfocus=\"kryon_handle_focus(%u)\"", component->id);
                    html_generator_write_format(generator, " onblur=\"kryon_handle_blur(%u)\"", component->id);
                    break;
                default:
                    break;
            }
        }
    }

    if (is_self_closing) {
        html_generator_write_string(generator, " />\n");
        return true;
    }

    html_generator_write_string(generator, ">\n");

    generator->indent_level++;

    // Special handling for markdown components with custom_data text
    if (component->type == IR_COMPONENT_HEADING) {
        IRHeadingData* data = (IRHeadingData*)component->custom_data;
        if (data && data->text) {
            html_generator_write_indent(generator);
            char escaped_text[2048];
            escape_html_text(data->text, escaped_text, sizeof(escaped_text));
            html_generator_write_format(generator, "%s\n", escaped_text);
        }
    } else if (component->type == IR_COMPONENT_CODE_BLOCK) {
        IRCodeBlockData* data = (IRCodeBlockData*)component->custom_data;
        if (data && data->code) {
            html_generator_write_indent(generator);
            html_generator_write_string(generator, "<code");
            if (data->language) {
                html_generator_write_format(generator, " class=\"language-%s\"", data->language);
            }
            html_generator_write_string(generator, ">");
            // Code blocks need special escaping to preserve formatting
            char escaped_code[4096];
            escape_html_text(data->code, escaped_code, sizeof(escaped_code));
            html_generator_write_string(generator, escaped_code);
            html_generator_write_string(generator, "</code>\n");
        }
    }
    // Write text content if present (for non-markdown components)
    else if (component->text_content && strlen(component->text_content) > 0) {
        html_generator_write_indent(generator);
        char escaped_text[1024];
        escape_html_text(component->text_content, escaped_text, sizeof(escaped_text));
        html_generator_write_format(generator, "%s\n", escaped_text);
    }

    // Write children
    for (uint32_t i = 0; i < component->child_count; i++) {
        generate_component_html(generator, component->children[i]);
    }

    generator->indent_level--;

    html_generator_write_indent(generator);
    html_generator_write_format(generator, "</%s>\n", tag);

    return true;
}

const char* html_generator_generate(HTMLGenerator* generator, IRComponent* root) {
    if (!generator || !root) return NULL;

    // Clear buffer
    generator->buffer_size = 0;
    if (generator->output_buffer) {
        generator->output_buffer[0] = '\0';
    }

    // Generate HTML document
    html_generator_write_string(generator, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
    html_generator_write_string(generator, "  <meta charset=\"UTF-8\">\n");
    html_generator_write_string(generator, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    html_generator_write_string(generator, "  <title>Kryon Web Application</title>\n");

    // Add CSS - either inline <style> or external <link>
    if (generator->options.inline_css) {
        // Generate inline CSS in <style> block
        CSSGenerator* css_gen = css_generator_create();
        if (css_gen) {
            const char* css = css_generator_generate(css_gen, root);
            if (css && strlen(css) > 0) {
                html_generator_write_string(generator, "  <style>\n");
                html_generator_write_string(generator, css);
                html_generator_write_string(generator, "  </style>\n");
            }
            css_generator_destroy(css_gen);
        }
    } else {
        // Link to external CSS file
        html_generator_write_string(generator, "  <link rel=\"stylesheet\" href=\"kryon.css\">\n");
    }

    // Add JavaScript runtime only in display mode
    if (generator->options.mode == HTML_MODE_DISPLAY) {
        html_generator_write_string(generator, "  <script src=\"kryon.js\"></script>\n");
    }

    html_generator_write_string(generator, "</head>\n<body>\n");

    generator->indent_level = 1;
    generate_component_html(generator, root);
    generator->indent_level = 0;

    html_generator_write_string(generator, "</body>\n</html>\n");

    return generator->output_buffer;
}

bool html_generator_write_to_file(HTMLGenerator* generator, IRComponent* root, const char* filename) {
    const char* html = html_generator_generate(generator, root);
    if (!html) return false;

    FILE* file = fopen(filename, "w");
    if (!file) return false;

    bool success = (fprintf(file, "%s", html) >= 0);
    fclose(file);

    return success;
}

size_t html_generator_get_size(HTMLGenerator* generator) {
    return generator ? generator->buffer_size : 0;
}

const char* html_generator_get_buffer(HTMLGenerator* generator) {
    return generator ? generator->output_buffer : NULL;
}