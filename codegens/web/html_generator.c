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
#include <fcntl.h>
#include <unistd.h>

#include "../../ir/include/ir_core.h"
#include "../../ir/include/ir_capability.h"
#include "../../ir/include/ir_logic.h"
#include "../../third_party/cJSON/cJSON.h"
#include "html_generator.h"
#include "css_generator.h"
#include "style_analyzer.h"

// Debug: write to file when this translation unit is loaded
__attribute__((constructor))
static void debug_html_gen_loaded(void) {
    FILE* f = fopen("/tmp/html_gen_loaded.txt", "w");
    if (f) {
        fprintf(f, "html_generator.c loaded successfully!\n");
        fclose(f);
    }
}

// Access global IR context for metadata
extern IRContext* g_ir_context;

// Helper to collect a handler for the Lua registry
static void collect_handler(HTMLGenerator* generator, uint32_t component_id, const char* code) {
    if (!generator || !code) return;

    // Grow array if needed
    if (generator->handler_count >= generator->handler_capacity) {
        size_t new_capacity = generator->handler_capacity == 0 ? 16 : generator->handler_capacity * 2;
        CollectedHandler* new_handlers = realloc(generator->handlers, new_capacity * sizeof(CollectedHandler));
        if (!new_handlers) return;
        generator->handlers = new_handlers;
        generator->handler_capacity = new_capacity;
    }

    // Add handler
    generator->handlers[generator->handler_count].component_id = component_id;
    generator->handlers[generator->handler_count].code = strdup(code);
    generator->handler_count++;
}

// HTML semantic tag mapping (NO kryon-* classes)
// Returns NULL for components that should output raw content without wrapper tags
static const char* get_html_tag(IRComponentType type) {
    switch (type) {
        // Raw text - no wrapper tag
        case IR_COMPONENT_TEXT: return NULL;

        // Template placeholders - output {{name}} syntax directly
        case IR_COMPONENT_PLACEHOLDER: return NULL;

        case IR_COMPONENT_BUTTON: return "button";
        case IR_COMPONENT_INPUT: return "input";
        case IR_COMPONENT_CHECKBOX: return "input";
        case IR_COMPONENT_IMAGE: return "img";
        case IR_COMPONENT_CANVAS: return "canvas";

        // Inline semantic elements (for rich text)
        case IR_COMPONENT_SPAN: return "span";
        case IR_COMPONENT_STRONG: return "strong";
        case IR_COMPONENT_EM: return "em";
        case IR_COMPONENT_CODE_INLINE: return "code";
        case IR_COMPONENT_SMALL: return "small";
        case IR_COMPONENT_MARK: return "mark";

        // Table elements
        case IR_COMPONENT_TABLE: return "table";
        case IR_COMPONENT_TABLE_HEAD: return "thead";
        case IR_COMPONENT_TABLE_BODY: return "tbody";
        case IR_COMPONENT_TABLE_FOOT: return "tfoot";
        case IR_COMPONENT_TABLE_ROW: return "tr";
        case IR_COMPONENT_TABLE_CELL: return "td";
        case IR_COMPONENT_TABLE_HEADER_CELL: return "th";

        // Markdown/semantic elements
        case IR_COMPONENT_PARAGRAPH: return "p";
        case IR_COMPONENT_BLOCKQUOTE: return "blockquote";
        case IR_COMPONENT_CODE_BLOCK: return "pre";
        case IR_COMPONENT_HORIZONTAL_RULE: return "hr";
        case IR_COMPONENT_LIST_ITEM: return "li";
        case IR_COMPONENT_LINK: return "a";

        // Modal dialog
        case IR_COMPONENT_MODAL: return "dialog";

        // Tab components - use semantic divs with ARIA roles
        case IR_COMPONENT_TAB_GROUP: return "div";
        case IR_COMPONENT_TAB_BAR: return "div";
        case IR_COMPONENT_TAB: return "button";
        case IR_COMPONENT_TAB_CONTENT: return "div";
        case IR_COMPONENT_TAB_PANEL: return "div";

        // ForEach (dynamic list rendering) - renders as div with special class
        case IR_COMPONENT_FOR_EACH: return "div";

        // Layout containers (all map to div)
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
        case IR_COMPONENT_CENTER:
        case IR_COMPONENT_HEADING:  // Will be overridden to h1-h6
        case IR_COMPONENT_LIST:     // Will be overridden to ul/ol
        case IR_COMPONENT_CUSTOM:
        default:
            return "div";
    }
}

// Generate ID prefix for semantic IDs (e.g., btn-1, input-2)
static const char* get_id_prefix(IRComponentType type) {
    switch (type) {
        case IR_COMPONENT_BUTTON: return "btn";
        case IR_COMPONENT_INPUT: return "input";
        case IR_COMPONENT_CHECKBOX: return "checkbox";
        case IR_COMPONENT_TAB_GROUP: return "tabgroup";
        case IR_COMPONENT_TAB_BAR: return "tabbar";
        case IR_COMPONENT_TAB: return "tab";
        case IR_COMPONENT_TAB_CONTENT: return "tabcontent";
        case IR_COMPONENT_TAB_PANEL: return "tabpanel";
        default: return "elem";
    }
}

// Structure to hold parsed custom attributes
typedef struct {
    char* element_id;      // Custom element ID (e.g., "month-display")
    cJSON* data_attrs;     // Data attributes as JSON object
    char* action;          // Named action for event dispatch (e.g., "prevMonth")
} CustomAttrs;

// Parse custom attributes from custom_data JSON
// Returns true if valid JSON with elementId or data was found
// The caller is responsible for cleaning up (call cleanup_custom_attrs)
static bool parse_custom_attrs(const char* custom_data, CustomAttrs* attrs) {
    if (!custom_data || !attrs) return false;

    // Initialize
    attrs->element_id = NULL;
    attrs->data_attrs = NULL;
    attrs->action = NULL;

    // Check if custom_data looks like JSON (starts with '{')
    // Skip if it's already used for other purposes (modal state, placeholder, etc.)
    if (custom_data[0] != '{') return false;

    cJSON* json = cJSON_Parse(custom_data);
    if (!json) return false;

    // Check for elementId
    cJSON* id_item = cJSON_GetObjectItem(json, "elementId");
    if (id_item && cJSON_IsString(id_item)) {
        attrs->element_id = strdup(id_item->valuestring);
    }

    // Check for data attributes
    cJSON* data_item = cJSON_GetObjectItem(json, "data");
    if (data_item && cJSON_IsObject(data_item)) {
        // Detach the data object so we can keep it after deleting the parent
        attrs->data_attrs = cJSON_Duplicate(data_item, true);
    }

    // Check for action (named action for event dispatch)
    cJSON* action_item = cJSON_GetObjectItem(json, "action");
    if (action_item && cJSON_IsString(action_item)) {
        attrs->action = strdup(action_item->valuestring);
    }

    cJSON_Delete(json);

    return (attrs->element_id != NULL || attrs->data_attrs != NULL || attrs->action != NULL);
}

// Clean up custom attributes
static void cleanup_custom_attrs(CustomAttrs* attrs) {
    if (!attrs) return;
    if (attrs->element_id) {
        free(attrs->element_id);
        attrs->element_id = NULL;
    }
    if (attrs->data_attrs) {
        cJSON_Delete(attrs->data_attrs);
        attrs->data_attrs = NULL;
    }
    if (attrs->action) {
        free(attrs->action);
        attrs->action = NULL;
    }
}

