/**
 * @file tcltk_from_tkir.c
 * @brief Tcl/Tk TKIR Emitter
 *
 * Generates Tcl/Tk scripts from TKIR (Toolkit Intermediate Representation).
 * This is a complete rewrite of the widget generation logic using TKIR.
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L

#include "tcltk_codegen.h"
#include "../codegen_common.h"
#include "../tkir/tkir.h"
#include "../tkir/tkir_builder.h"
#include "../tkir/tkir_emitter.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* ============================================================================
 * Emitter Context
 * ============================================================================ */

/**
 * Tcl/Tk Emitter Context.
 * Extends the generic TKIREmitterContext with Tcl/Tk-specific data.
 */
typedef struct {
    TKIREmitterContext base;          /**< Base context */
    StringBuilder* sb;                /**< String builder for output */
    bool include_comments;            /**< Add comments to generated code */
    bool use_ttk_widgets;             /**< Use themed ttk widgets */
    bool generate_main;               /**< Generate main event loop */
    char* root_background;            /**< Track root background color */
} TclTKEmitterContext;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static bool tcltk_emit_widget(TKIREmitterContext* ctx, TKIRWidget* widget);
static bool tcltk_emit_handler(TKIREmitterContext* ctx, TKIRHandler* handler);
static bool tcltk_emit_layout(TKIREmitterContext* ctx, TKIRWidget* widget);
static bool tcltk_emit_property(TKIREmitterContext* ctx, const char* widget_id,
                                 const char* property_name, cJSON* value);
static char* tcltk_emit_full(TKIREmitterContext* ctx, TKIRRoot* root);
static void tcltk_free_context(TKIREmitterContext* ctx);
static char* tcltk_normalize_color(const char* color);

/* ============================================================================
 * Virtual Function Table
 * ============================================================================ */

static const TKIREmitterVTable tcltk_emitter_vtable = {
    .emit_widget = tcltk_emit_widget,
    .emit_handler = tcltk_emit_handler,
    .emit_layout = tcltk_emit_layout,
    .emit_property = tcltk_emit_property,
    .emit_full = tcltk_emit_full,
    .free_context = tcltk_free_context,
};

/* ============================================================================
 * Color Normalization
 * ============================================================================ */

/**
 * Normalize color for Tcl/Tk.
 * Converts RGBA (8-digit hex) colors to RGB (6-digit hex) by stripping alpha channel.
 * Tcl/Tk doesn't support alpha channels in color specifications.
 *
 * @param color Input color string (e.g., "#ff0000" or "#ff000080")
 * @return Normalized color string (caller must free), or NULL on error
 */
static char* tcltk_normalize_color(const char* color) {
    if (!color || !*color) {
        return NULL;
    }

    // Check if it's a hex color starting with #
    if (color[0] != '#') {
        // Not a hex color, return as-is (named color like "red", "blue", etc.)
        return strdup(color);
    }

    size_t len = strlen(color);

    // RGBA format: #RRGGBBAA (9 characters with #)
    // RGB format: #RRGGBB (7 characters with #)
    if (len == 9) {
        // Strip alpha channel (last 2 characters)
        char* normalized = malloc(8); // # + 6 hex digits + null terminator
        if (normalized) {
            strncpy(normalized, color, 7); // Copy #RRGGBB
            normalized[7] = '\0';
            return normalized;
        }
        return NULL;
    }

    // Already in correct format or unknown format, return as-is
    return strdup(color);
}

/* ============================================================================
 * Context Creation
 * ============================================================================ */

/**
 * Create a Tcl/Tk emitter context.
 */
