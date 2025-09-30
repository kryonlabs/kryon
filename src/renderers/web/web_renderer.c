/**
 * @file web_renderer.c
 * @brief Web Renderer Implementation
 *
 * Implements Kryon renderer interface for HTML/CSS/JS output
 */

#include "web_renderer.h"
#include "css_generator.h"
#include "dom_manager.h"
#include "runtime.h"
#include "elements.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// BUFFER MANAGEMENT
// =============================================================================

#define INITIAL_BUFFER_SIZE 4096

static bool ensure_buffer_capacity(char** buffer, size_t* capacity, size_t* size, size_t additional) {
    if (*size + additional >= *capacity) {
        size_t new_capacity = (*capacity) * 2;
        while (new_capacity < *size + additional) {
            new_capacity *= 2;
        }
        char* new_buffer = realloc(*buffer, new_capacity);
        if (!new_buffer) {
            return false;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }
    return true;
}

static bool append_to_buffer(char** buffer, size_t* capacity, size_t* size, const char* data) {
    size_t len = strlen(data);
    if (!ensure_buffer_capacity(buffer, capacity, size, len + 1)) {
        return false;
    }
    memcpy(*buffer + *size, data, len);
    *size += len;
    (*buffer)[*size] = '\0';
    return true;
}

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonRenderResult web_initialize(void* surface);
static KryonRenderResult web_begin_frame(KryonRenderContext** context, KryonColor clear_color);
static KryonRenderResult web_end_frame(KryonRenderContext* context);
static KryonRenderResult web_execute_commands(KryonRenderContext* context,
                                             const KryonRenderCommand* commands,
                                             size_t command_count);
static KryonRenderResult web_resize(KryonVec2 new_size);
static KryonVec2 web_viewport_size(void);
static void web_destroy(void);
static bool web_point_in_element(KryonVec2 point, KryonRect element_bounds);
static bool web_handle_event(const KryonEvent* event);
static void* web_get_native_window(void);
static float web_measure_text_width(const char* text, float font_size);
static KryonRenderResult web_set_cursor(KryonCursorType cursor_type);
static KryonRenderResult web_update_window_size(int width, int height);
static KryonRenderResult web_update_window_title(const char* title);

// =============================================================================
// GLOBAL STATE
// =============================================================================

static WebRendererImpl g_web_impl = {0};

static KryonRendererVTable g_web_vtable = {
    .initialize = web_initialize,
    .begin_frame = web_begin_frame,
    .end_frame = web_end_frame,
    .execute_commands = web_execute_commands,
    .resize = web_resize,
    .viewport_size = web_viewport_size,
    .destroy = web_destroy,
    .point_in_element = web_point_in_element,
    .handle_event = web_handle_event,
    .get_native_window = web_get_native_window,
    .measure_text_width = web_measure_text_width,
    .set_cursor = web_set_cursor,
    .update_window_size = web_update_window_size,
    .update_window_title = web_update_window_title
};

// =============================================================================
// PUBLIC API
// =============================================================================

WebRendererConfig kryon_web_renderer_default_config(void) {
    WebRendererConfig config = {0};
    config.output_dir = strdup("./web_output");
    config.title = strdup("Kryon Application");
    config.inline_styles = false;
    config.inline_scripts = false;
    config.include_fengari = true;
    config.minify = false;
    config.standalone = false;
    return config;
}

KryonRenderer* kryon_web_renderer_create(const KryonRendererConfig* config) {
    if (!config) {
        return NULL;
    }

    KryonRenderer* renderer = malloc(sizeof(KryonRenderer));
    if (!renderer) {
        return NULL;
    }

    // Initialize web impl with default config
    g_web_impl.config = kryon_web_renderer_default_config();

    // Override output directory if provided via platform_context
    if (config->platform_context) {
        const char* output_dir = (const char*)config->platform_context;
        if (g_web_impl.config.output_dir) {
            free(g_web_impl.config.output_dir);
        }
        g_web_impl.config.output_dir = strdup(output_dir);
    }

    g_web_impl.html_buffer = NULL;
    g_web_impl.css_buffer = NULL;
    g_web_impl.js_buffer = NULL;
    g_web_impl.element_id_counter = 0;

    renderer->vtable = &g_web_vtable;
    renderer->impl_data = &g_web_impl;
    renderer->name = strdup("Web Renderer");
    renderer->backend = strdup("html");

    return renderer;
}

KryonRenderer* kryon_web_renderer_create_with_config(const WebRendererConfig* web_config) {
    KryonRenderer* renderer = kryon_web_renderer_create(NULL);
    if (!renderer) {
        return NULL;
    }

    // Override default config with provided config
    if (web_config) {
        if (g_web_impl.config.output_dir) free(g_web_impl.config.output_dir);
        if (g_web_impl.config.title) free(g_web_impl.config.title);

        g_web_impl.config = *web_config;
        if (web_config->output_dir) {
            g_web_impl.config.output_dir = strdup(web_config->output_dir);
        }
        if (web_config->title) {
            g_web_impl.config.title = strdup(web_config->title);
        }
    }

    return renderer;
}

// =============================================================================
// ELEMENT TREE TRAVERSAL
// =============================================================================

static void generate_element_html(KryonElement* element, char** html_buffer, size_t* html_capacity, size_t* html_size,
                                  char** css_buffer, size_t* css_capacity, size_t* css_size, int* element_counter, int depth);
static const char* get_property_string(KryonElement* element, const char* prop_name);
static float get_property_float(KryonElement* element, const char* prop_name, float default_value);
static uint32_t get_property_color(KryonElement* element, const char* prop_name, uint32_t default_value);

static const char* get_property_string(KryonElement* element, const char* prop_name) {
    if (!element || !prop_name) return NULL;

    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i] && element->properties[i]->name &&
            strcmp(element->properties[i]->name, prop_name) == 0) {
            if (element->properties[i]->type == KRYON_RUNTIME_PROP_STRING) {
                return element->properties[i]->value.string_value;
            }
        }
    }
    return NULL;
}

