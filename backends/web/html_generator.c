#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../ir/ir_core.h"
#include "../../ir/ir_plugin.h"
#include "html_generator.h"
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


HTMLGenerator* html_generator_create() {
    HTMLGenerator* generator = malloc(sizeof(HTMLGenerator));
    if (!generator) return NULL;

    generator->output_buffer = NULL;
    generator->buffer_size = 0;
    generator->buffer_capacity = 0;
    generator->indent_level = 0;
    generator->pretty_print = true;

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
    }

    strcat(generator->output_buffer + generator->buffer_size, string);
    generator->buffer_size += string_len;
    generator->output_buffer[generator->buffer_size] = '\0';

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
    bool is_self_closing = (component->type == IR_COMPONENT_INPUT ||
                          component->type == IR_COMPONENT_CHECKBOX ||
                          component->type == IR_COMPONENT_IMAGE ||
                          component->type == IR_COMPONENT_HORIZONTAL_RULE);

    html_generator_write_indent(generator);
    html_generator_write_format(generator, "<%s", tag);

    // Generate attributes directly using the generator
    html_generator_write_format(generator, " id=\"kryon-%u\"", component->id);
    const char* css_class = get_css_class(component);
    html_generator_write_format(generator, " class=\"%s\"", css_class);

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

        case IR_COMPONENT_FLOWCHART: {
            // Generate SVG for flowchart
            SVGOptions svg_opts = svg_options_default();
            svg_opts.theme = SVG_THEME_DEFAULT;  // Could read from context if available
            svg_opts.interactive = true;

            char* svg = flowchart_to_svg(component, &svg_opts);
            if (svg) {
                // Close the opening tag
                html_generator_write_string(generator, ">\n");
                generator->indent_level++;

                // Write SVG content
                html_generator_write_indent(generator);
                html_generator_write_string(generator, svg);
                html_generator_write_string(generator, "\n");
                free(svg);

                generator->indent_level--;
                html_generator_write_indent(generator);
                html_generator_write_format(generator, "</%s>\n", tag);
                return true;
            }
            break;
        }

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
                // Use semantic heading tag (h1-h6) instead of div
                html_generator_write_format(generator, " data-level=\"%u\"", data->level);
                // Override tag to use actual heading element
                tag = (data->level == 1) ? "h1" :
                      (data->level == 2) ? "h2" :
                      (data->level == 3) ? "h3" :
                      (data->level == 4) ? "h4" :
                      (data->level == 5) ? "h5" : "h6";
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
                // Use semantic list tags (ol/ul) instead of div
                tag = (data->type == IR_LIST_ORDERED) ? "ol" : "ul";
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

    // Add event handlers
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
    html_generator_write_string(generator, "  <link rel=\"stylesheet\" href=\"kryon.css\">\n");
    html_generator_write_string(generator, "  <script src=\"kryon.js\"></script>\n");
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