TKIREmitterContext* tcltk_emitter_create(const TKIREmitterOptions* options) {
    TclTKEmitterContext* ctx = calloc(1, sizeof(TclTKEmitterContext));
    if (!ctx) {
        return NULL;
    }

    // Initialize base context
    ctx->base.vtable = &tcltk_emitter_vtable;

    if (options) {
        ctx->base.options = *options;
        ctx->include_comments = options->include_comments;
    } else {
        ctx->base.options.include_comments = true;
        ctx->base.options.verbose = false;
        ctx->base.options.indent_string = "  ";
        ctx->base.options.indent_level = 0;
        ctx->include_comments = true;
    }

    // Create string builder
    ctx->sb = sb_create(8192);
    if (!ctx->sb) {
        free(ctx);
        return NULL;
    }

    ctx->use_ttk_widgets = true;
    ctx->generate_main = true;
    ctx->root_background = NULL;

    return (TKIREmitterContext*)ctx;
}

/* ============================================================================
 * Context Free
 * ============================================================================ */

static void tcltk_free_context(TKIREmitterContext* base_ctx) {
    if (!base_ctx) return;

    TclTKEmitterContext* ctx = (TclTKEmitterContext*)base_ctx;

    // Free Tcl/Tk-specific resources
    if (ctx->sb) {
        sb_free(ctx->sb);
    }

    if (ctx->root_background) {
        free(ctx->root_background);
    }

    // NOTE: Don't free ctx here!
    // The tkir_emitter_context_free() function will free the base context.
    // We only need to free the Tcl/Tk-specific resources.
}

/* ============================================================================
 * Full Output Generation
 * ============================================================================ */

static char* tcltk_emit_full(TKIREmitterContext* base_ctx, TKIRRoot* root) {
    if (!base_ctx || !root) {
        return NULL;
    }

    TclTKEmitterContext* ctx = (TclTKEmitterContext*)base_ctx;

    // Add header comment
    if (ctx->include_comments) {
        sb_append_fmt(ctx->sb, "# Generated by Kryon TKIR -> Tcl/Tk Emitter\n");
        sb_append_fmt(ctx->sb, "# Window: %s (%dx%d)\n\n",
                     root->title, root->width, root->height);
    }

    // Generate window setup
    sb_append_fmt(ctx->sb, "package require Tk\n");
    sb_append_fmt(ctx->sb, "wm title . \"%s\"\n", root->title);
    sb_append_fmt(ctx->sb, "wm geometry . %dx%d\n", root->width, root->height);

    if (!root->resizable) {
        sb_append_fmt(ctx->sb, "wm resizable . 0 0\n");
    }

    if (root->background) {
        char* norm_color = tcltk_normalize_color(root->background);
        if (norm_color) {
            sb_append_fmt(ctx->sb, ". configure -background %s\n", norm_color);
            ctx->root_background = norm_color;
        }
    }

    sb_append_fmt(ctx->sb, "\n");

    // Generate all widgets
    cJSON* widgets_array = root->widgets;
    if (widgets_array && cJSON_IsArray(widgets_array)) {
        cJSON* widget_json = NULL;
        cJSON_ArrayForEach(widget_json, widgets_array) {
            // Create temporary TKIRWidget wrapper
            TKIRWidget widget = {0};
            widget.json = widget_json;

            cJSON* id = cJSON_GetObjectItem(widget_json, "id");
            widget.id = id ? id->valuestring : NULL;

            cJSON* tk_type = cJSON_GetObjectItem(widget_json, "tk_type");
            widget.tk_type = tk_type ? tk_type->valuestring : NULL;

            cJSON* kir_type = cJSON_GetObjectItem(widget_json, "kir_type");
            widget.kir_type = kir_type ? kir_type->valuestring : NULL;

            // Emit widget
            tcltk_emit_widget(base_ctx, &widget);
        }
    }

    // Generate handlers
    cJSON* handlers_array = root->handlers;
    if (handlers_array && cJSON_IsArray(handlers_array)) {
        sb_append_fmt(ctx->sb, "\n");
        if (ctx->include_comments) {
            sb_append_fmt(ctx->sb, "# Event Handlers\n");
        }

        cJSON* handler_json = NULL;
        cJSON_ArrayForEach(handler_json, handlers_array) {
            // Create temporary TKIRHandler wrapper
            TKIRHandler handler = {0};
            handler.json = handler_json;

            cJSON* id = cJSON_GetObjectItem(handler_json, "id");
            handler.id = id ? id->valuestring : NULL;

            cJSON* event_type = cJSON_GetObjectItem(handler_json, "event_type");
            handler.event_type = event_type ? event_type->valuestring : NULL;

            cJSON* widget_id = cJSON_GetObjectItem(handler_json, "widget_id");
            handler.widget_id = widget_id ? widget_id->valuestring : NULL;

            // Emit handler
            tcltk_emit_handler(base_ctx, &handler);
        }
    }

    // Generate main event loop
    if (ctx->generate_main) {
        sb_append_fmt(ctx->sb, "\n");
        if (ctx->include_comments) {
            sb_append_fmt(ctx->sb, "# Start event loop\n");
        }
        sb_append_fmt(ctx->sb, "tkwait window .\n");
    }

    // Get final output
    return sb_get(ctx->sb);
}