static float get_property_float(KryonElement* element, const char* prop_name, float default_value) {
    if (!element || !prop_name) return default_value;

    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i] && element->properties[i]->name &&
            strcmp(element->properties[i]->name, prop_name) == 0) {
            if (element->properties[i]->type == KRYON_RUNTIME_PROP_FLOAT) {
                return element->properties[i]->value.float_value;
            }
        }
    }
    return default_value;
}

static uint32_t get_property_color(KryonElement* element, const char* prop_name, uint32_t default_value) {
    if (!element || !prop_name) return default_value;

    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i] && element->properties[i]->name &&
            strcmp(element->properties[i]->name, prop_name) == 0) {
            if (element->properties[i]->type == KRYON_RUNTIME_PROP_COLOR) {
                return element->properties[i]->value.color_value;
            }
        }
    }
    return default_value;
}

static void generate_element_html(KryonElement* element, char** html_buffer, size_t* html_capacity, size_t* html_size,
                                  char** css_buffer, size_t* css_capacity, size_t* css_size, int* element_counter, int depth) {
    if (!element) return;

    char element_id[64];
    snprintf(element_id, sizeof(element_id), "kryon-el-%d", (*element_counter)++);

    const char* type_name = element->type_name ? element->type_name : "Unknown";

    // Indentation for readability
    char indent[256] = "";
    for (int i = 0; i < depth && i < 32; i++) {
        strcat(indent, "  ");
    }

    // Generate HTML based on element type
    char html_chunk[4096];
    char css_chunk[4096];

    if (strcmp(type_name, "App") == 0) {
        // App element - just process children
        for (size_t i = 0; i < element->child_count; i++) {
            generate_element_html(element->children[i], html_buffer, html_capacity, html_size,
                                css_buffer, css_capacity, css_size, element_counter, depth);
        }
        return;
    }
    else if (strcmp(type_name, "Center") == 0) {
        // Center - div with flexbox centering
        snprintf(html_chunk, sizeof(html_chunk), "%s<div id=\"%s\" class=\"kryon-center\">\n", indent, element_id);
        append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);

        // Generate CSS for centering
        snprintf(css_chunk, sizeof(css_chunk),
                "#%s {\n"
                "  display: flex;\n"
                "  justify-content: center;\n"
                "  align-items: center;\n"
                "  width: 100%%;\n"
                "  height: 100%%;\n"
                "}\n",
                element_id);
        append_to_buffer(css_buffer, css_capacity, css_size, css_chunk);

        // Process children
        for (size_t i = 0; i < element->child_count; i++) {
            generate_element_html(element->children[i], html_buffer, html_capacity, html_size,
                                css_buffer, css_capacity, css_size, element_counter, depth + 1);
        }

        snprintf(html_chunk, sizeof(html_chunk), "%s</div>\n", indent);
        append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);
    }
    else if (strcmp(type_name, "Container") == 0 || strcmp(type_name, "Column") == 0 || strcmp(type_name, "Row") == 0) {
        // Container/Column/Row - div with children
        uint32_t bg_color_val = get_property_color(element, "background", 0x00000000);
        uint32_t border_color_val = get_property_color(element, "borderColor", 0x000000FF);
        float border_radius = get_property_float(element, "borderRadius", 0.0f);
        float border_width = get_property_float(element, "borderWidth", 0.0f);
        float padding_val = get_property_float(element, "padding", 0.0f);
        float pos_x = get_property_float(element, "posX", 0.0f);
        float pos_y = get_property_float(element, "posY", 0.0f);
        float width = get_property_float(element, "width", 0.0f);
        float height = get_property_float(element, "height", 0.0f);

        snprintf(html_chunk, sizeof(html_chunk), "%s<div id=\"%s\" class=\"kryon-container\">\n", indent, element_id);
        append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);

        // Generate CSS
        KryonColor bg_color = {
            .r = ((bg_color_val >> 24) & 0xFF) / 255.0f,
            .g = ((bg_color_val >> 16) & 0xFF) / 255.0f,
            .b = ((bg_color_val >> 8) & 0xFF) / 255.0f,
            .a = (bg_color_val & 0xFF) / 255.0f
        };
        KryonColor border_color = {
            .r = ((border_color_val >> 24) & 0xFF) / 255.0f,
            .g = ((border_color_val >> 16) & 0xFF) / 255.0f,
            .b = ((border_color_val >> 8) & 0xFF) / 255.0f,
            .a = (border_color_val & 0xFF) / 255.0f
        };

        char bg_str[64], border_str[64];
        kryon_css_color_to_string(bg_color, bg_str, sizeof(bg_str));
        kryon_css_color_to_string(border_color, border_str, sizeof(border_str));

        // Build CSS with all properties
        char css_props[1024];
        int css_offset = 0;

        css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                "  background-color: %s;\n", bg_str);

        if (border_width > 0) {
            css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                    "  border: %.0fpx solid %s;\n", border_width, border_str);
        }

        if (border_radius > 0) {
            css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                    "  border-radius: %.2fpx;\n", border_radius);
        }

        if (padding_val > 0) {
            css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                    "  padding: %.2fpx;\n", padding_val);
        }

        if (width > 0) {
            css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                    "  width: %.0fpx;\n", width);
        }

        if (height > 0) {
            css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                    "  height: %.0fpx;\n", height);
        }

        // Use absolute positioning if posX or posY is specified
        if (pos_x != 0 || pos_y != 0) {
            css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                    "  position: absolute;\n  left: %.0fpx;\n  top: %.0fpx;\n", pos_x, pos_y);
        } else {
            css_offset += snprintf(css_props + css_offset, sizeof(css_props) - css_offset,
                    "  position: relative;\n");
        }

        snprintf(css_chunk, sizeof(css_chunk), "#%s {\n%s}\n", element_id, css_props);
        append_to_buffer(css_buffer, css_capacity, css_size, css_chunk);

        // Process children
        for (size_t i = 0; i < element->child_count; i++) {
            generate_element_html(element->children[i], html_buffer, html_capacity, html_size,
                                css_buffer, css_capacity, css_size, element_counter, depth + 1);
        }

        snprintf(html_chunk, sizeof(html_chunk), "%s</div>\n", indent);
        append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);
    }
    else if (strcmp(type_name, "Text") == 0) {
        const char* text = get_property_string(element, "text");
        uint32_t color_val = get_property_color(element, "color", 0x000000FF);
        float font_size = get_property_float(element, "fontSize", 16.0f);

        if (text) {
            char escaped_text[1024];
            kryon_dom_escape_html(text, escaped_text, sizeof(escaped_text));

            snprintf(html_chunk, sizeof(html_chunk), "%s<span id=\"%s\" class=\"kryon-text\">%s</span>\n",
                    indent, element_id, escaped_text);
            append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);

            KryonColor text_color = {
                .r = ((color_val >> 24) & 0xFF) / 255.0f,
                .g = ((color_val >> 16) & 0xFF) / 255.0f,
                .b = ((color_val >> 8) & 0xFF) / 255.0f,
                .a = (color_val & 0xFF) / 255.0f
            };

            char color_str[64];
            kryon_css_color_to_string(text_color, color_str, sizeof(color_str));

            snprintf(css_chunk, sizeof(css_chunk),
                    "#%s {\n"
                    "  color: %s;\n"
                    "  font-size: %.2fpx;\n"
                    "}\n",
                    element_id, color_str, font_size);
            append_to_buffer(css_buffer, css_capacity, css_size, css_chunk);
        }
    }
    else if (strcmp(type_name, "Button") == 0) {
        const char* text = get_property_string(element, "text");
        uint32_t bg_color_val = get_property_color(element, "background", 0x404080FF);
        uint32_t border_color_val = get_property_color(element, "borderColor", 0xCCCCCCFF);
        float width = get_property_float(element, "width", 0.0f);
        float height = get_property_float(element, "height", 0.0f);

        if (text) {
            char escaped_text[512];
            kryon_dom_escape_html(text, escaped_text, sizeof(escaped_text));

            snprintf(html_chunk, sizeof(html_chunk), "%s<button id=\"%s\" class=\"kryon-button\">%s</button>\n",
                    indent, element_id, escaped_text);
            append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);

            // Convert colors
            KryonColor bg_color = {
                .r = ((bg_color_val >> 24) & 0xFF) / 255.0f,
                .g = ((bg_color_val >> 16) & 0xFF) / 255.0f,
                .b = ((bg_color_val >> 8) & 0xFF) / 255.0f,
                .a = (bg_color_val & 0xFF) / 255.0f
            };
            KryonColor border_color = {
                .r = ((border_color_val >> 24) & 0xFF) / 255.0f,
                .g = ((border_color_val >> 16) & 0xFF) / 255.0f,
                .b = ((border_color_val >> 8) & 0xFF) / 255.0f,
                .a = (border_color_val & 0xFF) / 255.0f
            };

            char bg_str[64], border_str[64];
            kryon_css_color_to_string(bg_color, bg_str, sizeof(bg_str));
            kryon_css_color_to_string(border_color, border_str, sizeof(border_str));

            snprintf(css_chunk, sizeof(css_chunk),
                    "#%s {\n"
                    "  padding: 10px 20px;\n"
                    "  cursor: pointer;\n"
                    "  border: 1px solid %s;\n"
                    "  border-radius: 4px;\n"
                    "  background: %s;\n"
                    "%s%s"
                    "}\n"
                    "#%s:hover { opacity: 0.9; }\n",
                    element_id,
                    border_str,
                    bg_str,
                    width > 0 ? "  width: " : "", width > 0 ? "px;\n" : "",
                    element_id);

            // Handle width and height separately if specified
            if (width > 0 || height > 0) {
                char size_css[256] = "";
                if (width > 0) {
                    snprintf(size_css, sizeof(size_css), "  width: %.0fpx;\n", width);
                }
                if (height > 0) {
                    char h_css[128];
                    snprintf(h_css, sizeof(h_css), "  height: %.0fpx;\n", height);
                    strncat(size_css, h_css, sizeof(size_css) - strlen(size_css) - 1);
                }

                // Rewrite the CSS with size
                snprintf(css_chunk, sizeof(css_chunk),
                        "#%s {\n"
                        "  padding: 10px 20px;\n"
                        "  cursor: pointer;\n"
                        "  border: 1px solid %s;\n"
                        "  border-radius: 4px;\n"
                        "  background: %s;\n"
                        "%s"
                        "}\n"
                        "#%s:hover { opacity: 0.9; }\n",
                        element_id, border_str, bg_str, size_css, element_id);
            }

            append_to_buffer(css_buffer, css_capacity, css_size, css_chunk);
        }
    }
    else if (strcmp(type_name, "Input") == 0) {
        const char* placeholder = get_property_string(element, "placeholder");

        snprintf(html_chunk, sizeof(html_chunk), "%s<input type=\"text\" id=\"%s\" class=\"kryon-input\" placeholder=\"%s\">\n",
                indent, element_id, placeholder ? placeholder : "");
        append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);

        snprintf(css_chunk, sizeof(css_chunk),
                "#%s {\n"
                "  padding: 8px;\n"
                "  border: 1px solid #ccc;\n"
                "  border-radius: 4px;\n"
                "}\n",
                element_id);
        append_to_buffer(css_buffer, css_capacity, css_size, css_chunk);
    }
    else {
        // Generic element
        snprintf(html_chunk, sizeof(html_chunk), "%s<!-- %s element (id=%s) -->\n", indent, type_name, element_id);
        append_to_buffer(html_buffer, html_capacity, html_size, html_chunk);
    }
}

