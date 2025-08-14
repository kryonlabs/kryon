/**
 * @file html_renderer.c
 * @brief HTML Renderer Implementation
 * 
 * HTML/CSS-based renderer that generates static output files
 */

#include "renderer_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// =============================================================================
// HTML SPECIFIC TYPES
// =============================================================================

typedef struct {
    char* output_path;
    char* title;
    int width;
    int height;
    bool include_viewport_meta;
    bool responsive;
} HtmlSurface;

typedef struct {
    FILE* html_file;
    FILE* css_file;
    int element_counter;
    int container_depth;
    char* current_container_id;
    bool in_frame;
} HtmlRenderContext;

typedef struct {
    char* output_path;
    char* title;
    int width;
    int height;
    bool initialized;
    int frame_counter;
} HtmlRendererImpl;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonRenderResult html_initialize(void* surface);
static KryonRenderResult html_begin_frame(KryonRenderContext** context, KryonColor clear_color);
static KryonRenderResult html_end_frame(KryonRenderContext* context);
static KryonRenderResult html_execute_commands(KryonRenderContext* context,
                                              const KryonRenderCommand* commands,
                                              size_t command_count);
static KryonRenderResult html_resize(KryonVec2 new_size);
static KryonVec2 html_viewport_size(void);
static void html_destroy(void);

// Helper functions
static void write_html_header(FILE* file, const char* title, int width, int height);
static void write_html_footer(FILE* file);
static void write_css_reset(FILE* file);
static char* kryon_color_to_css(KryonColor color);
static char* sanitize_string(const char* str);
static void generate_element_id(char* buffer, size_t size, const char* prefix, int counter);

// =============================================================================
// GLOBAL STATE
// =============================================================================

static HtmlRendererImpl g_html_impl = {0};

// Forward declarations for input handling
static KryonRenderResult html_get_input_state(KryonInputState* input_state);
static bool html_point_in_widget(KryonVec2 point, KryonRect widget_bounds);
static bool html_handle_event(const KryonEvent* event);
static void* html_get_native_window(void);

static KryonRendererVTable g_html_vtable = {
    .initialize = html_initialize,
    .begin_frame = html_begin_frame,
    .end_frame = html_end_frame,
    .execute_commands = html_execute_commands,
    .resize = html_resize,
    .viewport_size = html_viewport_size,
    .destroy = html_destroy,
    .get_input_state = html_get_input_state,
    .point_in_widget = html_point_in_widget,
    .handle_event = html_handle_event,
    .get_native_window = html_get_native_window
};

// =============================================================================
// PUBLIC API
// =============================================================================

KryonRenderer* kryon_html_renderer_create(void* surface) {
    KryonRenderer* renderer = malloc(sizeof(KryonRenderer));
    if (!renderer) {
        return NULL;
    }
    
    renderer->vtable = &g_html_vtable;
    renderer->impl_data = &g_html_impl;
    renderer->name = strdup("HTML Renderer");
    renderer->backend = strdup("html");
    
    // Initialize with provided surface
    if (html_initialize(surface) != KRYON_RENDER_SUCCESS) {
        free(renderer->name);
        free(renderer->backend);
        free(renderer);
        return NULL;
    }
    
    return renderer;
}

// =============================================================================
// RENDERER IMPLEMENTATION
// =============================================================================