/* ============================================================================
 * Widget Emission
 * ============================================================================ */

static bool tcltk_emit_widget(TKIREmitterContext* base_ctx, TKIRWidget* widget) {
    if (!base_ctx || !widget || !widget->id) {
        return false;
    }

    TclTKEmitterContext* ctx = (TclTKEmitterContext*)base_ctx;

    // Get properties
    cJSON* props = cJSON_GetObjectItem(widget->json, "properties");

    // Generate widget creation command
    if (ctx->include_comments) {
        sb_append_fmt(ctx->sb, "# Widget: %s (%s)\n", widget->id, widget->tk_type);
    }

    sb_append_fmt(ctx->sb, "%s .%s\n", widget->tk_type, widget->id);

    // Generate properties
    if (props) {
        // Text
        cJSON* text = cJSON_GetObjectItem(props, "text");
        if (text && cJSON_IsString(text)) {
            sb_append_fmt(ctx->sb, ".%s configure -text {%s}\n",
                         widget->id, text->valuestring);
        }

        // Background
        cJSON* background = cJSON_GetObjectItem(props, "background");
        if (background && cJSON_IsString(background)) {
            char* norm_color = tcltk_normalize_color(background->valuestring);
            if (norm_color) {
                sb_append_fmt(ctx->sb, ".%s configure -background %s\n",
                             widget->id, norm_color);
                free(norm_color);
            }
        }

        // Foreground (only for text-displaying widgets)
        cJSON* foreground = cJSON_GetObjectItem(props, "foreground");
        if (foreground && cJSON_IsString(foreground)) {
            // Only set foreground for widgets that support text color
            bool supports_foreground = (
                strcmp(widget->tk_type, "label") == 0 ||
                strcmp(widget->tk_type, "button") == 0 ||
                strcmp(widget->tk_type, "entry") == 0 ||
                strcmp(widget->tk_type, "text") == 0 ||
                strcmp(widget->tk_type, "checkbutton") == 0 ||
                strcmp(widget->tk_type, "radiobutton") == 0 ||
                strcmp(widget->tk_type, "menubutton") == 0 ||
                strcmp(widget->tk_type, "message") == 0
            );
            if (supports_foreground) {
                char* norm_color = tcltk_normalize_color(foreground->valuestring);
                if (norm_color) {
                    sb_append_fmt(ctx->sb, ".%s configure -foreground %s\n",
                                 widget->id, norm_color);
                    free(norm_color);
                }
            }
        }

        // Font
        cJSON* font = cJSON_GetObjectItem(props, "font");
        if (font && cJSON_IsObject(font)) {
            cJSON* family = cJSON_GetObjectItem(font, "family");
            cJSON* size = cJSON_GetObjectItem(font, "size");
            if (family && cJSON_IsString(family) && size && cJSON_IsNumber(size)) {
                sb_append_fmt(ctx->sb, ".%s configure -font {%s %d}\n",
                             widget->id, family->valuestring, (int)size->valuedouble);
            }
        }

        // Width
        cJSON* width = cJSON_GetObjectItem(props, "width");
        if (width && cJSON_IsObject(width)) {
            cJSON* value = cJSON_GetObjectItem(width, "value");
            if (value && cJSON_IsNumber(value)) {
                sb_append_fmt(ctx->sb, ".%s configure -width %d\n",
                             widget->id, (int)value->valuedouble);
            }
        }

        // Height
        cJSON* height = cJSON_GetObjectItem(props, "height");
        if (height && cJSON_IsObject(height)) {
            cJSON* value = cJSON_GetObjectItem(height, "value");
            if (value && cJSON_IsNumber(value)) {
                sb_append_fmt(ctx->sb, ".%s configure -height %d\n",
                             widget->id, (int)value->valuedouble);
            }
        }

        // Border
        cJSON* border = cJSON_GetObjectItem(props, "border");
        if (border && cJSON_IsObject(border)) {
            cJSON* width_val = cJSON_GetObjectItem(border, "width");
            cJSON* color = cJSON_GetObjectItem(border, "color");
            if (width_val && cJSON_IsNumber(width_val)) {
                sb_append_fmt(ctx->sb, ".%s configure -borderwidth %d\n",
                             widget->id, (int)width_val->valuedouble);
            }
            if (color && cJSON_IsString(color)) {
                char* norm_color = tcltk_normalize_color(color->valuestring);
                if (norm_color) {
                    sb_append_fmt(ctx->sb, ".%s configure -highlightbackground %s\n",
                                 widget->id, norm_color);
                    free(norm_color);
                }
            }
        }
    }

    // Generate layout
    tcltk_emit_layout(base_ctx, widget);

    sb_append_fmt(ctx->sb, "\n");

    return true;
}