bool kryon_web_renderer_finalize(KryonRenderer* renderer) {
    if (!renderer || !renderer->impl_data) {
        return false;
    }

    WebRendererImpl* impl = (WebRendererImpl*)renderer->impl_data;

    // Create output directory if it doesn't exist
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", impl->config.output_dir);
    system(mkdir_cmd);

    // Write HTML file
    char html_path[512];
    snprintf(html_path, sizeof(html_path), "%s/index.html", impl->config.output_dir);
    FILE* html_file = fopen(html_path, "w");
    if (!html_file) {
        fprintf(stderr, "Failed to open %s for writing\n", html_path);
        return false;
    }

    if (impl->html_buffer) {
        fwrite(impl->html_buffer, 1, impl->html_size, html_file);
    }
    fclose(html_file);

    // Write CSS file (if not inline)
    if (!impl->config.inline_styles && impl->css_buffer) {
        char css_path[512];
        snprintf(css_path, sizeof(css_path), "%s/styles.css", impl->config.output_dir);
        FILE* css_file = fopen(css_path, "w");
        if (css_file) {
            fwrite(impl->css_buffer, 1, impl->css_size, css_file);
            fclose(css_file);
        }
    }

    // Write JS file (if not inline)
    if (!impl->config.inline_scripts && impl->js_buffer) {
        char js_path[512];
        snprintf(js_path, sizeof(js_path), "%s/runtime.js", impl->config.output_dir);
        FILE* js_file = fopen(js_path, "w");
        if (js_file) {
            fwrite(impl->js_buffer, 1, impl->js_size, js_file);
            fclose(js_file);
        }
    }

    printf("✅ Web output generated in %s/\n", impl->config.output_dir);
    return true;
}

