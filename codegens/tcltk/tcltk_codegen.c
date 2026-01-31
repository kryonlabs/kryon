/**
 * Tcl/Tk Code Generator
 * Generates Tcl/Tk scripts from KIR JSON
 */

#define _POSIX_C_SOURCE 200809L

#include "tcltk_codegen.h"
#include "../codegen_common.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* ============================================================================
 * Context Structure
 * ============================================================================ */

/**
 * Context for Tcl/Tk generation
 */
typedef struct {
    StringBuilder* sb;              // String builder for output
    bool include_comments;          // Add comments to generated code
    bool use_ttk_widgets;           // Use themed ttk widgets
    bool generate_main;             // Generate main event loop
    const char* window_title;       // Window title
    int window_width;               // Window width
    int window_height;              // Window height
    int widget_counter;             // For generating unique widget names

    // Handler deduplication tracking (using shared utility from codegen_common.h)
    CodegenHandlerTracker* handler_tracker;
} TclTkContext;

/* ============================================================================
 * Context Management
 * ============================================================================ */

/**
 * Create Tcl/Tk context
 */
static TclTkContext* create_tcltk_context(TclTkCodegenOptions* options) {
    TclTkContext* ctx = calloc(1, sizeof(TclTkContext));
    if (!ctx) return NULL;

    ctx->sb = sb_create(8192);
    if (!ctx->sb) {
        free(ctx);
        return NULL;
    }

    if (options) {
        ctx->include_comments = options->include_comments;
        ctx->use_ttk_widgets = options->use_ttk_widgets;
        ctx->generate_main = options->generate_main;
        ctx->window_title = options->window_title ? options->window_title : "Kryon App";
        ctx->window_width = options->window_width > 0 ? options->window_width : 800;
        ctx->window_height = options->window_height > 0 ? options->window_height : 600;
    } else {
        ctx->include_comments = true;
        ctx->use_ttk_widgets = true;
        ctx->generate_main = true;
        ctx->window_title = "Kryon App";
        ctx->window_width = 800;
        ctx->window_height = 600;
    }

    ctx->widget_counter = 0;

    // Create handler tracker (using shared utility)
    ctx->handler_tracker = codegen_handler_tracker_create();
    if (!ctx->handler_tracker) {
        sb_free(ctx->sb);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * Free Tcl/Tk context
 */
static void destroy_tcltk_context(TclTkContext* ctx) {
    if (!ctx) return;

    // Free handler tracking (using shared utility)
    if (ctx->handler_tracker) {
        codegen_handler_tracker_free(ctx->handler_tracker);
    }

    if (ctx->sb) {
        sb_free(ctx->sb);
    }

    free(ctx);
}

/**
 * Check if handler was already generated (using shared utility)
 */
static bool is_handler_already_generated(TclTkContext* ctx, const char* handler_name) {
    if (!ctx || !ctx->handler_tracker || !handler_name) return false;
    return codegen_handler_tracker_contains(ctx->handler_tracker, handler_name);
}

/**
 * Mark handler as generated (using shared utility)
 */
static void mark_handler_generated(TclTkContext* ctx, const char* handler_name) {
    if (!ctx || !ctx->handler_tracker || !handler_name) return;
    codegen_handler_tracker_mark(ctx->handler_tracker, handler_name);
}

/**
 * Get property value from component with alias support
 * Tries multiple property name aliases and returns the first match
 *
 * @param component cJSON component object
 * @param aliases NULL-terminated array of property name aliases (e.g., {"color", "textColor", "foregroundColor"})
 * @return cJSON* property value, or NULL if none found
 */
static cJSON* get_property_with_aliases(cJSON* component, const char** aliases) {
    if (!component || !aliases) return NULL;

    for (int i = 0; aliases[i] != NULL; i++) {
        cJSON* value = cJSON_GetObjectItem(component, aliases[i]);
        if (value) {
            return value;
        }
    }

    return NULL;
}

/* ============================================================================
 * Widget Type Mapping (Phase 2)
 * ============================================================================ */

// Note: codegen_map_kir_to_tk_widget() now uses codegen_map_kir_to_tk_widget() from codegen_common.h
// This provides consistent widget mapping across all Tk-based codegens

/* ============================================================================
 * Widget Path Generation (Phase 2)
 * ============================================================================ */

/**
 * Generate unique widget path
 */
static void generate_widget_path(TclTkContext* ctx, char* buffer, size_t size,
                                 const char* parent_path) {
    ctx->widget_counter++;

    // If parent is "." (root window), start with ".", otherwise append to parent
    if (parent_path && strcmp(parent_path, ".") == 0) {
        snprintf(buffer, size, ".w%d", ctx->widget_counter);
    } else if (parent_path && parent_path[0]) {
        snprintf(buffer, size, "%s.w%d", parent_path, ctx->widget_counter);
    } else {
        snprintf(buffer, size, ".w%d", ctx->widget_counter);
    }
}

/* ============================================================================
 * Property Generation (Phase 2)
 * ============================================================================ */

/**
 * Generate widget properties from KIR component
 */
static void generate_widget_properties(TclTkContext* ctx, cJSON* component) {
    if (!component) return;

    // Text content
    cJSON* text = cJSON_GetObjectItem(component, "text");
    if (text && cJSON_IsString(text)) {
        sb_append_fmt(ctx->sb, " -text {%s}", text->valuestring);
    }

    // Width & Height
    // Note: For buttons and labels, -width/-height are in CHARACTERS, not pixels
    // For containers (frame), we can use pixel sizes
    cJSON* type = cJSON_GetObjectItem(component, "type");
    const char* widget_type = type && cJSON_IsString(type) ? type->valuestring : "";
    const char* tk_widget = codegen_map_kir_to_tk_widget(widget_type);

    bool is_text_widget = (strcmp(tk_widget, "button") == 0 ||
                           strcmp(tk_widget, "label") == 0);

    cJSON* width = cJSON_GetObjectItem(component, "width");
    if (width && !is_text_widget) {
        // Only set width for non-text widgets (containers use pixels)
        if (cJSON_IsString(width)) {
            float w = codegen_parse_px_value(width->valuestring);
            if (w > 0 && !strstr(width->valuestring, "%")) {
                sb_append_fmt(ctx->sb, " -width %d", (int)w);
            }
        } else if (cJSON_IsNumber(width)) {
            sb_append_fmt(ctx->sb, " -width %d", width->valueint);
        }
    }

    // Height
    cJSON* height = cJSON_GetObjectItem(component, "height");
    if (height && !is_text_widget) {
        // Only set height for non-text widgets (containers use pixels)
        if (cJSON_IsString(height)) {
            float h = codegen_parse_px_value(height->valuestring);
            if (h > 0 && !strstr(height->valuestring, "%")) {
                sb_append_fmt(ctx->sb, " -height %d", (int)h);
            }
        } else if (cJSON_IsNumber(height)) {
            sb_append_fmt(ctx->sb, " -height %d", height->valueint);
        }
    }

    // Background color (with unified alias support)
    const char* bg_aliases[] = {"background", "backgroundColor", NULL};
    cJSON* background = get_property_with_aliases(component, bg_aliases);
    if (background && cJSON_IsString(background)) {
        bool is_transparent = codegen_is_transparent_color(background->valuestring);
        fprintf(stderr, "[DEBUG] background=%s, is_transparent=%d, tk_widget=%s\n",
                background->valuestring, is_transparent, tk_widget);
        if (!is_transparent) {
            uint8_t r, g, b, a;
            if (codegen_parse_color_rgba(background->valuestring, &r, &g, &b, &a)) {
                const char* hex = codegen_format_rgba_hex(r, g, b, a, false);
                // For labels, set background to allow proper rendering
                if (strcmp(tk_widget, "label") == 0) {
                    sb_append_fmt(ctx->sb, " -background {%s}", hex);
                } else {
                    sb_append_fmt(ctx->sb, " -background {%s}", hex);
                }
            }
        } else {
            // For labels with transparent background, explicitly set to empty string
            // This allows the parent's background to show through
            if (strcmp(tk_widget, "label") == 0) {
                sb_append(ctx->sb, " -background {}");
            }
        }
    }

    // Foreground color (text color) - with unified alias support
    const char* fg_aliases[] = {"color", "textColor", "foregroundColor", NULL};
    cJSON* color = get_property_with_aliases(component, fg_aliases);
    if (color && cJSON_IsString(color)) {
        // Get widget type to check if -foreground is supported
        cJSON* type = cJSON_GetObjectItem(component, "type");
        const char* widget_type = type && cJSON_IsString(type) ? type->valuestring : "";
        const char* tk_widget = codegen_map_kir_to_tk_widget(widget_type);

        // -foreground is supported by: label, button, entry, checkbutton, radiobutton,
        // scale, listbox, menu, but NOT by: frame, canvas, toplevel
        bool supports_foreground = (strcmp(tk_widget, "frame") != 0 &&
                                   strcmp(tk_widget, "canvas") != 0);

        if (supports_foreground) {
            uint8_t r, g, b, a;
            if (codegen_parse_color_rgba(color->valuestring, &r, &g, &b, &a)) {
                const char* hex = codegen_format_rgba_hex(r, g, b, a, false);
                sb_append_fmt(ctx->sb, " -foreground {%s}", hex);
            }
        }
    }

    // Font
    cJSON* font = cJSON_GetObjectItem(component, "font");
    if (font && cJSON_IsString(font)) {
        sb_append_fmt(ctx->sb, " -font {%s}", font->valuestring);
    }

    // Border
    cJSON* border = cJSON_GetObjectItem(component, "border");
    if (border && cJSON_IsObject(border)) {
        cJSON* border_width = cJSON_GetObjectItem(border, "width");
        if (border_width && cJSON_IsNumber(border_width)) {
            sb_append_fmt(ctx->sb, " -borderwidth %d", border_width->valueint);
        }
        cJSON* border_color = cJSON_GetObjectItem(border, "color");
        if (border_color && cJSON_IsString(border_color)) {
            // Border color handled through -background in Tk
        }
    }

    // For Text/Label widgets, add padding
    // Use the 'type' variable already declared above
    if (type && cJSON_IsString(type)) {
        if (strcmp(type->valuestring, "Text") == 0) {
            sb_append(ctx->sb, " -padx 5 -pady 5");
        }
    }
}

/* ============================================================================
 * Layout Managers (Phase 3)
 * ============================================================================ */

/**
 * Generate pack layout command
 */
static void generate_pack_command(TclTkContext* ctx, const char* widget_path,
                                  const char* parent_type) {
    sb_append_fmt(ctx->sb, "pack %s", widget_path);

    if (strcmp(parent_type, "Row") == 0) {
        sb_append(ctx->sb, " -side left");
    } else if (strcmp(parent_type, "Column") == 0) {
        sb_append(ctx->sb, " -side top");
    } else if (strcmp(parent_type, "Center") == 0) {
        sb_append(ctx->sb, " -expand yes -anchor center");
    } else {
        sb_append(ctx->sb, " -fill both -expand yes");
    }

    sb_append(ctx->sb, "\n");
}

/**
 * Generate grid layout command
 */
static void generate_grid_command(TclTkContext* ctx, const char* widget_path,
                                  cJSON* component) {
    cJSON* row = cJSON_GetObjectItem(component, "row");
    cJSON* col = cJSON_GetObjectItem(component, "column");

    if (row || col) {
        sb_append_fmt(ctx->sb, "grid %s", widget_path);
        if (row && cJSON_IsNumber(row)) {
            sb_append_fmt(ctx->sb, " -row %d", row->valueint);
        }
        if (col && cJSON_IsNumber(col)) {
            sb_append_fmt(ctx->sb, " -column %d", col->valueint);
        }
        sb_append(ctx->sb, "\n");
    }
}

/**
 * Generate place layout command (absolute positioning)
 */
static void generate_place_command(TclTkContext* ctx, const char* widget_path,
                                   cJSON* component) {
    cJSON* left = cJSON_GetObjectItem(component, "left");
    cJSON* top = cJSON_GetObjectItem(component, "top");

    if (left && top) {
        sb_append_fmt(ctx->sb, "place %s", widget_path);
        if (cJSON_IsNumber(left)) {
            sb_append_fmt(ctx->sb, " -x %d", left->valueint);
        }
        if (cJSON_IsNumber(top)) {
            sb_append_fmt(ctx->sb, " -y %d", top->valueint);
        }
        sb_append(ctx->sb, "\n");
    }
}

/* ============================================================================
 * Widget Command Generation (Phase 2)
 * ============================================================================ */

/**
 * Check if component is a layout container
 */
static bool is_layout_container(const char* type) {
    if (!type) return false;
    return strcmp(type, "Row") == 0 ||
           strcmp(type, "Column") == 0 ||
           strcmp(type, "Container") == 0 ||
           strcmp(type, "Center") == 0 ||
           strcmp(type, "Canvas") == 0 ||
           strcmp(type, "Table") == 0 ||
           strcmp(type, "Tabs") == 0;
}

// Forward declarations
static bool process_component(TclTkContext* ctx, cJSON* component,
                              const char* parent_path, const char* parent_type);

/**
 * Generate widget creation command
 */
static bool generate_widget(TclTkContext* ctx, cJSON* component,
                           const char* parent_path, const char* parent_type) {
    if (!ctx || !component) return false;

    const char* ir_type = codegen_get_component_type(component);
    const char* tk_widget = codegen_map_kir_to_tk_widget(ir_type);

    char widget_path[256];
    generate_widget_path(ctx, widget_path, sizeof(widget_path), parent_path);

    // Add comment if enabled
    if (ctx->include_comments) {
        sb_append_fmt(ctx->sb, "\n# %s widget: %s\n", ir_type, widget_path);
    }

    // Generate widget command
    sb_append_fmt(ctx->sb, "%s %s", tk_widget, widget_path);

    // Add properties
    generate_widget_properties(ctx, component);

    sb_append(ctx->sb, "\n");

    // Check for positioning properties
    cJSON* left = cJSON_GetObjectItem(component, "left");
    cJSON* top = cJSON_GetObjectItem(component, "top");
    cJSON* row = cJSON_GetObjectItem(component, "row");

    // Apply layout
    if (left && top && cJSON_IsNumber(left) && cJSON_IsNumber(top)) {
        // Absolute positioning
        generate_place_command(ctx, widget_path, component);
    } else if (row) {
        // Grid layout
        generate_grid_command(ctx, widget_path, component);
    } else {
        // Pack layout (default)
        generate_pack_command(ctx, widget_path, parent_type);
    }

    // Process children if this is a container
    if (is_layout_container(ir_type)) {
        cJSON* children = cJSON_GetObjectItem(component, "children");
        if (children && cJSON_IsArray(children)) {
            cJSON* child = NULL;
            cJSON_ArrayForEach(child, children) {
                process_component(ctx, child, widget_path, ir_type);
            }
        }
    }

    return true;
}

/**
 * Process component (handles both widgets and containers)
 */
static bool process_component(TclTkContext* ctx, cJSON* component,
                              const char* parent_path, const char* parent_type) {
    if (!ctx || !component) return false;

    const char* type = codegen_get_component_type(component);

    // Check for For loop (template iteration)
    if (type && strcmp(type, "For") == 0) {
        // TODO: Implement For loop generation (Phase 4+)
        if (ctx->include_comments) {
            sb_append(ctx->sb, "\n# For loop - not yet implemented\n");
        }
        return true;
    }

    // Generate widget
    return generate_widget(ctx, component, parent_path, parent_type);
}

/* ============================================================================
 * Event Handler Generation (Phase 4)
 * ============================================================================ */

/**
 * Generate event handlers from logic_block
 */
static void generate_event_handlers(TclTkContext* ctx, cJSON* logic_block) {
    if (!ctx || !logic_block) return;

    cJSON* functions = cJSON_GetObjectItem(logic_block, "functions");
    if (!functions || !cJSON_IsArray(functions)) {
        return;
    }

    if (ctx->include_comments) {
        sb_append(ctx->sb, "\n# ============================================================================\n");
        sb_append(ctx->sb, "# Event Handlers\n");
        sb_append(ctx->sb, "# ============================================================================\n\n");
    }

    cJSON* func = NULL;
    cJSON_ArrayForEach(func, functions) {
        cJSON* name = cJSON_GetObjectItem(func, "name");
        if (!name || !cJSON_IsString(name)) continue;

        const char* handler_name = name->valuestring;

        // Skip if already generated
        if (is_handler_already_generated(ctx, handler_name)) {
            continue;
        }

        mark_handler_generated(ctx, handler_name);

        if (ctx->include_comments) {
            sb_append_fmt(ctx->sb, "# Handler: %s\n", handler_name);
        }

        sb_append_fmt(ctx->sb, "proc %s {} {\n", handler_name);

        // Look for Tcl source in sources array
        cJSON* sources = cJSON_GetObjectItem(func, "sources");
        bool has_tcl_source = false;

        if (sources && cJSON_IsArray(sources)) {
            cJSON* source = NULL;
            cJSON_ArrayForEach(source, sources) {
                cJSON* lang = cJSON_GetObjectItem(source, "language");
                if (lang && cJSON_IsString(lang) && strcmp(lang->valuestring, "tcl") == 0) {
                    cJSON* code = cJSON_GetObjectItem(source, "code");
                    if (code && cJSON_IsString(code)) {
                        sb_append(ctx->sb, code->valuestring);
                        has_tcl_source = true;
                        break;
                    }
                }
            }
        }

        if (!has_tcl_source) {
            // Generate stub
            sb_append(ctx->sb, "    # TODO: Implement handler\n");
        }

        sb_append(ctx->sb, "}\n\n");
    }
}

/* ============================================================================
 * Complete Script Generation (Phase 7)
 * ============================================================================ */

/**
 * Generate complete Tcl/Tk script
 */
static bool generate_complete_script(TclTkContext* ctx, cJSON* root) {
    if (!ctx || !root) return false;

    // 1. Shebang
    sb_append(ctx->sb, "#!/usr/bin/env wish\n");

    // 2. Header comments
    if (ctx->include_comments) {
        sb_append(ctx->sb, "\n# Generated by Kryon Tcl/Tk Codegen\n");
        sb_append_fmt(ctx->sb, "# Window: %s (%dx%d)\n",
                     ctx->window_title, ctx->window_width, ctx->window_height);
    }

    // 3. Package requires
    sb_append(ctx->sb, "\npackage require Tk\n");
    // Note: ttk (themed widgets) is included with Tk, no separate package require needed
    // if (ctx->use_ttk_widgets) {
    //     sb_append(ctx->sb, "package require ttk\n");
    // }

    // 4. Window configuration
    sb_append(ctx->sb, "\n# Window configuration\n");
    sb_append_fmt(ctx->sb, "wm title . {%s}\n", ctx->window_title);
    sb_append_fmt(ctx->sb, "wm geometry . %dx%d\n", ctx->window_width, ctx->window_height);

    // 5. Extract window configuration from KIR
    cJSON* app_config = cJSON_GetObjectItem(root, "app");
    if (app_config) {
        cJSON* title = cJSON_GetObjectItem(app_config, "windowTitle");
        if (title && cJSON_IsString(title)) {
            sb_append_fmt(ctx->sb, "wm title . {%s}\n", title->valuestring);
        }
        cJSON* width = cJSON_GetObjectItem(app_config, "windowWidth");
        cJSON* height = cJSON_GetObjectItem(app_config, "windowHeight");
        if (width && cJSON_IsNumber(width) && height && cJSON_IsNumber(height)) {
            sb_append_fmt(ctx->sb, "wm geometry . %dx%d\n", width->valueint, height->valueint);
        }
        cJSON* bg = cJSON_GetObjectItem(app_config, "background");
        if (bg && cJSON_IsString(bg)) {
            uint8_t r, g, b, a;
            if (codegen_parse_color_rgba(bg->valuestring, &r, &g, &b, &a)) {
                const char* hex = codegen_format_rgba_hex(r, g, b, a, false);
                sb_append_fmt(ctx->sb, "wm attributes . -background {%s}\n", hex);
            }
        }
    }

    // 6. Generate UI components
    sb_append(ctx->sb, "\n# UI Components\n");
    cJSON* root_component = cJSON_GetObjectItem(root, "root");
    if (root_component) {
        process_component(ctx, root_component, "", "Container");
    }

    // 7. Generate event handlers
    cJSON* logic_block = cJSON_GetObjectItem(root, "logic_block");
    if (logic_block) {
        generate_event_handlers(ctx, logic_block);
    }

    // 8. Main event loop
    if (ctx->generate_main) {
        sb_append(ctx->sb, "\n# Main event loop\n");
        sb_append(ctx->sb, "update\n");
        sb_append(ctx->sb, "focus -force .\n");
    }

    return true;
}

/* ============================================================================
 * Main API Functions
 * ============================================================================ */

/**
 * Generate Tcl/Tk script from KIR file
 */
bool tcltk_codegen_generate(const char* kir_path, const char* output_path) {
    return tcltk_codegen_generate_with_options(kir_path, output_path, NULL);
}

/**
 * Generate Tcl/Tk script from KIR JSON string
 */
char* tcltk_codegen_from_json(const char* kir_json) {
    if (!kir_json) {
        codegen_error("KIR JSON string is NULL");
        return NULL;
    }

    // Parse JSON
    cJSON* root = codegen_parse_kir_json(kir_json);
    if (!root) {
        codegen_error("Failed to parse KIR JSON");
        return NULL;
    }

    // Create context with default options
    TclTkContext* ctx = create_tcltk_context(NULL);
    if (!ctx) {
        cJSON_Delete(root);
        codegen_error("Failed to create Tcl/Tk context");
        return NULL;
    }

    // Set error prefix
    codegen_set_error_prefix("TclTk");

    // Generate complete script
    if (!generate_complete_script(ctx, root)) {
        destroy_tcltk_context(ctx);
        cJSON_Delete(root);
        codegen_error("Failed to generate Tcl/Tk script");
        return NULL;
    }

    // Get result
    char* result = sb_get(ctx->sb);

    // Cleanup
    destroy_tcltk_context(ctx);
    cJSON_Delete(root);

    return result;
}

/**
 * Generate Tcl/Tk module files from KIR
 */
bool tcltk_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    // For now, just call single-file generation
    // Multi-file support can be added later
    if (!output_dir) {
        return tcltk_codegen_generate(kir_path, "output.tcl");
    }

    // Extract project name from kir_path
    const char* filename = strrchr(kir_path, '/');
    if (!filename) filename = kir_path;
    else filename++;

    char project_name[256];
    snprintf(project_name, sizeof(project_name), "%.255s", filename);

    // Remove .kir extension if present
    char* ext = strstr(project_name, ".kir");
    if (ext) *ext = '\0';

    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/%s.tcl", output_dir, project_name);

    return tcltk_codegen_generate(kir_path, output_path);
}

/**
 * Generate Tcl/Tk script with options
 */
bool tcltk_codegen_generate_with_options(const char* kir_path,
                                         const char* output_path,
                                         TclTkCodegenOptions* options) {
    if (!kir_path) {
        codegen_error("KIR path is NULL");
        return false;
    }

    if (!output_path) {
        codegen_error("Output path is NULL");
        return false;
    }

    // Read KIR file
    size_t kir_size;
    char* kir_json = codegen_read_kir_file(kir_path, &kir_size);
    if (!kir_json) {
        codegen_error("Failed to read KIR file: %s", kir_path);
        return false;
    }

    // Parse JSON
    cJSON* root = codegen_parse_kir_json(kir_json);
    if (!root) {
        codegen_error("Failed to parse KIR JSON from: %s", kir_path);
        free(kir_json);
        return false;
    }

    // Create context
    TclTkContext* ctx = create_tcltk_context(options);
    if (!ctx) {
        cJSON_Delete(root);
        free(kir_json);
        codegen_error("Failed to create Tcl/Tk context");
        return false;
    }

    // Set error prefix
    codegen_set_error_prefix("TclTk");

    // Generate complete script
    if (!generate_complete_script(ctx, root)) {
        free(kir_json);
        destroy_tcltk_context(ctx);
        cJSON_Delete(root);
        codegen_error("Failed to generate Tcl/Tk script");
        return false;
    }

    // Get result
    char* result = sb_get(ctx->sb);

    // Write output file
    bool success = codegen_write_output_file(output_path, result);

    // Cleanup
    free(result);
    destroy_tcltk_context(ctx);
    cJSON_Delete(root);
    free(kir_json);

    return success;
}