/* ============================================================================
 * Layout Emission
 * ============================================================================ */

static bool tcltk_emit_layout(TKIREmitterContext* base_ctx, TKIRWidget* widget) {
    if (!base_ctx || !widget || !widget->json) {
        return false;
    }

    TclTKEmitterContext* ctx = (TclTKEmitterContext*)base_ctx;

    cJSON* layout = cJSON_GetObjectItem(widget->json, "layout");
    if (!layout) {
        return false;  // Root widget has no layout
    }

    cJSON* type = cJSON_GetObjectItem(layout, "type");
    cJSON* options = cJSON_GetObjectItem(layout, "options");
    if (!type || !options) {
        return false;
    }

    const char* layout_type = type->valuestring;

    if (strcmp(layout_type, "pack") == 0) {
        // Pack layout
        sb_append_fmt(ctx->sb, "pack .%s", widget->id);

        cJSON* side = cJSON_GetObjectItem(options, "side");
        if (side && cJSON_IsString(side)) {
            sb_append_fmt(ctx->sb, " -side %s", side->valuestring);
        }

        cJSON* fill = cJSON_GetObjectItem(options, "fill");
        if (fill && cJSON_IsString(fill)) {
            sb_append_fmt(ctx->sb, " -fill %s", fill->valuestring);
        }

        cJSON* expand = cJSON_GetObjectItem(options, "expand");
        if (expand && cJSON_IsBool(expand) && cJSON_IsTrue(expand)) {
            sb_append_fmt(ctx->sb, " -expand yes");
        }

        cJSON* anchor = cJSON_GetObjectItem(options, "anchor");
        if (anchor && cJSON_IsString(anchor)) {
            sb_append_fmt(ctx->sb, " -anchor %s", anchor->valuestring);
        }

        cJSON* padx = cJSON_GetObjectItem(options, "padx");
        if (padx && cJSON_IsNumber(padx)) {
            sb_append_fmt(ctx->sb, " -padx %d", padx->valueint);
        }

        cJSON* pady = cJSON_GetObjectItem(options, "pady");
        if (pady && cJSON_IsNumber(pady)) {
            sb_append_fmt(ctx->sb, " -pady %d", pady->valueint);
        }

        sb_append_fmt(ctx->sb, "\n");

    } else if (strcmp(layout_type, "grid") == 0) {
        // Grid layout
        sb_append_fmt(ctx->sb, "grid .%s", widget->id);

        cJSON* row = cJSON_GetObjectItem(options, "row");
        if (row && cJSON_IsNumber(row)) {
            sb_append_fmt(ctx->sb, " -row %d", row->valueint);
        }

        cJSON* column = cJSON_GetObjectItem(options, "column");
        if (column && cJSON_IsNumber(column)) {
            sb_append_fmt(ctx->sb, " -column %d", column->valueint);
        }

        cJSON* rowspan = cJSON_GetObjectItem(options, "rowspan");
        if (rowspan && cJSON_IsNumber(rowspan) && rowspan->valueint > 1) {
            sb_append_fmt(ctx->sb, " -rowspan %d", rowspan->valueint);
        }

        cJSON* columnspan = cJSON_GetObjectItem(options, "columnspan");
        if (columnspan && cJSON_IsNumber(columnspan) && columnspan->valueint > 1) {
            sb_append_fmt(ctx->sb, " -columnspan %d", columnspan->valueint);
        }

        cJSON* sticky = cJSON_GetObjectItem(options, "sticky");
        if (sticky && cJSON_IsString(sticky) && strlen(sticky->valuestring) > 0) {
            sb_append_fmt(ctx->sb, " -sticky %s", sticky->valuestring);
        }

        sb_append_fmt(ctx->sb, "\n");

    } else if (strcmp(layout_type, "place") == 0) {
        // Place layout (absolute positioning)
        sb_append_fmt(ctx->sb, "place .%s", widget->id);

        cJSON* x = cJSON_GetObjectItem(options, "x");
        if (x && cJSON_IsNumber(x)) {
            sb_append_fmt(ctx->sb, " -x %d", x->valueint);
        }

        cJSON* y = cJSON_GetObjectItem(options, "y");
        if (y && cJSON_IsNumber(y)) {
            sb_append_fmt(ctx->sb, " -y %d", y->valueint);
        }

        cJSON* width = cJSON_GetObjectItem(options, "width");
        if (width && cJSON_IsNumber(width)) {
            sb_append_fmt(ctx->sb, " -width %d", width->valueint);
        }

        cJSON* height = cJSON_GetObjectItem(options, "height");
        if (height && cJSON_IsNumber(height)) {
            sb_append_fmt(ctx->sb, " -height %d", height->valueint);
        }

        cJSON* anchor = cJSON_GetObjectItem(options, "anchor");
        if (anchor && cJSON_IsString(anchor)) {
            sb_append_fmt(ctx->sb, " -anchor %s", anchor->valuestring);
        }

        sb_append_fmt(ctx->sb, "\n");
    }

    return true;
}