bool kryon_web_renderer_finalize_with_runtime(KryonRenderer* renderer, KryonRuntime* runtime) {
    if (!renderer || !renderer->impl_data || !runtime) {
        return false;
    }

    WebRendererImpl* impl = (WebRendererImpl*)renderer->impl_data;

    printf("🔍 Traversing element tree from runtime...\n");

    // Initialize buffers if not already done
    if (!impl->initialized) {
        web_initialize(NULL);
    }

    // Traverse the element tree and generate HTML/CSS
    if (runtime->root) {
        printf("🌳 Found root element: %s\n", runtime->root->type_name ? runtime->root->type_name : "Unknown");

        int element_counter = 0;

        // Reset buffers and start fresh
        impl->html_size = 0;
        impl->css_size = 0;
        impl->js_size = 0;
        if (impl->html_buffer) impl->html_buffer[0] = '\0';
        if (impl->css_buffer) impl->css_buffer[0] = '\0';
        if (impl->js_buffer) impl->js_buffer[0] = '\0';

        // Start HTML document structure
        append_to_buffer(&impl->html_buffer, &impl->html_capacity, &impl->html_size,
                        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");

        char title_buf[256];
        const char* title = get_property_string(runtime->root, "title");
        if (!title) title = "Kryon Application";
        snprintf(title_buf, sizeof(title_buf), "  <title>%s</title>\n", title);
        append_to_buffer(&impl->html_buffer, &impl->html_capacity, &impl->html_size, title_buf);

        append_to_buffer(&impl->html_buffer, &impl->html_capacity, &impl->html_size,
                        "  <meta charset=\"UTF-8\">\n"
                        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                        "  <link rel=\"stylesheet\" href=\"styles.css\">\n"
                        "</head>\n<body>\n"
                        "  <div id=\"app\" class=\"kryon-app\">\n");

        // Add base CSS
        append_to_buffer(&impl->css_buffer, &impl->css_capacity, &impl->css_size,
                        "* { margin: 0; padding: 0; box-sizing: border-box; }\n"
                        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif; }\n");

        // Generate .kryon-app CSS with background color from App element
        // Use get_element_property_color which resolves styles automatically
        uint32_t app_bg_color = 0xFFFFFFFF; // default white
        if (runtime->root) {
            // Try "backgroundColor" first (used in @style blocks), then "background"
            app_bg_color = get_element_property_color(runtime->root, "backgroundColor", 0xFFFFFFFF);
            if (app_bg_color == 0xFFFFFFFF) {
                app_bg_color = get_element_property_color(runtime->root, "background", 0xFFFFFFFF);
            }
        }

        // Generate .kryon-app CSS with background
        KryonColor app_bg = {
            .r = ((app_bg_color >> 24) & 0xFF) / 255.0f,
            .g = ((app_bg_color >> 16) & 0xFF) / 255.0f,
            .b = ((app_bg_color >> 8) & 0xFF) / 255.0f,
            .a = (app_bg_color & 0xFF) / 255.0f
        };
        char app_bg_str[64];
        kryon_css_color_to_string(app_bg, app_bg_str, sizeof(app_bg_str));

        char app_css[256];
        snprintf(app_css, sizeof(app_css),
                ".kryon-app { width: 100%%; height: 100vh; background-color: %s; }\n"
                ".kryon-container { display: block; }\n",
                app_bg_str);
        append_to_buffer(&impl->css_buffer, &impl->css_capacity, &impl->css_size, app_css);

        // Traverse children (skip App element itself)
        for (size_t i = 0; i < runtime->root->child_count; i++) {
            generate_element_html(runtime->root->children[i],
                                &impl->html_buffer, &impl->html_capacity, &impl->html_size,
                                &impl->css_buffer, &impl->css_capacity, &impl->css_size,
                                &element_counter, 2);
        }

        // Close HTML document
        append_to_buffer(&impl->html_buffer, &impl->html_capacity, &impl->html_size,
                        "  </div>\n"
                        "  <script src=\"runtime.js\"></script>\n"
                        "</body>\n</html>\n");

        printf("✅ Generated %d elements\n", element_counter);
    } else {
        fprintf(stderr, "⚠️ No root element found in runtime\n");
    }

    // Copy runtime JavaScript
    FILE* runtime_js_src = fopen("web/runtime/kryon-runtime.js", "r");
    if (runtime_js_src) {
        fseek(runtime_js_src, 0, SEEK_END);
        long js_size = ftell(runtime_js_src);
        fseek(runtime_js_src, 0, SEEK_SET);

        char* js_content = malloc(js_size + 1);
        if (js_content) {
            fread(js_content, 1, js_size, runtime_js_src);
            js_content[js_size] = '\0';
            append_to_buffer(&impl->js_buffer, &impl->js_capacity, &impl->js_size, js_content);
            free(js_content);
        }
        fclose(runtime_js_src);
    }

    // Now call the regular finalize to write files
    return kryon_web_renderer_finalize(renderer);
}

// =============================================================================
// RENDERER IMPLEMENTATION
// =============================================================================

static KryonRenderResult web_initialize(void* surface) {
    // Initialize buffers
    g_web_impl.html_capacity = INITIAL_BUFFER_SIZE;
    g_web_impl.css_capacity = INITIAL_BUFFER_SIZE;
    g_web_impl.js_capacity = INITIAL_BUFFER_SIZE;

    g_web_impl.html_buffer = malloc(g_web_impl.html_capacity);
    g_web_impl.css_buffer = malloc(g_web_impl.css_capacity);
    g_web_impl.js_buffer = malloc(g_web_impl.js_capacity);

    if (!g_web_impl.html_buffer || !g_web_impl.css_buffer || !g_web_impl.js_buffer) {
        return KRYON_RENDER_ERROR_OUT_OF_MEMORY;
    }

    g_web_impl.html_buffer[0] = '\0';
    g_web_impl.css_buffer[0] = '\0';
    g_web_impl.js_buffer[0] = '\0';
    g_web_impl.html_size = 0;
    g_web_impl.css_size = 0;
    g_web_impl.js_size = 0;

    g_web_impl.initialized = true;

    // Start HTML document
    append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                    "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");

    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "  <title>%s</title>\n", g_web_impl.config.title);
    append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size, title_buf);

    append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                    "  <meta charset=\"UTF-8\">\n"
                    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");

    // Add CSS link or inline styles
    if (g_web_impl.config.inline_styles) {
        append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                        "  <style>\n");
    } else {
        append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                        "  <link rel=\"stylesheet\" href=\"styles.css\">\n");
    }

    // Add base CSS reset
    append_to_buffer(&g_web_impl.css_buffer, &g_web_impl.css_capacity, &g_web_impl.css_size,
                    "* { margin: 0; padding: 0; box-sizing: border-box; }\n"
                    "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif; }\n"
                    ".kryon-container { position: relative; }\n");

    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult web_begin_frame(KryonRenderContext** context, KryonColor clear_color) {
    if (!g_web_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }

    // Start body tag with background color
    if (g_web_impl.config.inline_styles) {
        append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                        "  </style>\n");
    }

    append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                    "</head>\n<body>\n");

    // Set body background color
    char bg_color[64];
    kryon_css_color_to_string(clear_color, bg_color, sizeof(bg_color));
    char body_style[128];
    snprintf(body_style, sizeof(body_style), "body { background-color: %s; }\n", bg_color);
    append_to_buffer(&g_web_impl.css_buffer, &g_web_impl.css_capacity, &g_web_impl.css_size, body_style);

    *context = (KryonRenderContext*)&g_web_impl;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult web_end_frame(KryonRenderContext* context) {
    if (!context) return KRYON_RENDER_ERROR_INVALID_PARAM;

    // Close body and add scripts
    append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                    "\n</body>\n");

    // Add JavaScript runtime
    if (g_web_impl.config.inline_scripts) {
        append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                        "<script>\n");
        append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                        g_web_impl.js_buffer);
        append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                        "</script>\n");
    } else {
        if (g_web_impl.config.include_fengari) {
            append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                            "<script src=\"https://cdn.jsdelivr.net/npm/fengari-web@0.1.4/dist/fengari-web.js\"></script>\n");
        }
        append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                        "<script src=\"runtime.js\"></script>\n");
    }

    append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size,
                    "</html>\n");

    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult web_execute_commands(KryonRenderContext* context,
                                             const KryonRenderCommand* commands,
                                             size_t command_count) {
    if (!context || !commands) return KRYON_RENDER_ERROR_INVALID_PARAM;

    char element_id[64];
    char html_buf[4096];
    char css_buf[4096];

    for (size_t i = 0; i < command_count; i++) {
        const KryonRenderCommand* cmd = &commands[i];

        // Generate unique element ID
        snprintf(element_id, sizeof(element_id), "kryon-el-%d", g_web_impl.element_id_counter++);

        // Generate HTML
        size_t html_len = kryon_dom_generate_for_command(cmd, element_id, html_buf, sizeof(html_buf));
        if (html_len > 0) {
            append_to_buffer(&g_web_impl.html_buffer, &g_web_impl.html_capacity, &g_web_impl.html_size, html_buf);
        }

        // Generate CSS
        size_t css_len = kryon_css_generate_for_command(cmd, element_id, css_buf, sizeof(css_buf));
        if (css_len > 0) {
            append_to_buffer(&g_web_impl.css_buffer, &g_web_impl.css_capacity, &g_web_impl.css_size, css_buf);
        }
    }

    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult web_resize(KryonVec2 new_size) {
    // Web renderer doesn't need to handle resize - browser handles it
    return KRYON_RENDER_SUCCESS;
}

static KryonVec2 web_viewport_size(void) {
    // Return default viewport size - browser will handle actual sizing
    return (KryonVec2){800, 600};
}

static void web_destroy(void) {
    if (g_web_impl.initialized) {
        free(g_web_impl.html_buffer);
        free(g_web_impl.css_buffer);
        free(g_web_impl.js_buffer);
        if (g_web_impl.config.output_dir) free(g_web_impl.config.output_dir);
        if (g_web_impl.config.title) free(g_web_impl.config.title);
        g_web_impl.initialized = false;
    }
}

static bool web_point_in_element(KryonVec2 point, KryonRect element_bounds) {
    // Browser handles hit testing
    return true;
}

static bool web_handle_event(const KryonEvent* event) {
    // Browser handles events
    return false;
}

static void* web_get_native_window(void) {
    return NULL;
}

static float web_measure_text_width(const char* text, float font_size) {
    // Rough estimate - browser will handle actual measurement
    if (!text) return 0.0f;
    return strlen(text) * (font_size * 0.6f);
}

static KryonRenderResult web_set_cursor(KryonCursorType cursor_type) {
    // Browser handles cursor - could add CSS cursor property
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult web_update_window_size(int width, int height) {
    // Browser handles window sizing
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult web_update_window_title(const char* title) {
    if (title && g_web_impl.config.title) {
        free(g_web_impl.config.title);
        g_web_impl.config.title = strdup(title);
    }
    return KRYON_RENDER_SUCCESS;
}