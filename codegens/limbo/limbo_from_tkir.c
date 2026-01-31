/**
 * @file limbo_from_tkir.c
 * @brief Limbo TKIR Emitter
 *
 * Generates Limbo source code from TKIR (Toolkit Intermediate Representation).
 * This is a complete rewrite of the widget generation logic using TKIR.
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L

#include "limbo_codegen.h"
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
#include <ctype.h>

/* ============================================================================
 * Emitter Context
 * ============================================================================ */

/**
 * Limbo Emitter Context.
 * Extends the generic TKIREmitterContext with Limbo-specific data.
 */
typedef struct {
    TKIREmitterContext base;          /**< Base context */
    char* buffer;                     /**< Output buffer */
    size_t size;                      /**< Current buffer size */
    size_t capacity;                  /**< Buffer capacity */
    bool include_comments;            /**< Add comments to generated code */
    bool generate_types;              /**< Generate type definitions */
    const char* module_name;          /**< Module name */
    int indent;                       /**< Current indentation level */
    char* root_background;            /**< Track root background color */
} LimboEmitterContext;

/* ============================================================================
 * Buffer Management
 * ============================================================================ */

static bool append_string(char** buffer, size_t* size, size_t* capacity, const char* str) {
    size_t len = strlen(str);
    if (*size + len >= *capacity) {
        *capacity *= 2;
        char* new_buffer = realloc(*buffer, *capacity);
        if (!new_buffer) return false;
        *buffer = new_buffer;
    }
    strcpy(*buffer + *size, str);
    *size += len;
    return true;
}

static bool append_fmt(char** buffer, size_t* size, size_t* capacity, const char* fmt, ...) {
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    return append_string(buffer, size, capacity, temp);
}

static void append_indent(LimboEmitterContext* ctx) {
    for (int i = 0; i < ctx->indent; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }
}

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static bool limbo_emit_widget(TKIREmitterContext* ctx, TKIRWidget* widget);
static bool limbo_emit_handler(TKIREmitterContext* ctx, TKIRHandler* handler);
static bool limbo_emit_layout(TKIREmitterContext* ctx, TKIRWidget* widget);
static bool limbo_emit_property(TKIREmitterContext* ctx, const char* widget_id,
                                 const char* property_name, cJSON* value);
static char* limbo_emit_full(TKIREmitterContext* ctx, TKIRRoot* root);
static void limbo_free_context(TKIREmitterContext* ctx);

/* ============================================================================
 * Virtual Function Table
 * ============================================================================ */

static const TKIREmitterVTable limbo_emitter_vtable = {
    .emit_widget = limbo_emit_widget,
    .emit_handler = limbo_emit_handler,
    .emit_layout = limbo_emit_layout,
    .emit_property = limbo_emit_property,
    .emit_full = limbo_emit_full,
    .free_context = limbo_free_context,
};

/* ============================================================================
 * Context Creation
 * ============================================================================ */

/**
 * Create a Limbo emitter context.
 */
TKIREmitterContext* limbo_emitter_create(const TKIREmitterOptions* options) {
    LimboEmitterContext* ctx = calloc(1, sizeof(LimboEmitterContext));
    if (!ctx) {
        return NULL;
    }

    // Initialize base context
    ctx->base.vtable = &limbo_emitter_vtable;

    // Initialize buffer
    ctx->capacity = 8192;
    ctx->size = 0;
    ctx->buffer = malloc(ctx->capacity);
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }
    ctx->buffer[0] = '\0';

    if (options) {
        ctx->base.options = *options;
        ctx->include_comments = options->include_comments;
    } else {
        ctx->base.options.include_comments = true;
        ctx->base.options.verbose = false;
        ctx->base.options.indent_string = "\t";
        ctx->base.options.indent_level = 0;
        ctx->include_comments = true;
    }

    ctx->generate_types = true;
    ctx->module_name = NULL;
    ctx->indent = 0;
    ctx->root_background = NULL;

    return (TKIREmitterContext*)ctx;
}

/* ============================================================================
 * Context Free
 * ============================================================================ */

static void limbo_free_context(TKIREmitterContext* base_ctx) {
    if (!base_ctx) return;

    LimboEmitterContext* ctx = (LimboEmitterContext*)base_ctx;

    if (ctx->buffer) {
        free(ctx->buffer);
    }

    if (ctx->root_background) {
        free(ctx->root_background);
    }

    free(ctx);
}

/* ============================================================================
 * Full Output Generation
 * ============================================================================ */