/* ============================================================================
 * Handler Emission
 * ============================================================================ */

static bool tcltk_emit_handler(TKIREmitterContext* base_ctx, TKIRHandler* handler) {
    if (!base_ctx || !handler || !handler->id) {
        return false;
    }

    TclTKEmitterContext* ctx = (TclTKEmitterContext*)base_ctx;

    // Get implementations
    cJSON* impls = cJSON_GetObjectItem(handler->json, "implementations");
    if (!impls) {
        return false;
    }

    cJSON* tcl_impl = cJSON_GetObjectItem(impls, "tcl");
    if (!tcl_impl || !cJSON_IsString(tcl_impl)) {
        // No Tcl implementation - skip
        return true;
    }

    // Output handler code
    sb_append_fmt(ctx->sb, "%s\n", tcl_impl->valuestring);

    return true;
}

/* ============================================================================
 * Property Emission
 * ============================================================================ */

static bool tcltk_emit_property(TKIREmitterContext* base_ctx, const char* widget_id,
                                 const char* property_name, cJSON* value) {
    if (!base_ctx || !widget_id || !property_name || !value) {
        return false;
    }

    TclTKEmitterContext* ctx = (TclTKEmitterContext*)base_ctx;

    // Get widget type
    // We need to look up the widget to get its type
    // For now, just generate a generic configure command
    if (cJSON_IsString(value)) {
        sb_append_fmt(ctx->sb, ".%s configure -%s {%s}\n",
                     widget_id, property_name, value->valuestring);
    } else if (cJSON_IsNumber(value)) {
        sb_append_fmt(ctx->sb, ".%s configure -%s %d\n",
                     widget_id, property_name, value->valueint);
    }

    return true;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * Generate Tcl/Tk code from TKIR.
 * This is the main entry point for the Tcl/Tk TKIR emitter.
 *
 * @param tkir_json TKIR JSON string
 * @param options Emitter options (can be NULL for defaults)
 * @return Generated Tcl/Tk code (caller must free), or NULL on error
 */
char* tcltk_codegen_from_tkir(const char* tkir_json, TclTkCodegenOptions* options) {
    if (!tkir_json) {
        return NULL;
    }

    // Parse KIR JSON
    cJSON* kir_root = codegen_parse_kir_json(tkir_json);
    if (!kir_root) {
        return NULL;
    }

    // Build TKIR from KIR (this transforms the hierarchical KIR into flat TKIR widgets array)
    TKIRRoot* root = tkir_build_from_cJSON(kir_root, false);
    if (!root) {
        cJSON_Delete(kir_root);
        return NULL;
    }

    // Create emitter options
    TKIREmitterOptions emitter_opts = {0};
    if (options) {
        emitter_opts.include_comments = options->include_comments;
        emitter_opts.verbose = false;
    }

    // Create emitter context
    TKIREmitterContext* ctx = tcltk_emitter_create(&emitter_opts);
    if (!ctx) {
        tkir_root_free(root);      // Free TKIRRoot first
        cJSON_Delete(kir_root);  // Then delete cJSON (we own it)
        return NULL;
    }

    // Generate output
    char* output = tkir_emit(ctx, root);

    // Cleanup - ORDER MATTERS!
    tkir_emitter_context_free(ctx);
    tkir_root_free(root);      // Free TKIRRoot (doesn't delete root->json)
    cJSON_Delete(kir_root);  // Delete cJSON (we own it)

    return output;
}

/**
 * Generate Tcl/Tk code from TKIR file.
 *
 * @param tkir_path Path to TKIR JSON file
 * @param output_path Path to output .tcl file
 * @param options Codegen options (can be NULL for defaults)
 * @return true on success, false on error
 */
bool tcltk_codegen_from_tkir_file(const char* tkir_path, const char* output_path,
                                   TclTkCodegenOptions* options) {
    if (!tkir_path || !output_path) {
        return false;
    }

    // Read TKIR file
    size_t size;
    char* tkir_json = codegen_read_kir_file(tkir_path, &size);
    if (!tkir_json) {
        return false;
    }

    // Generate Tcl/Tk code
    char* output = tcltk_codegen_from_tkir(tkir_json, options);
    free(tkir_json);

    if (!output) {
        return false;
    }

    // Write output file
    bool success = codegen_write_output_file(output_path, output);
    free(output);

    return success;
}

/* ============================================================================
 * Emitter Registration
 * ============================================================================ */

/**
 * Initialize the Tcl/Tk emitter.
 * Registers the emitter with the TKIR emitter registry.
 * Call this during initialization.
 */
void tcltk_tkir_emitter_init(void) {
    tkir_emitter_register(TKIR_EMITTER_TCLTK, &tcltk_emitter_vtable);
}

/**
 * Cleanup the Tcl/Tk emitter.
 * Unregisters the emitter from the TKIR emitter registry.
 */
void tcltk_tkir_emitter_cleanup(void) {
    tkir_emitter_unregister(TKIR_EMITTER_TCLTK);
}