static KryonRenderResult html_initialize(void* surface) {
    HtmlSurface* surf = (HtmlSurface*)surface;
    if (!surf || !surf->output_path) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    g_html_impl.output_path = strdup(surf->output_path);
    g_html_impl.title = strdup(surf->title ? surf->title : "Kryon Application");
    g_html_impl.width = surf->width;
    g_html_impl.height = surf->height;
    g_html_impl.initialized = true;
    g_html_impl.frame_counter = 0;
    
    printf("HTML Renderer initialized: output=%s, size=%dx%d\n", 
           surf->output_path, surf->width, surf->height);
    
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult html_begin_frame(KryonRenderContext** context, KryonColor clear_color) {
    if (!g_html_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    HtmlRenderContext* ctx = malloc(sizeof(HtmlRenderContext));
    if (!ctx) {
        return KRYON_RENDER_ERROR_OUT_OF_MEMORY;
    }
    
    // Generate output filenames
    char html_path[512];
    char css_path[512];
    
    if (g_html_impl.frame_counter == 0) {
        snprintf(html_path, sizeof(html_path), "%s.html", g_html_impl.output_path);
        snprintf(css_path, sizeof(css_path), "%s.css", g_html_impl.output_path);
    } else {
        snprintf(html_path, sizeof(html_path), "%s_%d.html", g_html_impl.output_path, g_html_impl.frame_counter);
        snprintf(css_path, sizeof(css_path), "%s_%d.css", g_html_impl.output_path, g_html_impl.frame_counter);
    }
    
    // Open output files
    ctx->html_file = fopen(html_path, "w");
    ctx->css_file = fopen(css_path, "w");
    
    if (!ctx->html_file || !ctx->css_file) {
        if (ctx->html_file) fclose(ctx->html_file);
        if (ctx->css_file) fclose(ctx->css_file);
        free(ctx);
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    ctx->element_counter = 0;
    ctx->container_depth = 0;
    ctx->current_container_id = strdup("body");
    ctx->in_frame = true;
    
    // Write HTML header
    write_html_header(ctx->html_file, g_html_impl.title, g_html_impl.width, g_html_impl.height);
    
    // Write CSS reset and base styles
    write_css_reset(ctx->css_file);
    
    // Set body background color
    char* bg_color = kryon_color_to_css(clear_color);
    fprintf(ctx->css_file, "body {\n");
    fprintf(ctx->css_file, "  background-color: %s;\n", bg_color);
    fprintf(ctx->css_file, "  width: %dpx;\n", g_html_impl.width);
    fprintf(ctx->css_file, "  height: %dpx;\n", g_html_impl.height);
    fprintf(ctx->css_file, "  position: relative;\n");
    fprintf(ctx->css_file, "  overflow: hidden;\n");
    fprintf(ctx->css_file, "}\n\n");
    free(bg_color);
    
    *context = (KryonRenderContext*)ctx;
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult html_end_frame(KryonRenderContext* context) {
    if (!context) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    HtmlRenderContext* ctx = (HtmlRenderContext*)context;
    
    // Close any open containers
    while (ctx->container_depth > 0) {
        fprintf(ctx->html_file, "</div>\n");
        ctx->container_depth--;
    }
    
    // Write HTML footer
    write_html_footer(ctx->html_file);
    
    // Close files
    fclose(ctx->html_file);
    fclose(ctx->css_file);
    
    // Cleanup
    free(ctx->current_container_id);
    free(ctx);
    
    g_html_impl.frame_counter++;
    
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult html_execute_commands(KryonRenderContext* context,
                                              const KryonRenderCommand* commands,
                                              size_t command_count) {
    if (!context || !commands) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    HtmlRenderContext* ctx = (HtmlRenderContext*)context;
    
    for (size_t i = 0; i < command_count; i++) {
        const KryonRenderCommand* cmd = &commands[i];
        
        switch (cmd->type) {
            case KRYON_CMD_SET_CANVAS_SIZE: {
                // Canvas size is handled during initialization
                break;
            }
            
            case KRYON_CMD_DRAW_RECT: {
                const KryonDrawRectData* data = &cmd->data.draw_rect;
                
                char element_id[64];
                generate_element_id(element_id, sizeof(element_id), "rect", ctx->element_counter++);
                
                // Generate HTML element
                fprintf(ctx->html_file, "<div id=\"%s\"></div>\n", element_id);
                
                // Generate CSS styles
                char* color = kryon_color_to_css(data->color);
                char* border_color = kryon_color_to_css(data->border_color);
                
                fprintf(ctx->css_file, "#%s {\n", element_id);
                fprintf(ctx->css_file, "  position: absolute;\n");
                fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                fprintf(ctx->css_file, "  width: %.1fpx;\n", data->size.x);
                fprintf(ctx->css_file, "  height: %.1fpx;\n", data->size.y);
                fprintf(ctx->css_file, "  background-color: %s;\n", color);
                
                if (data->border_radius > 0) {
                    fprintf(ctx->css_file, "  border-radius: %.1fpx;\n", data->border_radius);
                }
                
                if (data->border_width > 0) {
                    fprintf(ctx->css_file, "  border: %.1fpx solid %s;\n", data->border_width, border_color);
                }
                
                fprintf(ctx->css_file, "}\n\n");
                
                free(color);
                free(border_color);
                break;
            }
            
            case KRYON_CMD_DRAW_TEXT: {
                const KryonDrawTextData* data = &cmd->data.draw_text;
                
                if (data->text) {
                    char element_id[64];
                    generate_element_id(element_id, sizeof(element_id), "text", ctx->element_counter++);
                    
                    char* sanitized_text = sanitize_string(data->text);
                    
                    // Generate HTML element
                    fprintf(ctx->html_file, "<div id=\"%s\">%s</div>\n", element_id, sanitized_text);
                    
                    // Generate CSS styles
                    char* color = kryon_color_to_css(data->color);
                    
                    fprintf(ctx->css_file, "#%s {\n", element_id);
                    fprintf(ctx->css_file, "  position: absolute;\n");
                    fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                    fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                    fprintf(ctx->css_file, "  color: %s;\n", color);
                    fprintf(ctx->css_file, "  font-size: %.1fpx;\n", data->font_size);
                    
                    if (data->font_family) {
                        char* sanitized_font = sanitize_string(data->font_family);
                        fprintf(ctx->css_file, "  font-family: '%s', sans-serif;\n", sanitized_font);
                        free(sanitized_font);
                    }
                    
                    if (data->bold) {
                        fprintf(ctx->css_file, "  font-weight: bold;\n");
                    }
                    
                    if (data->italic) {
                        fprintf(ctx->css_file, "  font-style: italic;\n");
                    }
                    
                    if (data->max_width > 0) {
                        fprintf(ctx->css_file, "  max-width: %.1fpx;\n", data->max_width);
                        fprintf(ctx->css_file, "  word-wrap: break-word;\n");
                    }
                    
                    // Text alignment
                    switch (data->text_align) {
                        case 1: fprintf(ctx->css_file, "  text-align: center;\n"); break;
                        case 2: fprintf(ctx->css_file, "  text-align: right;\n"); break;
                        default: fprintf(ctx->css_file, "  text-align: left;\n"); break;
                    }
                    
                    fprintf(ctx->css_file, "}\n\n");
                    
                    free(sanitized_text);
                    free(color);
                }
                break;
            }
            
            case KRYON_CMD_DRAW_IMAGE: {
                const KryonDrawImageData* data = &cmd->data.draw_image;
                
                if (data->source) {
                    char element_id[64];
                    generate_element_id(element_id, sizeof(element_id), "img", ctx->element_counter++);
                    
                    char* sanitized_src = sanitize_string(data->source);
                    
                    // Generate HTML element
                    fprintf(ctx->html_file, "<img id=\"%s\" src=\"%s\" alt=\"Image\">\n", element_id, sanitized_src);
                    
                    // Generate CSS styles
                    fprintf(ctx->css_file, "#%s {\n", element_id);
                    fprintf(ctx->css_file, "  position: absolute;\n");
                    fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                    fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                    fprintf(ctx->css_file, "  width: %.1fpx;\n", data->size.x);
                    fprintf(ctx->css_file, "  height: %.1fpx;\n", data->size.y);
                    fprintf(ctx->css_file, "  opacity: %.2f;\n", data->opacity);
                    
                    if (data->maintain_aspect) {
                        fprintf(ctx->css_file, "  object-fit: contain;\n");
                    } else {
                        fprintf(ctx->css_file, "  object-fit: fill;\n");
                    }
                    
                    fprintf(ctx->css_file, "}\n\n");
                    
                    free(sanitized_src);
                }
                break;
            }
            
            case KRYON_CMD_BEGIN_CONTAINER: {
                const KryonBeginContainerData* data = &cmd->data.begin_container;
                
                char element_id[64];
                if (data->element_id) {
                    char* sanitized_id = sanitize_string(data->element_id);
                    strncpy(element_id, sanitized_id, sizeof(element_id) - 1);
                    element_id[sizeof(element_id) - 1] = '\0';
                    free(sanitized_id);
                } else {
                    generate_element_id(element_id, sizeof(element_id), "container", ctx->element_counter++);
                }
                
                // Generate HTML element
                fprintf(ctx->html_file, "<div id=\"%s\">\n", element_id);
                ctx->container_depth++;
                
                // Update current container
                free(ctx->current_container_id);
                ctx->current_container_id = strdup(element_id);
                
                // Generate CSS styles
                fprintf(ctx->css_file, "#%s {\n", element_id);
                fprintf(ctx->css_file, "  position: absolute;\n");
                fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                fprintf(ctx->css_file, "  width: %.1fpx;\n", data->size.x);
                fprintf(ctx->css_file, "  height: %.1fpx;\n", data->size.y);
                
                if (data->background_color.a > 0) {
                    char* bg_color = kryon_color_to_css(data->background_color);
                    fprintf(ctx->css_file, "  background-color: %s;\n", bg_color);
                    free(bg_color);
                }
                
                if (data->opacity < 1.0f) {
                    fprintf(ctx->css_file, "  opacity: %.2f;\n", data->opacity);
                }
                
                if (data->clip_children) {
                    fprintf(ctx->css_file, "  overflow: hidden;\n");
                }
                
                fprintf(ctx->css_file, "}\n\n");
                break;
            }
            
            case KRYON_CMD_END_CONTAINER: {
                if (ctx->container_depth > 0) {
                    fprintf(ctx->html_file, "</div>\n");
                    ctx->container_depth--;
                }
                break;
            }
            
            case KRYON_CMD_DRAW_BUTTON: {
                const KryonDrawButtonData* data = &cmd->data.draw_button;
                
                char element_id[64];
                if (data->widget_id) {
                    strncpy(element_id, data->widget_id, sizeof(element_id) - 1);
                    element_id[sizeof(element_id) - 1] = '\0';
                } else {
                    generate_element_id(element_id, sizeof(element_id), "button", ctx->element_counter++);
                }
                
                // Generate HTML button element
                fprintf(ctx->html_file, "<button id=\"%s\" type=\"button\"", element_id);
                
                if (data->onclick_handler) {
                    fprintf(ctx->html_file, " onclick=\"%s()\"", data->onclick_handler);
                }
                
                if (!data->enabled) {
                    fprintf(ctx->html_file, " disabled");
                }
                
                fprintf(ctx->html_file, ">%s</button>\n", data->text ? data->text : "");
                
                // Generate CSS styles
                char* bg_color = kryon_color_to_css(data->background_color);
                char* text_color = kryon_color_to_css(data->text_color);
                char* border_color = kryon_color_to_css(data->border_color);
                
                fprintf(ctx->css_file, "#%s {\n", element_id);
                fprintf(ctx->css_file, "  position: absolute;\n");
                fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                fprintf(ctx->css_file, "  width: %.1fpx;\n", data->size.x);
                fprintf(ctx->css_file, "  height: %.1fpx;\n", data->size.y);
                fprintf(ctx->css_file, "  background-color: %s;\n", bg_color);
                fprintf(ctx->css_file, "  color: %s;\n", text_color);
                fprintf(ctx->css_file, "  border: %.1fpx solid %s;\n", data->border_width, border_color);
                
                if (data->border_radius > 0) {
                    fprintf(ctx->css_file, "  border-radius: %.1fpx;\n", data->border_radius);
                }
                
                fprintf(ctx->css_file, "  cursor: pointer;\n");
                fprintf(ctx->css_file, "  font-size: 14px;\n");
                fprintf(ctx->css_file, "  display: flex;\n");
                fprintf(ctx->css_file, "  align-items: center;\n");
                fprintf(ctx->css_file, "  justify-content: center;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                // Add hover and active states
                fprintf(ctx->css_file, "#%s:hover {\n", element_id);
                fprintf(ctx->css_file, "  opacity: 0.9;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                fprintf(ctx->css_file, "#%s:active {\n", element_id);
                fprintf(ctx->css_file, "  opacity: 0.8;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                free(bg_color);
                free(text_color);
                free(border_color);
                break;
            }
            
            case KRYON_CMD_DRAW_DROPDOWN: {
                const KryonDrawDropdownData* data = &cmd->data.draw_dropdown;
                
                char element_id[64];
                if (data->widget_id) {
                    strncpy(element_id, data->widget_id, sizeof(element_id) - 1);
                    element_id[sizeof(element_id) - 1] = '\0';
                } else {
                    generate_element_id(element_id, sizeof(element_id), "dropdown", ctx->element_counter++);
                }
                
                // Generate HTML select element
                fprintf(ctx->html_file, "<select id=\"%s\"", element_id);
                
                if (!data->enabled) {
                    fprintf(ctx->html_file, " disabled");
                }
                
                fprintf(ctx->html_file, ">\n");
                
                // Add options
                if (data->items) {
                    for (size_t j = 0; j < data->item_count; j++) {
                        fprintf(ctx->html_file, "  <option value=\"%s\"", 
                               data->items[j].value ? data->items[j].value : "");
                        
                        if ((int)j == data->selected_index) {
                            fprintf(ctx->html_file, " selected");
                        }
                        
                        fprintf(ctx->html_file, ">%s</option>\n", 
                               data->items[j].text ? data->items[j].text : "");
                    }
                }
                
                fprintf(ctx->html_file, "</select>\n");
                
                // Generate CSS styles
                char* bg_color = kryon_color_to_css(data->background_color);
                char* text_color = kryon_color_to_css(data->text_color);
                char* border_color = kryon_color_to_css(data->border_color);
                
                fprintf(ctx->css_file, "#%s {\n", element_id);
                fprintf(ctx->css_file, "  position: absolute;\n");
                fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                fprintf(ctx->css_file, "  width: %.1fpx;\n", data->size.x);
                fprintf(ctx->css_file, "  height: %.1fpx;\n", data->size.y);
                fprintf(ctx->css_file, "  background-color: %s;\n", bg_color);
                fprintf(ctx->css_file, "  color: %s;\n", text_color);
                fprintf(ctx->css_file, "  border: %.1fpx solid %s;\n", data->border_width, border_color);
                
                if (data->border_radius > 0) {
                    fprintf(ctx->css_file, "  border-radius: %.1fpx;\n", data->border_radius);
                }
                
                fprintf(ctx->css_file, "  font-size: 14px;\n");
                fprintf(ctx->css_file, "  padding: 4px 8px;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                free(bg_color);
                free(text_color);
                free(border_color);
                break;
            }
            
            case KRYON_CMD_DRAW_INPUT: {
                const KryonDrawInputData* data = &cmd->data.draw_input;
                
                char element_id[64];
                if (data->widget_id) {
                    strncpy(element_id, data->widget_id, sizeof(element_id) - 1);
                    element_id[sizeof(element_id) - 1] = '\0';
                } else {
                    generate_element_id(element_id, sizeof(element_id), "input", ctx->element_counter++);
                }
                
                // Generate HTML input element
                fprintf(ctx->html_file, "<input id=\"%s\" type=\"%s\"", 
                       element_id, data->is_password ? "password" : "text");
                
                if (data->text) {
                    char* sanitized_text = sanitize_string(data->text);
                    fprintf(ctx->html_file, " value=\"%s\"", sanitized_text);
                    free(sanitized_text);
                }
                
                if (data->placeholder) {
                    char* sanitized_placeholder = sanitize_string(data->placeholder);
                    fprintf(ctx->html_file, " placeholder=\"%s\"", sanitized_placeholder);
                    free(sanitized_placeholder);
                }
                
                if (!data->enabled) {
                    fprintf(ctx->html_file, " disabled");
                }
                
                fprintf(ctx->html_file, ">\n");
                
                // Generate CSS styles
                char* bg_color = kryon_color_to_css(data->background_color);
                char* text_color = kryon_color_to_css(data->text_color);
                char* border_color = kryon_color_to_css(data->border_color);
                
                fprintf(ctx->css_file, "#%s {\n", element_id);
                fprintf(ctx->css_file, "  position: absolute;\n");
                fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                fprintf(ctx->css_file, "  width: %.1fpx;\n", data->size.x);
                fprintf(ctx->css_file, "  height: %.1fpx;\n", data->size.y);
                fprintf(ctx->css_file, "  background-color: %s;\n", bg_color);
                fprintf(ctx->css_file, "  color: %s;\n", text_color);
                fprintf(ctx->css_file, "  border: %.1fpx solid %s;\n", data->border_width, border_color);
                
                if (data->border_radius > 0) {
                    fprintf(ctx->css_file, "  border-radius: %.1fpx;\n", data->border_radius);
                }
                
                fprintf(ctx->css_file, "  font-size: 14px;\n");
                fprintf(ctx->css_file, "  padding: 4px 8px;\n");
                fprintf(ctx->css_file, "  box-sizing: border-box;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                // Focus styles
                fprintf(ctx->css_file, "#%s:focus {\n", element_id);
                fprintf(ctx->css_file, "  outline: none;\n");
                fprintf(ctx->css_file, "  border-color: #4a90e2;\n");
                fprintf(ctx->css_file, "  box-shadow: 0 0 4px rgba(74, 144, 226, 0.3);\n");
                fprintf(ctx->css_file, "}\n\n");
                
                free(bg_color);
                free(text_color);
                free(border_color);
                break;
            }
            
            case KRYON_CMD_DRAW_CHECKBOX: {
                const KryonDrawCheckboxData* data = &cmd->data.draw_checkbox;
                
                char element_id[64];
                if (data->widget_id) {
                    strncpy(element_id, data->widget_id, sizeof(element_id) - 1);
                    element_id[sizeof(element_id) - 1] = '\0';
                } else {
                    generate_element_id(element_id, sizeof(element_id), "checkbox", ctx->element_counter++);
                }
                
                // Generate HTML checkbox with label
                fprintf(ctx->html_file, "<div class=\"checkbox-container\">\n");
                fprintf(ctx->html_file, "  <input id=\"%s\" type=\"checkbox\"", element_id);
                
                if (data->checked) {
                    fprintf(ctx->html_file, " checked");
                }
                
                if (!data->enabled) {
                    fprintf(ctx->html_file, " disabled");
                }
                
                fprintf(ctx->html_file, ">\n");
                
                if (data->label) {
                    char* sanitized_label = sanitize_string(data->label);
                    fprintf(ctx->html_file, "  <label for=\"%s\">%s</label>\n", element_id, sanitized_label);
                    free(sanitized_label);
                }
                
                fprintf(ctx->html_file, "</div>\n");
                
                // Generate CSS styles
                char* bg_color = kryon_color_to_css(data->background_color);
                char* border_color = kryon_color_to_css(data->border_color);
                char* check_color = kryon_color_to_css(data->check_color);
                
                fprintf(ctx->css_file, ".checkbox-container {\n");
                fprintf(ctx->css_file, "  position: absolute;\n");
                fprintf(ctx->css_file, "  left: %.1fpx;\n", data->position.x);
                fprintf(ctx->css_file, "  top: %.1fpx;\n", data->position.y);
                fprintf(ctx->css_file, "  width: %.1fpx;\n", data->size.x);
                fprintf(ctx->css_file, "  height: %.1fpx;\n", data->size.y);
                fprintf(ctx->css_file, "  display: flex;\n");
                fprintf(ctx->css_file, "  align-items: center;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                fprintf(ctx->css_file, "#%s {\n", element_id);
                fprintf(ctx->css_file, "  width: 16px;\n");
                fprintf(ctx->css_file, "  height: 16px;\n");
                fprintf(ctx->css_file, "  margin-right: 8px;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                fprintf(ctx->css_file, "#%s + label {\n", element_id);
                fprintf(ctx->css_file, "  font-size: 14px;\n");
                fprintf(ctx->css_file, "  cursor: pointer;\n");
                fprintf(ctx->css_file, "}\n\n");
                
                free(bg_color);
                free(border_color);
                free(check_color);
                break;
            }
            
            case KRYON_CMD_UPDATE_WIDGET_STATE: {
                // Widget state updates would be handled by JavaScript in the browser
                // This is a placeholder for state synchronization
                break;
            }
            
            default:
                // Unsupported command, skip
                break;
        }
    }
    
    return KRYON_RENDER_SUCCESS;
}

static KryonRenderResult html_resize(KryonVec2 new_size) {
    if (!g_html_impl.initialized) {
        return KRYON_RENDER_ERROR_BACKEND_INIT_FAILED;
    }
    
    g_html_impl.width = (int)new_size.x;
    g_html_impl.height = (int)new_size.y;
    
    return KRYON_RENDER_SUCCESS;
}

static KryonVec2 html_viewport_size(void) {
    return (KryonVec2){(float)g_html_impl.width, (float)g_html_impl.height};
}

static void html_destroy(void) {
    if (g_html_impl.initialized) {
        free(g_html_impl.output_path);
        free(g_html_impl.title);
        g_html_impl.initialized = false;
        printf("HTML Renderer destroyed\n");
    }
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static void write_html_header(FILE* file, const char* title, int width, int height) {
    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html lang=\"en\">\n");
    fprintf(file, "<head>\n");
    fprintf(file, "    <meta charset=\"UTF-8\">\n");
    fprintf(file, "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(file, "    <title>%s</title>\n", title);
    fprintf(file, "    <link rel=\"stylesheet\" href=\"%s.css\">\n", g_html_impl.output_path);
    fprintf(file, "</head>\n");
    fprintf(file, "<body>\n");
}

static void write_html_footer(FILE* file) {
    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");
}

static void write_css_reset(FILE* file) {
    fprintf(file, "/* CSS Reset */\n");
    fprintf(file, "* {\n");
    fprintf(file, "  margin: 0;\n");
    fprintf(file, "  padding: 0;\n");
    fprintf(file, "  box-sizing: border-box;\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "html, body {\n");
    fprintf(file, "  margin: 0;\n");
    fprintf(file, "  padding: 0;\n");
    fprintf(file, "  font-family: Arial, sans-serif;\n");
    fprintf(file, "}\n\n");
}

static char* kryon_color_to_css(KryonColor color) {
    char* result = malloc(32);
    if (!result) return NULL;
    
    if (color.a < 1.0f) {
        snprintf(result, 32, "rgba(%d, %d, %d, %.2f)",
                (int)(color.r * 255), (int)(color.g * 255), 
                (int)(color.b * 255), color.a);
    } else {
        snprintf(result, 32, "rgb(%d, %d, %d)",
                (int)(color.r * 255), (int)(color.g * 255), (int)(color.b * 255));
    }
    
    return result;
}

static char* sanitize_string(const char* str) {
    if (!str) return strdup("");
    
    size_t len = strlen(str);
    char* result = malloc(len * 6 + 1); // Worst case: every char becomes entity
    if (!result) return strdup("");
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '<':
                strcpy(&result[j], "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(&result[j], "&gt;");
                j += 4;
                break;
            case '&':
                strcpy(&result[j], "&amp;");
                j += 5;
                break;
            case '"':
                strcpy(&result[j], "&quot;");
                j += 6;
                break;
            case '\'':
                strcpy(&result[j], "&#39;");
                j += 5;
                break;
            default:
                result[j++] = str[i];
                break;
        }
    }
    result[j] = '\0';
    
    return result;
}

static void generate_element_id(char* buffer, size_t size, const char* prefix, int counter) {
    snprintf(buffer, size, "kryon_%s_%d", prefix, counter);
}

// =============================================================================
// INPUT HANDLING IMPLEMENTATION
// =============================================================================

static KryonRenderResult html_get_input_state(KryonInputState* input_state) {
    if (!input_state) return KRYON_RENDER_ERROR_INVALID_PARAM;
    
    // HTML renderer doesn't have direct access to input state since it generates static files
    // Input handling would be managed by JavaScript in the browser
    memset(input_state, 0, sizeof(KryonInputState));
    
    // Return success but with empty state - actual input would be handled by browser
    return KRYON_RENDER_SUCCESS;
}

static bool html_point_in_widget(KryonVec2 point, KryonRect widget_bounds) {
    // For HTML renderer, hit testing would be handled by the browser's DOM
    // This function provides the same logic for consistency with other renderers
    return point.x >= widget_bounds.x && 
           point.x <= widget_bounds.x + widget_bounds.width &&
           point.y >= widget_bounds.y && 
           point.y <= widget_bounds.y + widget_bounds.height;
}

// =============================================================================
// EVENT HANDLING IMPLEMENTATION
// =============================================================================

static bool html_handle_event(const KryonEvent* event) {
    if (!event || !g_html_impl.initialized) return false;
    
    // HTML renderer generates static files, so event handling would be done
    // in JavaScript in the browser. This is a placeholder implementation.
    switch (event->type) {
        case KRYON_EVENT_WINDOW_RESIZE:
            g_html_impl.width = event->data.windowResize.width;
            g_html_impl.height = event->data.windowResize.height;
            return true;
            
        default:
            return false;
    }
}

static void* html_get_native_window(void) {
    // HTML renderer has no native window
    return NULL;
}