static char* limbo_emit_full(TKIREmitterContext* base_ctx, TKIRRoot* root) {
    if (!base_ctx || !root) {
        return NULL;
    }

    LimboEmitterContext* ctx = (LimboEmitterContext*)base_ctx;

    // Add header comment
    if (ctx->include_comments) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "# Generated by Kryon TKIR -> Limbo Emitter\n");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "# Window: %s (%dx%d)\n\n", root->title, root->width, root->height);
    }

    // Generate module declaration
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "implement %s;\n\n",
               ctx->module_name ? ctx->module_name : "KryonApp");

    // Include Draw module
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
               "include \"draw.m\";\n\n");

    // Generate type definitions
    if (ctx->generate_types) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "Widget := adt {\n");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "\tref Draw->Context;\n");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "\tref Draw->Image;\n");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "\tref Draw->Screen;\n");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "\tref Widget;\n");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "};\n\n");
    }

    // Generate widgets
    cJSON* widgets_array = root->widgets;
    if (widgets_array && cJSON_IsArray(widgets_array)) {
        if (ctx->include_comments) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                       "# Widget Creation\n");
        }

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
            limbo_emit_widget(base_ctx, &widget);
        }

        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");
    }

    // Generate handlers
    cJSON* handlers_array = root->handlers;
    if (handlers_array && cJSON_IsArray(handlers_array)) {
        if (ctx->include_comments) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                       "# Event Handlers\n");
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
            limbo_emit_handler(base_ctx, &handler);
        }

        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");
    }

    // Return the buffer (caller must free)
    char* result = strdup(ctx->buffer);
    return result;
}

/* ============================================================================
 * Widget Emission
 * ============================================================================ */

static bool limbo_emit_widget(TKIREmitterContext* base_ctx, TKIRWidget* widget) {
    if (!base_ctx || !widget || !widget->id) {
        return false;
    }

    LimboEmitterContext* ctx = (LimboEmitterContext*)base_ctx;

    // Get properties
    cJSON* props = cJSON_GetObjectItem(widget->json, "properties");

    // Generate widget variable
    if (ctx->include_comments) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "# Widget: %s (%s)\n", widget->id, widget->tk_type);
    }

    append_indent(ctx);
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
               "%s: ref Widget;\n", widget->id);

    // Generate widget creation
    append_indent(ctx);
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
               "%s = load_widget%s(ctx", widget->id, widget->tk_type);

    // Add properties
    if (props) {
        // Text
        cJSON* text = cJSON_GetObjectItem(props, "text");
        if (text && cJSON_IsString(text)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                       ", \"%s\"", text->valuestring);
        }

        // Background
        cJSON* background = cJSON_GetObjectItem(props, "background");
        if (background && cJSON_IsString(background)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                       ", %s", background->valuestring);
        }

        // Width
        cJSON* width = cJSON_GetObjectItem(props, "width");
        if (width && cJSON_IsObject(width)) {
            cJSON* value = cJSON_GetObjectItem(width, "value");
            if (value && cJSON_IsNumber(value)) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                           ", %d", (int)value->valuedouble);
            }
        }

        // Height
        cJSON* height = cJSON_GetObjectItem(props, "height");
        if (height && cJSON_IsObject(height)) {
            cJSON* value = cJSON_GetObjectItem(height, "value");
            if (value && cJSON_IsNumber(value)) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                           ", %d", (int)value->valuedouble);
            }
        }
    }

    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, ");\n");

    // Generate layout
    limbo_emit_layout(base_ctx, widget);

    return true;
}

/* ============================================================================
 * Layout Emission
 * ============================================================================ */

static bool limbo_emit_layout(TKIREmitterContext* base_ctx, TKIRWidget* widget) {
    if (!base_ctx || !widget || !widget->json) {
        return false;
    }

    LimboEmitterContext* ctx = (LimboEmitterContext*)base_ctx;

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

    append_indent(ctx);

    if (strcmp(layout_type, "pack") == 0) {
        // Pack layout
        cJSON* side = cJSON_GetObjectItem(options, "side");
        if (side && cJSON_IsString(side)) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "pack_");
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, side->valuestring);
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "(widget-%s", widget->id);

            cJSON* padx = cJSON_GetObjectItem(options, "padx");
            cJSON* pady = cJSON_GetObjectItem(options, "pady");
            if ((padx && cJSON_IsNumber(padx)) || (pady && cJSON_IsNumber(pady))) {
                int px = padx && cJSON_IsNumber(padx) ? padx->valueint : 0;
                int py = pady && cJSON_IsNumber(pady) ? pady->valueint : 0;
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, ", %d, %d", px, py);
            }

            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ");\n");
        }

    } else if (strcmp(layout_type, "grid") == 0) {
        // Grid layout
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "grid(widget-");
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, widget->id);

        cJSON* row = cJSON_GetObjectItem(options, "row");
        cJSON* column = cJSON_GetObjectItem(options, "column");
        if (row && cJSON_IsNumber(row)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, ", %d", row->valueint);
        }
        if (column && cJSON_IsNumber(column)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, ", %d", column->valueint);
        }

        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ");\n");

    } else if (strcmp(layout_type, "place") == 0) {
        // Place layout (absolute positioning)
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "place(widget-");
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, widget->id);

        cJSON* x = cJSON_GetObjectItem(options, "x");
        cJSON* y = cJSON_GetObjectItem(options, "y");
        if (x && cJSON_IsNumber(x)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, ", %d", x->valueint);
        }
        if (y && cJSON_IsNumber(y)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, ", %d", y->valueint);
        }

        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ");\n");
    }

    return true;
}