// Output data attributes from a JSON object
static void output_data_attrs(HTMLGenerator* generator, cJSON* data) {
    if (!data) return;

    cJSON* item = NULL;
    cJSON_ArrayForEach(item, data) {
        if (cJSON_IsString(item)) {
            html_generator_write_format(generator, " data-%s=\"%s\"",
                item->string, item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            html_generator_write_format(generator, " data-%s=\"%.0f\"",
                item->string, item->valuedouble);
        } else if (cJSON_IsBool(item)) {
            html_generator_write_format(generator, " data-%s=\"%s\"",
                item->string, cJSON_IsTrue(item) ? "true" : "false");
        }
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

// Helper to format a dimension value as CSS string
static void format_dimension_inline(char* buffer, size_t size, IRDimension dim) {
    switch (dim.type) {
        case IR_DIMENSION_PX:
            snprintf(buffer, size, "%.0fpx", dim.value);
            break;
        case IR_DIMENSION_PERCENT:
            snprintf(buffer, size, "%.0f%%", dim.value);
            break;
        case IR_DIMENSION_VW:
            snprintf(buffer, size, "%.0fvw", dim.value);
            break;
        case IR_DIMENSION_VH:
            snprintf(buffer, size, "%.0fvh", dim.value);
            break;
        case IR_DIMENSION_REM:
            snprintf(buffer, size, "%.2frem", dim.value);
            break;
        case IR_DIMENSION_EM:
            snprintf(buffer, size, "%.2fem", dim.value);
            break;
        default:
            buffer[0] = '\0';  // Don't output for AUTO
            break;
    }
}

// Generate CSS variable inline styles for components with custom colors AND sizing
// This outputs per-instance properties that can't be shared via CSS classes
static void generate_css_var_styles(HTMLGenerator* generator, IRComponent* component) {
    if (!component || !component->style) return;

    // Skip for Modal (CSS handles visibility)
    if (component->type == IR_COMPONENT_MODAL) return;

    // Skip for tab buttons (CSS handles via aria-selected)
    bool is_tab_button = (component->type == IR_COMPONENT_TAB) ||
                         (component->type == IR_COMPONENT_BUTTON &&
                          component->parent && component->parent->type == IR_COMPONENT_TAB_BAR);
    if (is_tab_button) return;

    char style_buffer[2048] = "";
    char temp[256];
    bool has_styles = false;

    // Background color variable - only if solid and visible
    if (component->style->background.type == IR_COLOR_SOLID &&
        component->style->background.data.a > 0) {
        append_color_string(temp, sizeof(temp), component->style->background);
        snprintf(style_buffer + strlen(style_buffer),
                sizeof(style_buffer) - strlen(style_buffer),
                "--bg-color: %s; ", temp);
        has_styles = true;
    }

    // Text color variable - only for text components
    bool is_text_component = (component->type == IR_COMPONENT_TEXT ||
                              component->type == IR_COMPONENT_BUTTON ||
                              component->type == IR_COMPONENT_INPUT ||
                              component->type == IR_COMPONENT_SPAN ||
                              component->type == IR_COMPONENT_STRONG ||
                              component->type == IR_COMPONENT_EM ||
                              component->type == IR_COMPONENT_LINK ||
                              component->type == IR_COMPONENT_PARAGRAPH ||
                              component->type == IR_COMPONENT_HEADING);
    if (is_text_component &&
        component->style->font.color.type == IR_COLOR_SOLID &&
        component->style->font.color.data.a > 0) {
        append_color_string(temp, sizeof(temp), component->style->font.color);
        snprintf(style_buffer + strlen(style_buffer),
                sizeof(style_buffer) - strlen(style_buffer),
                "--text-color: %s; ", temp);
        has_styles = true;
    }

    // Border color variable - only if border has width
    if (component->style->border.width > 0 &&
        component->style->border.color.type == IR_COLOR_SOLID) {
        append_color_string(temp, sizeof(temp), component->style->border.color);
        snprintf(style_buffer + strlen(style_buffer),
                sizeof(style_buffer) - strlen(style_buffer),
                "--border-color: %s; ", temp);
        has_styles = true;
    }

    // ========================================================================
    // Sizing properties - output as inline styles for per-component values
    // ========================================================================

    // Width (when not auto)
    if (component->style->width.type != IR_DIMENSION_AUTO) {
        format_dimension_inline(temp, sizeof(temp), component->style->width);
        if (temp[0] != '\0') {
            snprintf(style_buffer + strlen(style_buffer),
                    sizeof(style_buffer) - strlen(style_buffer),
                    "width: %s; ", temp);
            has_styles = true;
        }
    }

    // Height (when not auto)
    if (component->style->height.type != IR_DIMENSION_AUTO) {
        format_dimension_inline(temp, sizeof(temp), component->style->height);
        if (temp[0] != '\0') {
            snprintf(style_buffer + strlen(style_buffer),
                    sizeof(style_buffer) - strlen(style_buffer),
                    "height: %s; ", temp);
            has_styles = true;
        }
    }

    // Padding (when any side is non-zero)
    if (component->style->padding.set_flags != 0) {
        float top = component->style->padding.top;
        float right = component->style->padding.right;
        float bottom = component->style->padding.bottom;
        float left = component->style->padding.left;

        if (top > 0 || right > 0 || bottom > 0 || left > 0) {
            // Use shortest shorthand notation
            if (top == right && right == bottom && bottom == left) {
                snprintf(style_buffer + strlen(style_buffer),
                        sizeof(style_buffer) - strlen(style_buffer),
                        "padding: %.0fpx; ", top);
            } else if (top == bottom && left == right) {
                snprintf(style_buffer + strlen(style_buffer),
                        sizeof(style_buffer) - strlen(style_buffer),
                        "padding: %.0fpx %.0fpx; ", top, right);
            } else {
                snprintf(style_buffer + strlen(style_buffer),
                        sizeof(style_buffer) - strlen(style_buffer),
                        "padding: %.0fpx %.0fpx %.0fpx %.0fpx; ", top, right, bottom, left);
            }
            has_styles = true;
        }
    }

    // Border-radius (when > 0)
    if (component->style->border.radius > 0) {
        snprintf(style_buffer + strlen(style_buffer),
                sizeof(style_buffer) - strlen(style_buffer),
                "border-radius: %upx; ", component->style->border.radius);
        has_styles = true;
    }

    // Gap (for flex containers)
    if (component->layout && component->layout->flex.gap > 0) {
        snprintf(style_buffer + strlen(style_buffer),
                sizeof(style_buffer) - strlen(style_buffer),
                "gap: %upx; ", component->layout->flex.gap);
        has_styles = true;
    }

    // Font size (when specified)
    if (component->style->font.size > 0) {
        snprintf(style_buffer + strlen(style_buffer),
                sizeof(style_buffer) - strlen(style_buffer),
                "font-size: %.0fpx; ", component->style->font.size);
        has_styles = true;
    }

    // Only write style attribute if we have any inline styles
    if (has_styles && strlen(style_buffer) > 0) {
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

/* Length-aware version for substring escaping (used by syntax highlighting) */
static void escape_html_text_len(const char* text, size_t text_len, char* buffer, size_t buffer_size) {
    if (!text || !buffer || buffer_size == 0) return;

    size_t pos = 0;
    for (size_t i = 0; i < text_len && pos < buffer_size - 1; i++) {
        char c = text[i];
        switch (c) {
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
                buffer[pos++] = c;
                break;
        }
    }
    buffer[pos] = '\0';
}


HtmlGeneratorOptions html_generator_default_options(void) {
    HtmlGeneratorOptions opts = {
        .minify = false,
        .embedded_css = false,  // Use external CSS file by default
        .include_runtime = true
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
    generator->logic_block = NULL;
    generator->manifest = NULL;
    generator->metadata = NULL;  // CRITICAL: initialize to NULL

    // Initialize handler collection
    generator->handlers = NULL;
    generator->handler_count = 0;
    generator->handler_capacity = 0;

    return generator;
}

void html_generator_destroy(HTMLGenerator* generator) {
    if (!generator) return;

    if (generator->output_buffer) {
        free(generator->output_buffer);
    }

    // Free collected handlers
    if (generator->handlers) {
        for (size_t i = 0; i < generator->handler_count; i++) {
            if (generator->handlers[i].code) {
                free(generator->handlers[i].code);
            }
        }
        free(generator->handlers);
    }

    free(generator);
}

void html_generator_set_pretty_print(HTMLGenerator* generator, bool pretty) {
    generator->pretty_print = pretty;
}

void html_generator_set_manifest(HTMLGenerator* generator, IRReactiveManifest* manifest) {
    if (!generator) return;
    generator->manifest = manifest;  // Store reference (not owned)
}

void html_generator_set_metadata(HTMLGenerator* generator, IRSourceMetadata* metadata) {
    if (!generator) return;
    generator->metadata = metadata;  // Store reference (not owned)
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

// Check if a component type is inline (for HTML output formatting)
static bool is_inline_component(IRComponent* component) {
    if (!component) return false;

    // TEXT components are always inline
    if (component->type == IR_COMPONENT_TEXT) return true;

    // Inline semantic elements
    switch (component->type) {
        case IR_COMPONENT_SPAN:
        case IR_COMPONENT_LINK:
        case IR_COMPONENT_STRONG:
        case IR_COMPONENT_EM:
        case IR_COMPONENT_CODE_INLINE:
        case IR_COMPONENT_SMALL:
        case IR_COMPONENT_MARK:
            return true;
        default:
            break;
    }

    return false;
}

// Check if all children are inline elements (for determining output format)
static bool has_only_inline_content(IRComponent* component) {
    if (!component) return false;

    // If there's text content and no children, it's inline
    if (component->text_content && component->child_count == 0) {
        return true;
    }

    // Check all children
    for (uint32_t i = 0; i < component->child_count; i++) {
        IRComponent* child = component->children[i];
        if (!is_inline_component(child)) {
            return false;
        }
    }

    // Has only inline children (or text + inline children)
    return component->child_count > 0 || component->text_content;
}

// Helper: escape a string for JavaScript (single quotes)
static void js_escape_lua_expr(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return;

    size_t di = 0;
    for (size_t si = 0; src[si] && di < dest_size - 2; si++) {
        char c = src[si];
        if (c == '\\' || c == '\'' || c == '\n' || c == '\r' || c == '\t') {
            if (di + 2 >= dest_size) break;
            dest[di++] = '\\';
            switch (c) {
                case '\n': dest[di++] = 'n'; break;
                case '\r': dest[di++] = 'r'; break;
                case '\t': dest[di++] = 't'; break;
                default: dest[di++] = c; break;
            }
        } else {
            dest[di++] = c;
        }
    }
    dest[di] = '\0';
}

// Recursively collect and output dynamic bindings from the component tree
static void output_dynamic_bindings_recursive(HTMLGenerator* generator, IRComponent* component) {
    if (!generator || !component) return;

    // Output dynamic bindings for this component
    uint32_t count = ir_component_get_dynamic_binding_count(component);
    for (uint32_t i = 0; i < count; i++) {
        IRDynamicBinding* binding = ir_component_get_dynamic_binding(component, i);
        if (!binding || !binding->lua_expr) continue;

        // Escape the Lua expression for JavaScript
        char escaped_expr[2048];
        js_escape_lua_expr(escaped_expr, binding->lua_expr, sizeof(escaped_expr));

        html_generator_write_format(generator,
            "    kryonRegisterDynamicBinding({ id: '%s', selector: '%s', updateType: '%s', luaExpr: '%s' });\n",
            binding->binding_id ? binding->binding_id : "unknown",
            binding->element_selector ? binding->element_selector : "",
            binding->update_type ? binding->update_type : "text",
            escaped_expr);
    }

    // Recurse into children
    for (uint32_t i = 0; i < component->child_count; i++) {
        output_dynamic_bindings_recursive(generator, component->children[i]);
    }
}

// Check if component tree has any dynamic bindings
static bool has_dynamic_bindings(IRComponent* component) {
    if (!component) return false;

    if (ir_component_get_dynamic_binding_count(component) > 0) {
        return true;
    }

    for (uint32_t i = 0; i < component->child_count; i++) {
        if (has_dynamic_bindings(component->children[i])) {
            return true;
        }
    }

    return false;
}

static bool generate_component_html(HTMLGenerator* generator, IRComponent* component) {
    if (!generator || !component) return false;

    // Debug: log to file immediately (no static state)
    FILE* debug_log = fopen("/tmp/html_gen_debug.log", "a");
    if (debug_log) {
        static int call_count = 0;
        fprintf(debug_log, "[html_gen] Call #%d: component type=%d (CODE_BLOCK=%d)\n",
                call_count++, component->type, IR_COMPONENT_CODE_BLOCK);
        fclose(debug_log);
    }

    // Check if a plugin has registered a web renderer for this component type
    bool has_plugin = ir_capability_has_web_renderer(component->type);
    if (component->type == IR_COMPONENT_CODE_BLOCK) {
        printf("[html_gen] CodeBlock: type=%d, has_plugin=%d\n", component->type, has_plugin);
        fflush(stdout);
    }
    if (has_plugin) {
        // Use capability-based plugin renderer
        // Create a data handle for the component
        KryonDataHandle handle = ir_capability_create_data_handle(
            component->custom_data,
            0,  // size - not needed for most components
            component->type,
            component->id
        );
        char* plugin_html = ir_capability_render_web(component->type, &handle, "light");
        if (component->type == IR_COMPONENT_CODE_BLOCK) {
            printf("[html_gen] plugin_html=%p\n", (void*)plugin_html);
            if (plugin_html) {
                printf("[html_gen] HTML: %.100s...\n", plugin_html);
            }
            fflush(stdout);
        }
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

    // Check if this is a Body component (uses HTML <body> tag)
    if (component->tag && strcmp(component->tag, "Body") == 0) {
        tag = "body";
    }

    // Check for semantic HTML tags stored in component->tag
    // These should be used for roundtrip HTML generation accuracy
    if (component->tag && component->type == IR_COMPONENT_CONTAINER) {
        if (strcmp(component->tag, "header") == 0 ||
            strcmp(component->tag, "footer") == 0 ||
            strcmp(component->tag, "section") == 0 ||
            strcmp(component->tag, "nav") == 0 ||
            strcmp(component->tag, "main") == 0 ||
            strcmp(component->tag, "article") == 0 ||
            strcmp(component->tag, "aside") == 0) {
            tag = component->tag;
        }
    }

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

    // Handle template placeholders ({{name}} syntax)
    // Outputs placeholder text directly for preserve mode or warns if not substituted
    if (component->type == IR_COMPONENT_PLACEHOLDER) {
        IRPlaceholderData* ph = (IRPlaceholderData*)component->custom_data;
        if (ph && ph->name) {
            if (ph->preserve) {
                // Preserve mode: output {{name}} syntax directly
                html_generator_write_format(generator, "{{%s}}", ph->name);
            } else {
                // Expand mode: placeholder should have been substituted before codegen
                fprintf(stderr, "Warning: Unresolved placeholder {{%s}} - should have been substituted\n", ph->name);
                // Output as comment for debugging
                html_generator_write_format(generator, "<!-- Unresolved: {{%s}} -->", ph->name);
            }
        }
        return true;
    }

    // Handle raw text output (no wrapper tag)
    // IR_COMPONENT_TEXT outputs raw text content without any HTML tag
    // UNLESS it has custom attributes (elementId, data-*) which require a wrapper
    if (tag == NULL) {
        // Check if text has custom attributes that require a wrapper span
        CustomAttrs text_custom_attrs = {0};
        bool has_custom_attrs = parse_custom_attrs(component->custom_data, &text_custom_attrs);
        bool needs_wrapper = has_custom_attrs && (text_custom_attrs.element_id || text_custom_attrs.data_attrs);

        if (needs_wrapper) {
            // Wrap text in span with custom attributes
            html_generator_write_string(generator, "<span");
            if (text_custom_attrs.element_id) {
                html_generator_write_format(generator, " id=\"%s\"", text_custom_attrs.element_id);
            }
            if (text_custom_attrs.data_attrs) {
                output_data_attrs(generator, text_custom_attrs.data_attrs);
            }
            html_generator_write_string(generator, ">");
        }

        // Output text content directly (escaped) WITHOUT indentation
        // Indentation would add unwanted whitespace before inline text
        if (component->text_content && strlen(component->text_content) > 0) {
            // Trim leading/trailing whitespace for inline text
            const char* start = component->text_content;
            const char* end = component->text_content + strlen(component->text_content) - 1;

            while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) start++;
            while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;

            if (end >= start) {
                size_t trimmed_len = end - start + 1;
                char trimmed[4096];
                if (trimmed_len < sizeof(trimmed)) {
                    strncpy(trimmed, start, trimmed_len);
                    trimmed[trimmed_len] = '\0';
                    char escaped_text[4096];
                    escape_html_text(trimmed, escaped_text, sizeof(escaped_text));
                    html_generator_write_string(generator, escaped_text);
                }
            }
            // Don't add newline for inline text - let parent handle formatting
        }
        // Process children (inline semantic components may have text children)
        for (uint32_t i = 0; i < component->child_count; i++) {
            generate_component_html(generator, component->children[i]);
        }

        // Close wrapper span if we opened one
        if (needs_wrapper) {
            html_generator_write_string(generator, "</span>");
        }

        cleanup_custom_attrs(&text_custom_attrs);
        return true;
    }

    bool is_self_closing = (component->type == IR_COMPONENT_INPUT ||
                          component->type == IR_COMPONENT_CHECKBOX ||
                          component->type == IR_COMPONENT_IMAGE ||
                          component->type == IR_COMPONENT_HORIZONTAL_RULE);

    // Skip indentation for inline elements to avoid whitespace between inline content
    if (!is_inline_component(component)) {
        html_generator_write_indent(generator);
    }
    html_generator_write_format(generator, "<%s", tag);

    // Analyze component styling to determine if ID/class needed
    StyleAnalysis* analysis = analyze_component_style(component, generator->logic_block);

    // Pre-detect if this button is inside a TabBar (making it a tab)
    // This affects ID/class generation below
    bool is_tab_button = false;
    int tab_button_index = 0;
    int tab_button_selected_index = 0;

    if (component->type == IR_COMPONENT_BUTTON &&
        component->parent && component->parent->type == IR_COMPONENT_TAB_BAR) {
        is_tab_button = true;

        // Find tab index by position in parent
        for (uint32_t i = 0; i < component->parent->child_count; i++) {
            if (component->parent->children[i] == component) {
                tab_button_index = (int)i;
                break;
            }
        }

        // Find selected index from TabGroup grandparent
        if (component->parent->parent &&
            component->parent->parent->type == IR_COMPONENT_TAB_GROUP &&
            component->parent->parent->custom_data) {
            tab_button_selected_index = atoi(component->parent->parent->custom_data);
        }
    }

    // Parse custom attributes from custom_data JSON (elementId, data-*)
    CustomAttrs custom_attrs = {0};
    parse_custom_attrs(component->custom_data, &custom_attrs);

    // Add ID: use custom elementId if present, otherwise use default ID generation
    // Skip for Modal and Tab components - they generate their own ID in the switch below
    // Also skip for tab-buttons - they get id="tab-N" in the switch below
    bool skip_default_id = (component->type == IR_COMPONENT_MODAL ||
                            component->type == IR_COMPONENT_TAB_GROUP ||
                            component->type == IR_COMPONENT_TAB_BAR ||
                            component->type == IR_COMPONENT_TAB ||
                            component->type == IR_COMPONENT_TAB_CONTENT ||
                            component->type == IR_COMPONENT_TAB_PANEL ||
                            is_tab_button);

    if (custom_attrs.element_id) {
        // Use custom element ID
        html_generator_write_format(generator, " id=\"%s\"", custom_attrs.element_id);
    } else if (analysis && analysis->needs_id && !skip_default_id) {
        // Use default ID generation
        const char* prefix = get_id_prefix(component->type);
        html_generator_write_format(generator, " id=\"%s-%u\"", prefix, component->id);
    }

    // Always add data-kryon-id for reactive binding lookups
    // This allows the JS reactive system to find any component by ID
    html_generator_write_format(generator, " data-kryon-id=\"%u\"", component->id);

    // Output data attributes if present
    if (custom_attrs.data_attrs) {
        output_data_attrs(generator, custom_attrs.data_attrs);
    }

    // NOTE: Don't cleanup custom_attrs here - we need custom_attrs.action
    // for event handler generation later. Cleanup happens before returns.

    // ONLY add class if component has EXPLICIT css_class set
    // This allows descendant selectors like ".logo span" to work correctly
    // Skip for Modal and Tab components - they generate their own class in the switch below
    // Also skip tab-buttons - they use role="tab" styling, not class="button"
    if (component->type == IR_COMPONENT_MODAL ||
        component->type == IR_COMPONENT_TAB_GROUP ||
        component->type == IR_COMPONENT_TAB_BAR ||
        component->type == IR_COMPONENT_TAB_CONTENT ||
        is_tab_button) {
        // These components handle their own class generation
    } else if (component->css_class && component->css_class[0] != '\0') {
        html_generator_write_format(generator, " class=\"%s\"", component->css_class);
    } else if (component->type == IR_COMPONENT_ROW) {
        // Row always needs class="row" for display:flex layout
        html_generator_write_string(generator, " class=\"row\"");
    } else if (component->type == IR_COMPONENT_COLUMN) {
        // Column always needs class="column" for display:flex layout
        html_generator_write_string(generator, " class=\"column\"");
    } else if (analysis && analysis->needs_css && analysis->suggested_class) {
        // Fallback: add class for components with custom styling but no explicit class
        // Skip elements that CSS can target directly (semantic elements or element selectors)
        bool skip_class = false;

        // If selector_type is ELEMENT, CSS can target tag directly (e.g., header, footer, nav)
        if (component->selector_type == IR_SELECTOR_ELEMENT) {
            skip_class = true;
        } else {
            // Also skip specific component types with semantic HTML tags
            switch (component->type) {
                // Inline elements - skip to preserve descendant selectors
                case IR_COMPONENT_SPAN:
                case IR_COMPONENT_STRONG:
                case IR_COMPONENT_EM:
                case IR_COMPONENT_CODE_INLINE:
                case IR_COMPONENT_MARK:
                case IR_COMPONENT_LINK:
                // Semantic block elements - CSS can target h1, h2, p, etc. directly
                case IR_COMPONENT_HEADING:
                case IR_COMPONENT_PARAGRAPH:
                case IR_COMPONENT_LIST:
                case IR_COMPONENT_LIST_ITEM:
                case IR_COMPONENT_BLOCKQUOTE:
                case IR_COMPONENT_CODE_BLOCK:
                    skip_class = true;
                    break;
                default:
                    break;
            }
        }
        if (!skip_class) {
            html_generator_write_format(generator, " class=\"%s\"", analysis->suggested_class);
        }
    }

    // Add inline CSS variable styles
    generate_css_var_styles(generator, component);

    // Component-specific attributes
    switch (component->type) {
        case IR_COMPONENT_BUTTON: {
            // Tab-button detection was done above (is_tab_button, tab_button_index, tab_button_selected_index)
            if (is_tab_button) {
                // Generate tab-specific ID and attributes
                html_generator_write_format(generator, " id=\"tab-%u\"", component->id);

                bool is_selected = (tab_button_index == tab_button_selected_index);
                html_generator_write_string(generator, " role=\"tab\"");
                html_generator_write_format(generator, " data-tab=\"%d\"", tab_button_index);
                html_generator_write_format(generator, " aria-selected=\"%s\"", is_selected ? "true" : "false");

                // Generate native tab switching onclick
                html_generator_write_format(generator, " onclick=\"kryonSelectTab(this.closest('.kryon-tabs'), %d)\"", tab_button_index);

                // Store action name as data attribute (kryonSelectTab will dispatch it)
                if (custom_attrs.action) {
                    html_generator_write_format(generator, " data-action=\"%s\"", custom_attrs.action);
                }
            }

            // Add data-text for all buttons (tabs and regular)
            if (component->text_content) {
                char escaped_text[1024];
                escape_html_text(component->text_content, escaped_text, sizeof(escaped_text));
                html_generator_write_format(generator, " data-text=\"%s\"", escaped_text);
            }
            break;
        }

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
            if (data && data->id) {
                // Only add id attribute if explicitly set (e.g., for anchor links)
                // Level is already encoded in the h1/h2/h3 tag itself
                html_generator_write_format(generator, " id=\"%s\"", data->id);
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
                if (data->target) {
                    char escaped_target[128];
                    escape_html_text(data->target, escaped_target, sizeof(escaped_target));
                    html_generator_write_format(generator, " target=\"%s\"", escaped_target);
                }
                if (data->rel) {
                    char escaped_rel[256];
                    escape_html_text(data->rel, escaped_rel, sizeof(escaped_rel));
                    html_generator_write_format(generator, " rel=\"%s\"", escaped_rel);
                }
                if (data->title) {
                    char escaped_title[1024];
                    escape_html_text(data->title, escaped_title, sizeof(escaped_title));
                    html_generator_write_format(generator, " title=\"%s\"", escaped_title);
                }
            }
            break;
        }

        case IR_COMPONENT_MODAL: {
            // Modal state stored as "open|title" or "closed|title" in custom_data
            bool is_open = false;
            const char* title = NULL;

            if (component->custom_data) {
                const char* state_str = component->custom_data;
                is_open = (strncmp(state_str, "open", 4) == 0);
                const char* pipe = strchr(state_str, '|');
                if (pipe && pipe[1] != '\0') {
                    title = pipe + 1;
                }
            }

            // Add modal ID and class
            html_generator_write_format(generator, " id=\"modal-%u\"", component->id);
            html_generator_write_string(generator, " class=\"kryon-modal\"");
            html_generator_write_format(generator, " data-modal-open=\"%s\"", is_open ? "true" : "false");

            if (title) {
                char escaped_title[256];
                escape_html_text(title, escaped_title, sizeof(escaped_title));
                html_generator_write_format(generator, " data-modal-title=\"%s\"", escaped_title);
            }
            break;
        }

        // Tab components with ARIA roles and native switching
        case IR_COMPONENT_TAB_GROUP: {
            // Find selected tab index from props or default to 0
            int selected = 0;
            if (component->custom_data) {
                selected = atoi(component->custom_data);
            }
            html_generator_write_format(generator, " id=\"tabgroup-%u\"", component->id);
            html_generator_write_string(generator, " class=\"kryon-tabs\"");
            html_generator_write_format(generator, " data-selected=\"%d\"", selected);
            break;
        }

        case IR_COMPONENT_TAB_BAR: {
            html_generator_write_format(generator, " id=\"tabbar-%u\"", component->id);
            html_generator_write_string(generator, " class=\"kryon-tab-bar\" role=\"tablist\"");
            break;
        }

        case IR_COMPONENT_TAB: {
            // Find tab index by looking at position in parent
            int tab_index = 0;
            bool is_selected = false;
            if (component->parent) {
                for (uint32_t i = 0; i < component->parent->child_count; i++) {
                    if (component->parent->children[i] == component) {
                        tab_index = i;
                        break;
                    }
                }
                // Check if this tab is selected by looking at grandparent's data
                if (component->parent->parent && component->parent->parent->custom_data) {
                    int selected = atoi(component->parent->parent->custom_data);
                    is_selected = (tab_index == selected);
                }
            }
            html_generator_write_format(generator, " id=\"tab-%u\"", component->id);
            html_generator_write_string(generator, " role=\"tab\"");
            html_generator_write_format(generator, " data-tab=\"%d\"", tab_index);
            html_generator_write_format(generator, " aria-selected=\"%s\"", is_selected ? "true" : "false");
            html_generator_write_format(generator, " onclick=\"kryonSelectTab(this.closest('.kryon-tabs'), %d)\"", tab_index);
            // Store handler ID for Lua callback if there's a click event
            if (component->events) {
                IREvent* event = component->events;
                while (event) {
                    if (event->type == IR_EVENT_CLICK && event->logic_id) {
                        if (strncmp(event->logic_id, "lua_event_", 10) == 0) {
                            int handler_id = atoi(event->logic_id + 10);
                            html_generator_write_format(generator, " data-handler-id=\"%d\"", handler_id);
                        }
                    }
                    event = event->next;
                }
            }
            break;
        }

        case IR_COMPONENT_TAB_CONTENT: {
            html_generator_write_format(generator, " id=\"tabcontent-%u\"", component->id);
            html_generator_write_string(generator, " class=\"kryon-tab-content\"");
            break;
        }

        case IR_COMPONENT_TAB_PANEL: {
            // Find panel index by looking at position in parent
            int panel_index = 0;
            int selected = 0;  // Default to first panel visible
            if (component->parent) {
                for (uint32_t i = 0; i < component->parent->child_count; i++) {
                    if (component->parent->children[i] == component) {
                        panel_index = i;
                        break;
                    }
                }
                // Check if this panel should be visible by looking at grandparent's data
                if (component->parent->parent && component->parent->parent->custom_data) {
                    selected = atoi(component->parent->parent->custom_data);
                }
            }
            bool is_active = (panel_index == selected);
            html_generator_write_format(generator, " id=\"tabpanel-%u\"", component->id);
            html_generator_write_string(generator, " role=\"tabpanel\"");
            html_generator_write_format(generator, " data-panel=\"%d\"", panel_index);
            if (!is_active) {
                html_generator_write_string(generator, " hidden");
            }
            break;
        }

        case IR_COMPONENT_FOR_EACH: {
            // ForEach: dynamic list rendering with runtime reactivity
            html_generator_write_format(generator, " id=\"foreach-%u\"", component->id);
            html_generator_write_string(generator, " class=\"kryon-forEach\"");

            // Add ForEach-specific data attributes for runtime re-rendering
            if (component->each_source && component->each_source[0] != '\0') {
                char escaped_source[512];
                escape_html_text(component->each_source, escaped_source, sizeof(escaped_source));
                html_generator_write_format(generator, " data-each-source=\"%s\"", escaped_source);
            }
            if (component->each_item_name && component->each_item_name[0] != '\0') {
                char escaped_item[256];
                escape_html_text(component->each_item_name, escaped_item, sizeof(escaped_item));
                html_generator_write_format(generator, " data-item-name=\"%s\"", escaped_item);
            }
            if (component->each_index_name && component->each_index_name[0] != '\0') {
                char escaped_index[256];
                escape_html_text(component->each_index_name, escaped_index, sizeof(escaped_index));
                html_generator_write_format(generator, " data-index-name=\"%s\"", escaped_index);
            }
            break;
        }

        case IR_COMPONENT_FLOWCHART: {
            // Flowchart plugin will handle rendering via plugin system
            // No special attributes needed for now
            break;
        }

        default:
            break;
    }

    // Add event handlers from logic_block (if available)
    if (generator->logic_block) {
        // Lookup handlers from logic_block and generate inline event attributes
        const char* event_types[] = {"click", "change", "input", "focus", "blur", "mouseenter", "mouseleave", "keydown", "keyup"};
        const char* html_events[] = {"onclick", "onchange", "oninput", "onfocus", "onblur", "onmouseenter", "onmouseleave", "onkeydown", "onkeyup"};

        for (size_t i = 0; i < sizeof(event_types) / sizeof(event_types[0]); i++) {
            const char* handler_name = ir_logic_block_get_handler(generator->logic_block, component->id, event_types[i]);
            if (handler_name) {
                IRLogicFunction* func = ir_logic_block_find_function(generator->logic_block, handler_name);
                if (func && func->source_count > 0) {
                    // Get JavaScript source (prefer javascript, fallback to first available)
                    const char* source = NULL;
                    for (int j = 0; j < func->source_count; j++) {
                        if (strcmp(func->sources[j].language, "javascript") == 0) {
                            source = func->sources[j].source;
                            break;
                        }
                    }
                    if (!source && func->source_count > 0) {
                        source = func->sources[0].source;
                    }

                    if (source) {
                        // Escape quotes in handler source
                        char escaped_source[1024];
                        const char* src = source;
                        char* dst = escaped_source;
                        size_t remaining = sizeof(escaped_source) - 2;
                        while (*src && remaining > 6) {
                            if (*src == '"') {
                                memcpy(dst, "&quot;", 6);
                                dst += 6;
                                remaining -= 6;
                                src++;
                            } else if (*src == '\'') {
                                memcpy(dst, "&#x27;", 6);
                                dst += 6;
                                remaining -= 6;
                                src++;
                            } else {
                                *dst++ = *src++;
                                remaining--;
                            }
                        }
                        *dst = '\0';

                        html_generator_write_format(generator, " %s=\"%s\"", html_events[i], escaped_source);
                    }
                }
            }
        }
    }

    // Generate event handlers from component's events array (for Lua events)
    // This handles lua_event_N style handlers from Lua source files
    // NOTE: Tab-buttons already have onclick generated above (kryonSelectTab + data-handler-id)
    if (component->events) {
        IREvent* event = component->events;
        while (event) {
            if (event->logic_id && strncmp(event->logic_id, "lua_event_", 10) == 0) {
                // Map IR event type to HTML event attribute
                const char* html_event = NULL;
                switch (event->type) {
                    case IR_EVENT_CLICK: html_event = "onclick"; break;
                    case IR_EVENT_HOVER: html_event = "onmouseenter"; break;
                    case IR_EVENT_FOCUS: html_event = "onfocus"; break;
                    case IR_EVENT_BLUR: html_event = "onblur"; break;
                    case IR_EVENT_TEXT_CHANGE: html_event = "oninput"; break;
                    default: break;
                }

                if (html_event) {
                    // Skip onclick for tab-buttons - they use kryonSelectTab which calls handler via data-handler-id
                    if (is_tab_button && event->type == IR_EVENT_CLICK) {
                        // Already handled above with kryonSelectTab + data-handler-id
                        event = event->next;
                        continue;
                    }

                    // Handler source-based dispatch using embedded Lua code from KIR
                    // The handler source is extracted at compile time and stored in the event
                    if (event->handler_source && event->handler_source->code) {
                        // Collect handler for Lua registry generation
                        collect_handler(generator, component->id, event->handler_source->code);

                        // Generate onclick to call handler by component ID
                        // Pass event object so handlers can access event.target for data attributes
                        html_generator_write_format(generator,
                            " %s=\"kryonCallHandler(%u, event)\"", html_event, component->id);
                    }
                }
            }
            event = event->next;
        }
    }

    if (is_self_closing) {
        html_generator_write_string(generator, " />\n");
        cleanup_custom_attrs(&custom_attrs);
        style_analysis_free(analysis);
        return true;
    }

    // Check if content should be output inline (no newlines between tag and content)
    bool inline_content = has_only_inline_content(component);

    if (inline_content) {
        // Inline formatting: <tag>content</tag>
        html_generator_write_string(generator, ">");

        // Write text content inline (trimmed)
        if (component->text_content && strlen(component->text_content) > 0) {
            // Trim leading/trailing whitespace for inline text
            const char* start = component->text_content;
            const char* end = component->text_content + strlen(component->text_content) - 1;

            while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) start++;
            while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;

            if (end >= start) {
                size_t trimmed_len = end - start + 1;
                char trimmed[1024];
                if (trimmed_len < sizeof(trimmed)) {
                    strncpy(trimmed, start, trimmed_len);
                    trimmed[trimmed_len] = '\0';
                    char escaped_text[1024];
                    escape_html_text(trimmed, escaped_text, sizeof(escaped_text));
                    html_generator_write_string(generator, escaped_text);
                }
            }
        }

        // Write inline children
        for (uint32_t i = 0; i < component->child_count; i++) {
            generate_component_html(generator, component->children[i]);
        }

        // For inline elements, don't add newline after closing tag
        // For block elements with inline content, add newline
        if (is_inline_component(component)) {
            html_generator_write_format(generator, "</%s>", tag);
        } else {
            html_generator_write_format(generator, "</%s>\n", tag);
        }
    } else {
        // Block formatting: newlines between tag and content
        html_generator_write_string(generator, ">\n");

        generator->indent_level++;

        // Special handling for Modal - generate title bar and content wrapper
        if (component->type == IR_COMPONENT_MODAL) {
            // Parse modal state for title
            const char* title = NULL;
            if (component->custom_data) {
                const char* pipe = strchr(component->custom_data, '|');
                if (pipe && pipe[1] != '\0') {
                    title = pipe + 1;
                }
            }

            // Generate title bar if title exists
            if (title) {
                html_generator_write_indent(generator);
                html_generator_write_string(generator, "<div class=\"modal-title-bar\">\n");
                generator->indent_level++;

                html_generator_write_indent(generator);
                html_generator_write_format(generator, "<span class=\"modal-title\">%s</span>\n", title);

                html_generator_write_indent(generator);
                html_generator_write_string(generator, "<button class=\"modal-close\" onclick=\"this.closest('dialog').close()\">&times;</button>\n");

                generator->indent_level--;
                html_generator_write_indent(generator);
                html_generator_write_string(generator, "</div>\n");
            }

            // Wrap children in modal-content div
            html_generator_write_indent(generator);
            html_generator_write_string(generator, "<div class=\"modal-content\">\n");
            generator->indent_level++;

            // Write children
            for (uint32_t i = 0; i < component->child_count; i++) {
                generate_component_html(generator, component->children[i]);
            }

            generator->indent_level--;
            html_generator_write_indent(generator);
            html_generator_write_string(generator, "</div>\n");

            generator->indent_level--;
            html_generator_write_indent(generator);
            html_generator_write_format(generator, "</%s>\n", tag);

            cleanup_custom_attrs(&custom_attrs);
            style_analysis_free(analysis);
            return true;
        }

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
            // Check if a plugin has registered a web renderer for code blocks
            printf("[html_gen] CODE_BLOCK component, checking for plugin renderer (type=%d)...\n", IR_COMPONENT_CODE_BLOCK);
            bool has_renderer = ir_capability_has_web_renderer(IR_COMPONENT_CODE_BLOCK);
            printf("[html_gen] has_renderer=%d\n", has_renderer);

            if (has_renderer) {
                // Use capability-based plugin renderer for syntax highlighting
                html_generator_write_indent(generator);
                printf("[html_gen] Calling plugin renderer...\n");
                KryonDataHandle handle = ir_capability_create_data_handle(
                    component->custom_data,
                    0,
                    component->type,
                    component->id
                );
                char* rendered_html = ir_capability_render_web(component->type, &handle, "dark");
                printf("[html_gen] Renderer returned: %p\n", (void*)rendered_html);
                if (rendered_html) {
                    printf("[html_gen] HTML snippet: %.100s...\n", rendered_html);
                    html_generator_write_string(generator, rendered_html);
                    html_generator_write_string(generator, "\n");
                    free(rendered_html);
                } else {
                    // Fallback to plain code if renderer fails
                    printf("[html_gen] Renderer returned NULL, using fallback\n");
                    IRCodeBlockData* data = (IRCodeBlockData*)component->custom_data;
                    if (data && data->code) {
                        html_generator_write_indent(generator);
                        html_generator_write_string(generator, "<code");
                        if (data->language) {
                            html_generator_write_format(generator, " class=\"language-%s\"", data->language);
                        }
                        html_generator_write_string(generator, ">");

                        size_t code_len = data->length ? data->length : strlen(data->code);
                        char* escaped_code = malloc(code_len * 6 + 1);
                        if (escaped_code) {
                            escape_html_text_len(data->code, code_len, escaped_code, code_len * 6 + 1);
                            html_generator_write_string(generator, escaped_code);
                            free(escaped_code);
                        }

                        html_generator_write_string(generator, "</code>\n");
                    }
                }
            } else {
                // No plugin renderer - use plain code block
                IRCodeBlockData* data = (IRCodeBlockData*)component->custom_data;
                if (data && data->code) {
                    html_generator_write_indent(generator);
                    html_generator_write_string(generator, "<code");
                    if (data->language) {
                        html_generator_write_format(generator, " class=\"language-%s\"", data->language);
                    }
                    html_generator_write_string(generator, ">");

                    size_t code_len = data->length ? data->length : strlen(data->code);
                    char* escaped_code = malloc(code_len * 6 + 1);
                    if (escaped_code) {
                        escape_html_text_len(data->code, code_len, escaped_code, code_len * 6 + 1);
                        html_generator_write_string(generator, escaped_code);
                        free(escaped_code);
                    }

                    html_generator_write_string(generator, "</code>\n");
                }
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
    }

    cleanup_custom_attrs(&custom_attrs);
    style_analysis_free(analysis);
    return true;
}

const char* html_generator_generate(HTMLGenerator* generator, IRComponent* root) {
    // Debug: write directly to file using syscalls
    int fd = open("/tmp/html_gen_generate.log", O_WRONLY|O_CREAT|O_APPEND, 0644);
    if (fd >= 0) {
        const char msg[] = "html_generator_generate called!\n";
        ssize_t ret __attribute__((unused)) = write(fd, msg, sizeof(msg)-1);
        (void)ret;
        close(fd);
    }

    if (!generator || !root) return NULL;

    // Clear buffer
    generator->buffer_size = 0;
    if (generator->output_buffer) {
        generator->output_buffer[0] = '\0';
    }

    // Reset handler collection for fresh generation
    if (generator->handlers) {
        for (size_t i = 0; i < generator->handler_count; i++) {
            if (generator->handlers[i].code) {
                free(generator->handlers[i].code);
            }
        }
    }
    generator->handler_count = 0;

    // Generate HTML document
    html_generator_write_string(generator, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
    html_generator_write_string(generator, "  <meta charset=\"UTF-8\">\n");
    html_generator_write_string(generator, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    // Use metadata title if available, otherwise use default
    const char* title = (g_ir_context && g_ir_context->metadata && g_ir_context->metadata->window_title)
                       ? g_ir_context->metadata->window_title
                       : "Kryon Web Application";
    html_generator_write_format(generator, "  <title>%s</title>\n", title);

    // Add CSS - either embedded <style> tag or external <link>
    if (generator->options.embedded_css) {
        // Generate CSS in embedded <style> block
        CSSGenerator* css_gen = css_generator_create();
        if (css_gen) {
            // Pass manifest for CSS variable support
            if (generator->manifest) {
                css_generator_set_manifest(css_gen, generator->manifest);
            }
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

    // Add Fengari Lua VM for Lua event execution (local file, no CDN)
    // Only include if the source language is Lua
    bool needs_fengari = false;
    if (generator->metadata && generator->metadata->source_language) {
        needs_fengari = (strcmp(generator->metadata->source_language, "lua") == 0);
    }

    if (needs_fengari) {
        html_generator_write_string(generator, "  <script src=\"fengari-web.min.js\"></script>\n");
    }

    // Add JavaScript runtime if requested
    if (generator->options.include_runtime) {
        html_generator_write_string(generator, "  <script src=\"kryon.js\"></script>\n");
        // Add reactive system (Phase 2: Self-Contained KIR)
        html_generator_write_string(generator, "  <script src=\"kryon-reactive.js\"></script>\n");
    }

    html_generator_write_string(generator, "</head>\n");

    // Check if root or its first child is a Body component
    IRComponent* body_component = NULL;
    bool root_is_body = (root->tag && strcmp(root->tag, "Body") == 0);

    if (root_is_body) {
        body_component = root;
    } else if (root->child_count > 0 && root->children && root->children[0] && root->children[0]->tag &&
               strcmp(root->children[0]->tag, "Body") == 0) {
        body_component = root->children[0];
    }

    if (body_component) {
        // Render Body component as actual <body> element
        html_generator_write_string(generator, "<body>\n");

        generator->indent_level = 1;
        // Render Body's children
        for (uint32_t i = 0; i < body_component->child_count; i++) {
            generate_component_html(generator, body_component->children[i]);
        }
        generator->indent_level = 0;
    } else {
        // Normal case: create wrapper body and render component tree
        html_generator_write_string(generator, "<body>\n");
        generator->indent_level = 1;
        generate_component_html(generator, root);
        generator->indent_level = 0;
    }

    // NOTE: Handler registry is now generated by lua_bundler and appended to the
    // bundled Lua code. This ensures handlers share the same scope as app functions.
    // The html_generator still collects handlers (for onclick attributes) but doesn't
    // generate the registry script anymore.

    // Inject initial state from manifest (Phase 2.2)
    if (generator->manifest && generator->manifest->variable_count > 0) {
        html_generator_write_string(generator, "  <script>\n");
        html_generator_write_string(generator, "  // Initial reactive state from IRReactiveManifest\n");
        html_generator_write_string(generator, "  window.__KRYON_STATE__ = {\n");
        for (uint32_t i = 0; i < generator->manifest->variable_count; i++) {
            IRReactiveVarDescriptor* var = &generator->manifest->variables[i];
            if (!var->name) continue;
            const char* comma = (i < generator->manifest->variable_count - 1) ? "," : "";
            if (var->initial_value_json && strlen(var->initial_value_json) > 0) {
                html_generator_write_format(generator, "    %s: %s%s\n",
                    var->name, var->initial_value_json, comma);
            } else {
                switch (var->type) {
                    case IR_REACTIVE_TYPE_INT:
                        html_generator_write_format(generator, "    %s: %ld%s\n",
                            var->name, (long)var->value.as_int, comma);
                        break;
                    case IR_REACTIVE_TYPE_FLOAT:
                        html_generator_write_format(generator, "    %s: %f%s\n",
                            var->name, var->value.as_float, comma);
                        break;
                    case IR_REACTIVE_TYPE_STRING:
                        html_generator_write_format(generator, "    %s: \"%s\"%s\n",
                            var->name, var->value.as_string ? var->value.as_string : "", comma);
                        break;
                    case IR_REACTIVE_TYPE_BOOL:
                        html_generator_write_format(generator, "    %s: %s%s\n",
                            var->name, var->value.as_bool ? "true" : "false", comma);
                        break;
                    default:
                        html_generator_write_format(generator, "    %s: null%s\n", var->name, comma);
                        break;
                }
            }
        }
        html_generator_write_string(generator, "  };\n");
        html_generator_write_string(generator, "  console.log('[Kryon] Initial state loaded:', Object.keys(window.__KRYON_STATE__).length, 'variables');\n");
        html_generator_write_string(generator, "  </script>\n");
    }

    // Inject reactive bindings from manifest (Phase 3: Generic Reactive Binding Support)
    // This allows the JS runtime to automatically update DOM elements when state changes
    if (generator->manifest && generator->manifest->binding_count > 0) {
        html_generator_write_string(generator, "  <script>\n");
        html_generator_write_string(generator, "  // Reactive bindings from IRReactiveManifest\n");
        html_generator_write_string(generator, "  (function() {\n");
        html_generator_write_string(generator, "    const bindings = [\n");

        for (uint32_t i = 0; i < generator->manifest->binding_count; i++) {
            IRReactiveBinding* binding = &generator->manifest->bindings[i];

            // Get the variable name from the expression (it contains the state path)
            const char* stateKey = binding->expression ? binding->expression : "";

            // Parse property name from path if it's a property binding (format: "path:propName")
            char stateKeyBuf[256] = "";
            char propName[64] = "";
            const char* colonPos = stateKey ? strchr(stateKey, ':') : NULL;
            if (colonPos) {
                // Extract state key (before colon) and property name (after colon)
                size_t keyLen = colonPos - stateKey;
                if (keyLen < sizeof(stateKeyBuf)) {
                    strncpy(stateKeyBuf, stateKey, keyLen);
                    stateKeyBuf[keyLen] = '\0';
                    strncpy(propName, colonPos + 1, sizeof(propName) - 1);
                    propName[sizeof(propName) - 1] = '\0';
                }
            } else if (stateKey) {
                strncpy(stateKeyBuf, stateKey, sizeof(stateKeyBuf) - 1);
                stateKeyBuf[sizeof(stateKeyBuf) - 1] = '\0';
            }

            // Map binding type to JavaScript binding type string
            const char* bindingType = "text";
            switch (binding->binding_type) {
                case IR_BINDING_TEXT: bindingType = "text"; break;
                case IR_BINDING_CONDITIONAL: bindingType = "visibility"; break;
                case IR_BINDING_ATTRIBUTE:
                    // If we have a property name, use "property", otherwise "attribute"
                    bindingType = propName[0] ? "property" : "attribute";
                    break;
                default: bindingType = "text"; break;
            }

            // Output binding registration
            const char* comma = (i < generator->manifest->binding_count - 1) ? "," : "";
            if (propName[0]) {
                html_generator_write_format(generator,
                    "      { stateKey: '%s', componentId: %u, type: '%s', prop: '%s' }%s\n",
                    stateKeyBuf, binding->component_id, bindingType, propName, comma);
            } else {
                html_generator_write_format(generator,
                    "      { stateKey: '%s', componentId: %u, type: '%s' }%s\n",
                    stateKeyBuf[0] ? stateKeyBuf : stateKey, binding->component_id, bindingType, comma);
            }
        }

        html_generator_write_string(generator, "    ];\n");
        html_generator_write_string(generator, "    // Register bindings with Kryon reactive system\n");
        html_generator_write_string(generator, "    bindings.forEach(function(b) {\n");
        html_generator_write_string(generator, "      var selector = '[data-kryon-id=\"' + b.componentId + '\"]';\n");
        html_generator_write_string(generator, "      var opts = b.prop ? { prop: b.prop } : {};\n");
        html_generator_write_string(generator, "      if (window.kryonRegisterBinding) {\n");
        html_generator_write_string(generator, "        window.kryonRegisterBinding(b.stateKey, selector, b.type, opts);\n");
        html_generator_write_string(generator, "      }\n");
        html_generator_write_string(generator, "    });\n");
        html_generator_write_string(generator, "    console.log('[Kryon] Registered', bindings.length, 'reactive bindings');\n");
        html_generator_write_string(generator, "  })();\n");
        html_generator_write_string(generator, "  </script>\n");
    }

    // Inject dynamic bindings from component tree (Phase 5: Runtime Lua Expression Evaluation)
    // These bindings have Lua expressions that are re-evaluated when state changes
    if (has_dynamic_bindings(root)) {
        html_generator_write_string(generator, "  <script>\n");
        html_generator_write_string(generator, "  // Dynamic bindings (Lua expressions evaluated at runtime)\n");
        html_generator_write_string(generator, "  (function() {\n");
        output_dynamic_bindings_recursive(generator, root);
        html_generator_write_string(generator, "    console.log('[Kryon] Registered dynamic bindings');\n");
        html_generator_write_string(generator, "  })();\n");
        html_generator_write_string(generator, "  </script>\n");
    }

    // Inject hot reload script in dev mode
    const char* dev_mode = getenv("KRYON_DEV_MODE");
    const char* ws_port_str = getenv("KRYON_WS_PORT");
    if (dev_mode && strcmp(dev_mode, "1") == 0 && ws_port_str) {
        html_generator_write_string(generator, "  <script>\n");
        html_generator_write_string(generator, "  (function() {\n");
        html_generator_write_format(generator, "    const ws = new WebSocket('ws://localhost:%s');\n", ws_port_str);
        html_generator_write_string(generator, "    ws.onmessage = function() {\n");
        html_generator_write_string(generator, "      console.log('[Kryon] Reloading...');\n");
        html_generator_write_string(generator, "      location.reload();\n");
        html_generator_write_string(generator, "    };\n");
        html_generator_write_string(generator, "    ws.onerror = function() {\n");
        html_generator_write_string(generator, "      console.log('[Kryon] WebSocket error, retrying...');\n");
        html_generator_write_string(generator, "      setTimeout(function() { location.reload(); }, 1000);\n");
        html_generator_write_string(generator, "    };\n");
        html_generator_write_string(generator, "    console.log('[Kryon] Hot reload enabled');\n");
        html_generator_write_string(generator, "  })();\n");
        html_generator_write_string(generator, "  </script>\n");
    }

    html_generator_write_string(generator, "</body>\n</html>\n");

    return generator->output_buffer;
}

const char* html_generator_generate_with_logic(HTMLGenerator* generator, IRComponent* root, IRLogicBlock* logic_block) {
    if (!generator || !root) return NULL;

    // Set logic_block in generator context
    generator->logic_block = logic_block;

    // Use the main generate function
    const char* result = html_generator_generate(generator, root);

    // Clear logic_block after generation
    generator->logic_block = NULL;

    return result;
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