/* ============================================================================
 * Handler Emission
 * ============================================================================ */

static bool limbo_emit_handler(TKIREmitterContext* base_ctx, TKIRHandler* handler) {
    if (!base_ctx || !handler || !handler->id) {
        return false;
    }

    LimboEmitterContext* ctx = (LimboEmitterContext*)base_ctx;

    // Get implementations
    cJSON* impls = cJSON_GetObjectItem(handler->json, "implementations");
    if (!impls) {
        return false;
    }

    cJSON* limbo_impl = cJSON_GetObjectItem(impls, "limbo");
    if (!limbo_impl || !cJSON_IsString(limbo_impl)) {
        // No Limbo implementation - skip
        return true;
    }

    // Output handler code
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, limbo_impl->valuestring);
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");

    return true;
}

/* ============================================================================
 * Property Emission
 * ============================================================================ */

static bool limbo_emit_property(TKIREmitterContext* base_ctx, const char* widget_id,
                                 const char* property_name, cJSON* value) {
    if (!base_ctx || !widget_id || !property_name || !value) {
        return false;
    }

    LimboEmitterContext* ctx = (LimboEmitterContext*)base_ctx;

    append_indent(ctx);

    if (cJSON_IsString(value)) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "%s.%s = \"%s\";\n", widget_id, property_name, value->valuestring);
    } else if (cJSON_IsNumber(value)) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                   "%s.%s = %d;\n", widget_id, property_name, value->valueint);
    }

    return true;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * Generate Limbo code from TKIR.
 * This is the main entry point for the Limbo TKIR emitter.
 *
 * @param tkir_json TKIR JSON string
 * @param options Emitter options (can be NULL for defaults)
 * @return Generated Limbo code (caller must free), or NULL on error
 */
char* limbo_codegen_from_tkir(const char* tkir_json, LimboCodegenOptions* options) {
    if (!tkir_json) {
        return NULL;
    }

    // Parse TKIR JSON
    cJSON* tkir_root = codegen_parse_kir_json(tkir_json);
    if (!tkir_root) {
        return NULL;
    }

    // Create TKIRRoot from parsed JSON
    TKIRRoot* root = tkir_root_from_cJSON(tkir_root);
    if (!root) {
        cJSON_Delete(tkir_root);
        return NULL;
    }

    // Create emitter options
    TKIREmitterOptions emitter_opts = {0};
    if (options) {
        emitter_opts.include_comments = options->include_comments;
        emitter_opts.verbose = false;
    }

    // Create emitter context
    TKIREmitterContext* ctx = limbo_emitter_create(&emitter_opts);
    if (!ctx) {
        tkir_root_free(root);
        cJSON_Delete(tkir_root);
        return NULL;
    }

    // Set module name if provided
    if (options && options->module_name) {
        LimboEmitterContext* limbo_ctx = (LimboEmitterContext*)ctx;
        limbo_ctx->module_name = options->module_name;
    }

    // Generate output
    char* output = tkir_emit(ctx, root);

    // Cleanup
    tkir_emitter_context_free(ctx);
    tkir_root_free(root);
    cJSON_Delete(tkir_root);

    return output;
}

/**
 * Generate Limbo code from TKIR file.
 *
 * @param tkir_path Path to TKIR JSON file
 * @param output_path Path to output .b file
 * @param options Codegen options (can be NULL for defaults)
 * @return true on success, false on error
 */
bool limbo_codegen_from_tkir_file(const char* tkir_path, const char* output_path,
                                   LimboCodegenOptions* options) {
    if (!tkir_path || !output_path) {
        return false;
    }

    // Read TKIR file
    size_t size;
    char* tkir_json = codegen_read_kir_file(tkir_path, &size);
    if (!tkir_json) {
        return false;
    }

    // Generate Limbo code
    char* output = limbo_codegen_from_tkir(tkir_json, options);
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
 * Initialize the Limbo emitter.
 * Registers the emitter with the TKIR emitter registry.
 * Call this during initialization.
 */
void limbo_tkir_emitter_init(void) {
    tkir_emitter_register(TKIR_EMITTER_LIMBO, &limbo_emitter_vtable);
}

/**
 * Cleanup the Limbo emitter.
 * Unregisters the emitter from the TKIR emitter registry.
 */
void limbo_tkir_emitter_cleanup(void) {
    tkir_emitter_unregister(TKIR_EMITTER_LIMBO);
}
