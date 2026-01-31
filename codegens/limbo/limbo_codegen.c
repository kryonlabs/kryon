/**
 * Limbo Code Generator
 * Generates Limbo source (.b) files from KIR JSON for Inferno/TaijiOS
 */

#define _POSIX_C_SOURCE 200809L

#include "limbo_codegen.h"
#include "../codegen_common.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>

// Helper to append string to buffer with reallocation
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

// Helper to append formatted string
static bool append_fmt(char** buffer, size_t* size, size_t* capacity, const char* fmt, ...) {
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    return append_string(buffer, size, capacity, temp);
}

// Context for Limbo generation
typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
    bool include_comments;
    bool generate_types;
    const char* module_name;
    int widget_counter;
    int event_handler_counter;
    int indent;                 // Current indentation level
    char** modules;             // Additional modules to include
    int modules_count;          // Number of additional modules
    cJSON* source_structures;   // Source structures for const resolution
    bool cmd_declared_in_loop;  // Track if cmd variable is declared in current for loop scope

    // Handler deduplication tracking (using shared utility from codegen_common.h)
    CodegenHandlerTracker* handler_tracker;
} LimboContext;

// Initialize Limbo context
static LimboContext* create_limbo_context(LimboCodegenOptions* options) {
    LimboContext* ctx = malloc(sizeof(LimboContext));
    if (!ctx) return NULL;

    ctx->capacity = 8192;
    ctx->size = 0;
    ctx->buffer = malloc(ctx->capacity);
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }
    ctx->buffer[0] = '\0';

    if (options) {
        ctx->include_comments = options->include_comments;
        ctx->generate_types = options->generate_types;
        ctx->module_name = options->module_name;
        ctx->modules = options->modules;
        ctx->modules_count = options->modules_count;
    } else {
        ctx->include_comments = true;
        ctx->generate_types = true;
        ctx->module_name = NULL;
        ctx->modules = NULL;
        ctx->modules_count = 0;
    }

    ctx->widget_counter = 0;
    ctx->event_handler_counter = 0;
    ctx->indent = 0;

    // Create handler tracker
    ctx->handler_tracker = codegen_handler_tracker_create();
    if (!ctx->handler_tracker) {
        free(ctx->buffer);
        free(ctx);
        return NULL;
    }

    ctx->cmd_declared_in_loop = false;

    return ctx;
}

// Free Limbo context
static void destroy_limbo_context(LimboContext* ctx) {
    if (!ctx) return;
    free(ctx->buffer);
    // Free handler tracking (using shared utility)
    if (ctx->handler_tracker) {
        codegen_handler_tracker_free(ctx->handler_tracker);
    }
    free(ctx);
}

// Check if handler was already generated (using shared utility)
static bool is_handler_already_generated(LimboContext* ctx, const char* handler_name) {
    if (!ctx || !ctx->handler_tracker || !handler_name) return false;
    return codegen_handler_tracker_contains(ctx->handler_tracker, handler_name);
}

// Mark handler as generated (using shared utility)
static void mark_handler_generated(LimboContext* ctx, const char* handler_name) {
    if (!ctx || !ctx->handler_tracker || !handler_name) return;
    codegen_handler_tracker_mark(ctx->handler_tracker, handler_name);
}

// Event handler storage with inline code
typedef struct {
    int handler_number;
    char* handler_name;
    char* handler_code;  // Store the actual Limbo code to inline
} EventHandlerMapping;

static EventHandlerMapping event_handlers[256];
static int event_handler_mapping_count = 0;

// Store mapping between handler number and name
static void register_event_handler(int number, const char* name) {
    if (event_handler_mapping_count >= 256 || !name) return;

    event_handlers[event_handler_mapping_count].handler_number = number;
    event_handlers[event_handler_mapping_count].handler_name = strdup(name);
    event_handlers[event_handler_mapping_count].handler_code = NULL;  // Will be set later
    event_handler_mapping_count++;
}

// Set the handler code for a given handler number
static void set_event_handler_code(int number, const char* code) {
    for (int i = 0; i < event_handler_mapping_count; i++) {
        if (event_handlers[i].handler_number == number) {
            if (event_handlers[i].handler_code) {
                free(event_handlers[i].handler_code);
            }
            event_handlers[i].handler_code = strdup(code);
            int len = strlen(code);
            fprintf(stderr, "[LIMBO] Stored handler code for number %d (len=%d): ", number, len);
            for (int j = 0; j < len; j++) {
                if (code[j] == '\n') fprintf(stderr, "\\n");
                else if (code[j] == '\r') fprintf(stderr, "\\r");
                else if (code[j] == '\t') fprintf(stderr, "\\t");
                else fprintf(stderr, "%c", code[j]);
            }
            fprintf(stderr, "\n");
            break;
        }
    }
}

// Get handler name by number
static const char* get_handler_name_by_number(int number) {
    for (int i = 0; i < event_handler_mapping_count; i++) {
        if (event_handlers[i].handler_number == number) {
            return event_handlers[i].handler_name;
        }
    }
    return NULL;
}

// Get handler code by number
static const char* get_handler_code_by_number(int number) {
    fprintf(stderr, "[LIMBO] Looking up handler code for number %d (total handlers: %d)\n", number, event_handler_mapping_count);
    for (int i = 0; i < event_handler_mapping_count; i++) {
        fprintf(stderr, "[LIMBO] Checking handler %d: number=%d, code=%p\n", i, event_handlers[i].handler_number, (void*)event_handlers[i].handler_code);
        if (event_handlers[i].handler_number == number) {
            fprintf(stderr, "[LIMBO] Found handler code: %s\n", event_handlers[i].handler_code ? event_handlers[i].handler_code : "NULL");
            return event_handlers[i].handler_code;
        }
    }
    fprintf(stderr, "[LIMBO] Handler code not found for number %d\n", number);
    return NULL;
}

// Cleanup handler mappings
static void cleanup_event_handler_mappings() {
    for (int i = 0; i < event_handler_mapping_count; i++) {
        if (event_handlers[i].handler_name) {
            free(event_handlers[i].handler_name);
        }
        if (event_handlers[i].handler_code) {
            free(event_handlers[i].handler_code);
        }
    }
    event_handler_mapping_count = 0;
}

// Extract module name from filename
static char* extract_module_name(const char* filename) {
    if (!filename) return strdup("KryonApp");

    // Find the last '/' or '\\'
    const char* base = strrchr(filename, '/');
    if (!base) base = strrchr(filename, '\\');
    if (base) base++;
    else base = filename;

    // Remove extension
    const char* dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);

    char* name = malloc(len + 1);
    if (!name) return NULL;

    strncpy(name, base, len);
    name[len] = '\0';

    // Capitalize first letter
    if (name[0]) {
        name[0] = toupper(name[0]);
    }

    return name;
}

// Layout properties for flexbox containers
typedef struct {
    const char* justify_content;  // "start", "center", "end", "space-between", "space-around", "space-evenly"
    const char* align_items;      // "start", "center", "end", "stretch"
    int gap;                      // gap in pixels
    int padding;                  // padding in pixels
} LayoutProperties;

// Extract layout properties from component JSON
static LayoutProperties extract_layout_properties(cJSON* component) {
    LayoutProperties props = {0};

    if (!component) return props;

    // Extract justifyContent
    cJSON* justify = cJSON_GetObjectItem(component, "justifyContent");
    if (justify && cJSON_IsString(justify)) {
        props.justify_content = justify->valuestring;
    }

    // Extract alignItems
    cJSON* align = cJSON_GetObjectItem(component, "alignItems");
    if (align && cJSON_IsString(align)) {
        props.align_items = align->valuestring;
    }

    // Extract gap
    cJSON* gap = cJSON_GetObjectItem(component, "gap");
    if (gap && cJSON_IsNumber(gap)) {
        props.gap = gap->valueint;
    }

    // Extract padding
    cJSON* padding = cJSON_GetObjectItem(component, "padding");
    if (padding && cJSON_IsNumber(padding)) {
        props.padding = padding->valueint;
    }

    return props;
}

// Extract layout properties from property_bindings (for For loop templates)
static LayoutProperties extract_layout_from_bindings(cJSON* component) {
    LayoutProperties props = {0};

    if (!component) return props;

    // Check for property_bindings first (for static for loops)
    cJSON* bindings = cJSON_GetObjectItem(component, "property_bindings");
    if (bindings) {
        cJSON* justify_binding = cJSON_GetObjectItem(bindings, "justifyContent");
        if (justify_binding) {
            cJSON* source = cJSON_GetObjectItem(justify_binding, "source_expr");
            if (source && cJSON_IsString(source)) {
                props.justify_content = source->valuestring;
            }
        }

        cJSON* align_binding = cJSON_GetObjectItem(bindings, "alignItems");
        if (align_binding) {
            cJSON* source = cJSON_GetObjectItem(align_binding, "source_expr");
            if (source && cJSON_IsString(source)) {
                props.align_items = source->valuestring;
            }
        }

        cJSON* gap_binding = cJSON_GetObjectItem(bindings, "gap");
        if (gap_binding) {
            cJSON* source = cJSON_GetObjectItem(gap_binding, "source_expr");
            if (source && cJSON_IsString(source)) {
                // Try to parse as number
                props.gap = atoi(source->valuestring);
            }
        }
    }

    // Fallback to direct properties if not found in bindings
    if (!props.justify_content) {
        cJSON* justify = cJSON_GetObjectItem(component, "justifyContent");
        if (justify && cJSON_IsString(justify)) {
            props.justify_content = justify->valuestring;
        }
    }

    if (!props.align_items) {
        cJSON* align = cJSON_GetObjectItem(component, "alignItems");
        if (align && cJSON_IsString(align)) {
            props.align_items = align->valuestring;
        }
    }

    if (props.gap == 0) {
        cJSON* gap = cJSON_GetObjectItem(component, "gap");
        if (gap && cJSON_IsNumber(gap)) {
            props.gap = gap->valueint;
        }
    }

    if (props.padding == 0) {
        cJSON* padding = cJSON_GetObjectItem(component, "padding");
        if (padding && cJSON_IsNumber(padding)) {
            props.padding = padding->valueint;
        }
    }

    return props;
}

// Normalize alignment values (flex-start -> start, flex-end -> end)
static const char* normalize_alignment_value(const char* align) {
    if (!align) return NULL;

    if (strcmp(align, "flex-start") == 0) return "start";
    if (strcmp(align, "flex-end") == 0) return "end";
    return align;
}

// Convert CSS alignment to Tk horizontal anchor
static const char* alignment_to_anchor_x(const char* align) {
    if (!align) return "";

    align = normalize_alignment_value(align);

    if (strcmp(align, "center") == 0) return "center";
    if (strcmp(align, "end") == 0) return "e";
    if (strcmp(align, "start") == 0) return "w";
    return "";  // default
}

// Convert CSS alignment to Tk vertical anchor
static const char* alignment_to_anchor_y(const char* align) {
    if (!align) return "";

    align = normalize_alignment_value(align);

    if (strcmp(align, "center") == 0) return "center";
    if (strcmp(align, "end") == 0) return "s";
    if (strcmp(align, "start") == 0) return "n";
    return "";  // default
}

// Check if alignment requires expand flag
static bool needs_expand_for_alignment(const char* align) {
    if (!align) return false;

    align = normalize_alignment_value(align);

    return (strcmp(align, "center") == 0 ||
            strcmp(align, "end") == 0 ||
            strcmp(align, "stretch") == 0);
}

// Note: codegen_map_kir_to_tk_widget() now uses codegen_map_kir_to_tk_widget() from codegen_common.h
// This provides consistent widget mapping across all Tk-based codegens

// Generate widget path name
static void generate_widget_path(char* buffer, size_t buffer_size, int widget_id, const char* parent_path) {
    if (parent_path && strlen(parent_path) > 0) {
        snprintf(buffer, buffer_size, "%s.w%d", parent_path, widget_id);
    } else {
        snprintf(buffer, buffer_size, ".w%d", widget_id);
    }
}

// Note: codegen_parse_size_value() now uses codegen_codegen_parse_size_value() from codegen_common.h

// Convert hex color to TK color name or keep simple colors
static const char* convert_to_tk_color(const char* hex_color) {
    if (!hex_color) return "white";

    // Skip transparent colors
    if (strstr(hex_color, "#00000000") || strcmp(hex_color, "transparent") == 0) {
        return NULL;  // Don't set this color
    }

    // Common color mappings
    if (strcmp(hex_color, "#ffffff") == 0 || strcmp(hex_color, "#FFFFFF") == 0) return "white";
    if (strcmp(hex_color, "#000000") == 0) return "black";
    if (strcmp(hex_color, "#ff0000") == 0 || strcmp(hex_color, "#FF0000") == 0) return "red";
    if (strcmp(hex_color, "#00ff00") == 0 || strcmp(hex_color, "#00FF00") == 0) return "green";
    if (strcmp(hex_color, "#0000ff") == 0 || strcmp(hex_color, "#0000FF") == 0) return "blue";
    if (strcmp(hex_color, "#ffff00") == 0 || strcmp(hex_color, "#FFFF00") == 0) return "yellow";
    if (strcmp(hex_color, "#00ffff") == 0 || strcmp(hex_color, "#00FFFF") == 0) return "cyan";
    if (strcmp(hex_color, "#ff00ff") == 0 || strcmp(hex_color, "#FF00FF") == 0) return "magenta";
    if (strcmp(hex_color, "#808080") == 0) return "gray";

    // For other colors, use the hex value as-is (TK supports some hex)
    return hex_color;
}

// Apply Row layout (horizontal) to pack command
static void apply_row_layout(LimboContext* ctx, LayoutProperties* props,
                             int child_index, int total_children) {
    // Base: -side left for row
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -side left\"");

    // Add gap as padx between items
    if (props->gap > 0) {
        // For rows, add horizontal padding (gap on left side, except first item)
        // child_index < 0 means runtime loop where we can't track - add gap to all
        if (child_index < 0 || child_index > 0) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -padx %d\"", props->gap);
        }
    }

    // Handle alignItems (cross-axis alignment for rows = vertical)
    if (props->align_items) {
        const char* anchor = alignment_to_anchor_y(props->align_items);
        if (strlen(anchor) > 0) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -anchor %s\"", anchor);
        } else if (strcmp(props->align_items, "stretch") == 0) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -fill y\"");
        }
        // start is default
    }
}

// Apply Column layout (vertical) to pack command
static void apply_column_layout(LimboContext* ctx, LayoutProperties* props,
                                int child_index, int total_children) {
    // Base: default pack (vertical stacking)

    // Handle justifyContent (main-axis alignment for columns = vertical)
    if (props->justify_content) {
        const char* justify = normalize_alignment_value(props->justify_content);

        if (strcmp(justify, "center") == 0) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -expand 1 -anchor center\"");
        } else if (strcmp(justify, "end") == 0) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -expand 1 -anchor s\"");
        } else if (strcmp(justify, "space-between") == 0) {
            // Add gap, only between items
            if (child_index > 0 && props->gap > 0) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -pady %d\"", props->gap);
            }
        } else if (strcmp(justify, "space-around") == 0) {
            // Half gap before first, full gap between, half after last
            int pady = (child_index == 0 || child_index == total_children - 1) ?
                       (props->gap > 0 ? props->gap / 2 : 0) : props->gap;
            if (pady > 0) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -pady %d\"", pady);
            }
        } else if (strcmp(justify, "space-evenly") == 0) {
            // Full gap everywhere
            if (props->gap > 0) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -pady %d\"", props->gap);
            }
        }
        // start is default (no special options)
    }

    // Add gap if not handled by space-* alignments
    if (props->gap > 0 && (!props->justify_content ||
        (strcmp(props->justify_content, "space-between") != 0 &&
         strcmp(props->justify_content, "space-around") != 0 &&
         strcmp(props->justify_content, "space-evenly") != 0))) {
        if (child_index > 0) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -pady %d\"", props->gap);
        }
    }

    // Handle alignItems (cross-axis alignment for columns = horizontal)
    if (props->align_items) {
        const char* anchor = alignment_to_anchor_x(props->align_items);
        if (strlen(anchor) > 0) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -anchor %s\"", anchor);
        } else if (strcmp(props->align_items, "stretch") == 0) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -fill x\"");
        }
        // start is default
    }
}

// Generate Tk options from KIR properties
static void generate_tk_options(LimboContext* ctx, cJSON* component, const char* parent_bg) {
    if (!component) return;

    // Text content
    cJSON* text = cJSON_GetObjectItem(component, "text");
    if (text && text->type == cJSON_String) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -text {%s}", text->valuestring);
    }

    // Width (can be string like "200.0px", "100%" or number)
    cJSON* width = cJSON_GetObjectItem(component, "width");
    if (width) {
        if (width->type == cJSON_String) {
            // Check for percentage values
            if (strstr(width->valuestring, "%")) {
                // Skip setting width for percentage values - let widget expand
                // In Tk, pack geometry manager will handle sizing
            } else {
                int w = codegen_parse_size_value(width->valuestring);
                if (w > 0) {
                    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -width %d", w);
                }
            }
        } else if (width->type == cJSON_Number) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -width %d", width->valueint);
        }
    }

    // Height
    cJSON* height = cJSON_GetObjectItem(component, "height");
    if (height) {
        if (height->type == cJSON_String) {
            // Check for percentage values
            if (strstr(height->valuestring, "%")) {
                // Skip setting height for percentage values - let widget expand
            } else {
                int h = codegen_parse_size_value(height->valuestring);
                if (h > 0) {
                    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -height %d", h);
                }
            }
        } else if (height->type == cJSON_Number) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -height %d", height->valueint);
        }
    }

    // Background color
    cJSON* background = cJSON_GetObjectItem(component, "background");
    cJSON* type = cJSON_GetObjectItem(component, "type");
    const char* widget_type = (type && type->type == cJSON_String) ? type->valuestring : "";
    bool is_text = (strcmp(widget_type, "Text") == 0 || strcmp(widget_type, "Label") == 0);

    if (background && background->type == cJSON_String) {
        const char* tk_color = convert_to_tk_color(background->valuestring);
        if (tk_color) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -bg %s", tk_color);
        } else if (is_text && parent_bg) {
            // Transparent background on text - use parent's background
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -bg %s", parent_bg);
        }
    } else if (is_text && parent_bg) {
        // No background specified on text - use parent's background
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -bg %s", parent_bg);
    }

    // Foreground color (text color)
    cJSON* color = cJSON_GetObjectItem(component, "color");
    if (color && color->type == cJSON_String) {
        const char* tk_color = convert_to_tk_color(color->valuestring);
        if (tk_color) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -fg %s", tk_color);
        }
    }

    // Border
    cJSON* border = cJSON_GetObjectItem(component, "border");
    if (border && cJSON_IsObject(border)) {
        cJSON* border_width = cJSON_GetObjectItem(border, "width");
        cJSON* border_color = cJSON_GetObjectItem(border, "color");

        if (border_width && border_width->type == cJSON_Number) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -borderwidth %d", border_width->valueint);
        }
        if (border_color && border_color->type == cJSON_String) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -relief raised");
        }
    }

    // Font - use TaijiOS default font if not specified
    cJSON* font = cJSON_GetObjectItem(component, "font");
    if (font && font->type == cJSON_String) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -font {%s}", font->valuestring);
    } else {
        // Add default font for text elements
        cJSON* type = cJSON_GetObjectItem(component, "type");
        if (type && type->type == cJSON_String) {
            if (strcmp(type->valuestring, "Text") == 0 || strcmp(type->valuestring, "Label") == 0) {
                append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " -font /fonts/lucidasans/latin1.7.font");
            }
        }
    }

    // Add padding for text elements to make them visible (reuse 'type' from above)
    if (is_text) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " -padx 5 -pady 5");
    }
}

// Forward declarations - ALL must be before first use
static bool process_component(LimboContext* ctx, cJSON* component, const char* parent_path, int depth, const char* parent_bg, const char* parent_type, LayoutProperties* parent_layout);
static void generate_for_loop_runtime(LimboContext* ctx, cJSON* for_comp, const char* parent_path, int depth, const char* parent_bg, const char* parent_type, LayoutProperties* parent_layout, cJSON* source_structures);
static bool process_component_with_bindings(LimboContext* ctx, cJSON* comp, const char* parent_path, int depth, const char* parent_bg, const char* parent_type, const char* item_name, cJSON* data_item, LayoutProperties* parent_layout);
static bool generate_widget_with_bindings(LimboContext* ctx, cJSON* component, const char* parent_path, int depth, const char* parent_bg, const char* parent_type, const char* item_name, cJSON* data_item, LayoutProperties* parent_layout, int child_index, int total_children);
static void generate_tk_options_with_bindings(LimboContext* ctx, cJSON* component, const char* parent_bg, const char* item_name, cJSON* data_item);
static bool generate_widget(LimboContext* ctx, cJSON* component, const char* parent_path, int depth, const char* parent_bg, const char* parent_type, LayoutProperties* parent_layout, int child_index, int total_children);
static cJSON* resolve_for_source_data(cJSON* for_def, cJSON* source_structures);

// Resolve For loop source data from source_structures
static cJSON* resolve_for_source_data(cJSON* for_def, cJSON* source_structures) {
    cJSON* source = cJSON_GetObjectItem(for_def, "source");
    if (!source) return NULL;

    cJSON* source_type = cJSON_GetObjectItem(source, "type");
    if (!source_type || !cJSON_IsString(source_type)) return NULL;

    const char* type_str = source_type->valuestring;

    // For variable sources, look up in source_structures
    if (strcmp(type_str, "variable") == 0) {
        cJSON* expr = cJSON_GetObjectItem(source, "expression");
        if (!expr || !cJSON_IsString(expr)) return NULL;

        const char* var_name = expr->valuestring;

        // Find const declaration
        if (source_structures) {
            cJSON* const_decls = cJSON_GetObjectItem(source_structures, "const_declarations");
            if (const_decls && cJSON_IsArray(const_decls)) {
                cJSON* decl;
                cJSON_ArrayForEach(decl, const_decls) {
                    cJSON* name = cJSON_GetObjectItem(decl, "name");
                    if (name && cJSON_IsString(name) && strcmp(name->valuestring, var_name) == 0) {
                        cJSON* value_json = cJSON_GetObjectItem(decl, "value_json");
                        if (value_json && cJSON_IsString(value_json)) {
                            // Parse the JSON value
                            cJSON* data = cJSON_Parse(value_json->valuestring);
                            // Handle double-encoded JSON
                            if (data && cJSON_IsString(data)) {
                                cJSON* inner = cJSON_Parse(data->valuestring);
                                cJSON_Delete(data);
                                return inner;
                            }
                            return data;
                        }
                    }
                }
            }
        }
    }

    // For literal sources, get data directly
    if (strcmp(type_str, "literal") == 0) {
        cJSON* data = cJSON_GetObjectItem(source, "data");
        if (data) {
            // Return a clone
            return cJSON_Duplicate(data, true);
        }
    }

    return NULL;
}

// Forward declarations for runtime loop generation
static void generate_data_arrays(LimboContext* ctx, cJSON* source_structures);
static void generate_for_loop_runtime(LimboContext* ctx, cJSON* for_comp,
                                      const char* parent_path, int depth,
                                      const char* parent_bg, const char* parent_type,
                                      LayoutProperties* parent_layout,
                                      cJSON* source_structures);
static void generate_template_with_runtime_bindings(LimboContext* ctx, cJSON* template,
                                                    int depth, const char* parent_bg,
                                                    const char* parent_type,
                                                    const char* array_base_name,
                                                    const char* parent_path_expr,
                                                    LayoutProperties* parent_layout);
static const char* convert_binding_to_array_access(const char* expr, const char* array_base_name,
                                                    char* buffer, size_t buffer_size);

// Generate data arrays from source_structures
static void generate_data_arrays(LimboContext* ctx, cJSON* source_structures) {
    if (!ctx || !source_structures) return;

    cJSON* const_decls = cJSON_GetObjectItem(source_structures, "const_declarations");
    if (!const_decls || !cJSON_IsArray(const_decls)) return;

    if (ctx->include_comments) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n# Data arrays for For loops\n");
    }

    cJSON* decl;
    cJSON_ArrayForEach(decl, const_decls) {
        cJSON* name = cJSON_GetObjectItem(decl, "name");
        cJSON* value_json = cJSON_GetObjectItem(decl, "value_json");
        if (!name || !value_json || !cJSON_IsString(name) || !cJSON_IsString(value_json)) {
            continue;
        }

        // Parse the array data
        cJSON* data = cJSON_Parse(value_json->valuestring);
        if (!data || !cJSON_IsArray(data)) {
            if (data) cJSON_Delete(data);
            continue;
        }

        int data_count = cJSON_GetArraySize(data);
        if (data_count == 0) {
            cJSON_Delete(data);
            continue;
        }

        // Analyze first item to determine field types
        cJSON* first_item = cJSON_GetArrayItem(data, 0);
        if (!first_item || !cJSON_IsObject(first_item)) {
            cJSON_Delete(data);
            continue;
        }

        // For each field in the object, create a parallel array
        cJSON* field;
        cJSON_ArrayForEach(field, first_item) {
            const char* field_name = field->string;

            // Determine array type from first value
            if (cJSON_IsString(field)) {
                // Generate string array (NOTE: no [] on type for 1D arrays)
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                          "%s_%s: array of string = array[] of {",
                          name->valuestring, field_name);

                // Collect all values for this field
                bool first = true;
                for (int i = 0; i < data_count; i++) {
                    cJSON* item = cJSON_GetArrayItem(data, i);
                    cJSON* value = cJSON_GetObjectItem(item, field_name);
                    if (value && cJSON_IsString(value)) {
                        if (!first) {
                            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ", ");
                        }
                        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                                  "\"%s\"", value->valuestring);
                        first = false;
                    }
                }
                append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "};\n");
            }
            else if (cJSON_IsNumber(field)) {
                // Generate int array (NOTE: no [] on type for 1D arrays)
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                          "%s_%s: array of int = array[] of {",
                          name->valuestring, field_name);

                bool first = true;
                for (int i = 0; i < data_count; i++) {
                    cJSON* item = cJSON_GetArrayItem(data, i);
                    cJSON* value = cJSON_GetObjectItem(item, field_name);
                    if (value && cJSON_IsNumber(value)) {
                        if (!first) {
                            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ", ");
                        }
                        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                                  "%d", value->valueint);
                        first = false;
                    }
                }
                append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "};\n");
            }
            else if (cJSON_IsArray(field)) {
                // Generate array of arrays (for colors)
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                          "%s_%s: array of array of string = array[] of {\n",
                          name->valuestring, field_name);

                for (int i = 0; i < data_count; i++) {
                    cJSON* item = cJSON_GetArrayItem(data, i);
                    cJSON* arr_value = cJSON_GetObjectItem(item, field_name);
                    if (arr_value && cJSON_IsArray(arr_value)) {
                        if (i > 0) {
                            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ",\n");
                        }
                        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\tarray[] of {");

                        int elem_count = cJSON_GetArraySize(arr_value);
                        for (int j = 0; j < elem_count; j++) {
                            cJSON* elem = cJSON_GetArrayItem(arr_value, j);
                            if (elem && cJSON_IsString(elem)) {
                                if (j > 0) {
                                    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ", ");
                                }
                                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                                          "\"%s\"", elem->valuestring);
                            }
                        }
                        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "}");
                    }
                }
                append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n};\n");
            }
        }

        cJSON_Delete(data);
    }
}

// Convert binding expression to array access
// "item.name" -> "alignments_name[i]"
// "item.colors[0]" -> "alignments_colors[i][0]"
static const char* convert_binding_to_array_access(const char* expr, const char* array_base_name,
                                                    char* buffer, size_t buffer_size) {
    if (!expr || !buffer || buffer_size == 0) return expr;

    // Check if it starts with "item."
    if (strncmp(expr, "item.", 5) == 0) {
        const char* field = expr + 5;

        // Look for array index [n]
        const char* bracket = strchr(field, '[');
        if (bracket == NULL) {
            // Simple field: item.name -> alignments_name[i]
            snprintf(buffer, buffer_size, "%s_%s[i]", array_base_name, field);
            return buffer;
        }
        else {
            // Array access: item.colors[0] -> alignments_colors[i][0]
            char field_name[64];
            int index;
            if (sscanf(field, "%63[^[][%d]", field_name, &index) == 2) {
                snprintf(buffer, buffer_size, "%s_%s[i][%d]", array_base_name, field_name, index);
                return buffer;
            }
        }
    }

    return expr;  // Return as-is if no conversion
}

// Generate template widget with runtime binding resolution
static void generate_template_with_runtime_bindings(LimboContext* ctx, cJSON* template,
                                                    int depth, const char* parent_bg,
                                                    const char* parent_type,
                                                    const char* array_base_name,
                                                    const char* parent_path_expr,
                                                    LayoutProperties* parent_layout) {
    if (!template) return;

    cJSON* type = cJSON_GetObjectItem(template, "type");
    if (!type || !cJSON_IsString(type)) return;

    const char* widget_type = type->valuestring;
    const char* tk_widget = codegen_map_kir_to_tk_widget(widget_type);

    // Indentation
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    // Generate widget path dynamically (unique name for each widget)
    ctx->widget_counter++;
    char widget_var_name[64];
    snprintf(widget_var_name, sizeof(widget_var_name), "widget_path_%d", ctx->widget_counter);

    // If parent_path_expr is provided, make this widget a child of parent
    // Otherwise, make it a child of container_path (for loop root level)
    if (parent_path_expr && strlen(parent_path_expr) > 0) {
        // Check if parent_path_expr is a variable name (like "widget_path_3") or a literal path (like ".w1.w2")
        bool is_variable = (strncmp(parent_path_expr, "widget_path_", 12) == 0);
        if (is_variable) {
            // Use variable directly without quotes
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                      "%s := %s + \".w%d\";\n", widget_var_name, parent_path_expr, ctx->widget_counter);
        } else {
            // Use quoted string for literal paths
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                      "%s := \"%s\" + \".w%d\";\n", widget_var_name, parent_path_expr, ctx->widget_counter);
        }
    } else {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                  "%s := container_path + \".w%d\";\n", widget_var_name, ctx->widget_counter);
    }

    // Build options string first
    char options[1024] = "";
    size_t options_len = 0;

    char binding_buffer[256];

    // Add properties with runtime binding resolution
    cJSON* bindings = cJSON_GetObjectItem(template, "property_bindings");
    bool has_dynamic_bindings = false;

    if (bindings) {
        // Text binding
        cJSON* text_binding = cJSON_GetObjectItem(bindings, "text");
        if (text_binding) {
            cJSON* source_expr = cJSON_GetObjectItem(text_binding, "source_expr");
            if (source_expr && cJSON_IsString(source_expr)) {
                has_dynamic_bindings = true;
            }
        }

        // Background binding
        cJSON* bg_binding = cJSON_GetObjectItem(bindings, "background");
        if (bg_binding) {
            cJSON* source_expr = cJSON_GetObjectItem(bg_binding, "source_expr");
            if (source_expr && cJSON_IsString(source_expr)) {
                has_dynamic_bindings = true;
            }
        }
    }

    // Add static properties (width, height, etc.)
    cJSON* width = cJSON_GetObjectItem(template, "width");
    cJSON* height = cJSON_GetObjectItem(template, "height");

    if (width) {
        if (cJSON_IsString(width)) {
            // Skip percentage values - let widget expand
            if (!strstr(width->valuestring, "%")) {
                int w = codegen_parse_size_value(width->valuestring);
                if (w > 0) {
                    options_len += snprintf(options + options_len, sizeof(options) - options_len,
                                          " -width %d", w);
                }
            }
        } else if (cJSON_IsNumber(width)) {
            options_len += snprintf(options + options_len, sizeof(options) - options_len,
                                  " -width %d", width->valueint);
        }
    }

    if (height) {
        if (cJSON_IsString(height)) {
            // Skip percentage values - let widget expand
            if (!strstr(height->valuestring, "%")) {
                int h = codegen_parse_size_value(height->valuestring);
                if (h > 0) {
                    options_len += snprintf(options + options_len, sizeof(options) - options_len,
                                          " -height %d", h);
                }
            }
        } else if (cJSON_IsNumber(height)) {
            options_len += snprintf(options + options_len, sizeof(options) - options_len,
                                  " -height %d", height->valueint);
        }
    }

    // Add font for text elements
    bool is_text = (strcmp(widget_type, "Text") == 0 || strcmp(widget_type, "Label") == 0);
    if (is_text) {
        options_len += snprintf(options + options_len, sizeof(options) - options_len,
                              " -font /fonts/lucidasans/latin1.7.font -padx 5 -pady 5");
    }

    // Indentation for command
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    // Start building tk command
    // Check if cmd is already declared in this scope
    bool use_reassignment = ctx->cmd_declared_in_loop;

    if (has_dynamic_bindings) {
        // We have dynamic bindings - build command with string concatenation
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity,
                     use_reassignment ? "cmd = \"" : "cmd := \"");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s \" + %s", tk_widget, widget_var_name);

        if (bindings) {
            cJSON* text_binding = cJSON_GetObjectItem(bindings, "text");
            if (text_binding) {
                cJSON* source_expr = cJSON_GetObjectItem(text_binding, "source_expr");
                if (source_expr && cJSON_IsString(source_expr)) {
                    const char* resolved = convert_binding_to_array_access(
                        source_expr->valuestring, array_base_name, binding_buffer, sizeof(binding_buffer));
                    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -text \" + %s", resolved);
                }
            }

            cJSON* bg_binding = cJSON_GetObjectItem(bindings, "background");
            if (bg_binding) {
                cJSON* source_expr = cJSON_GetObjectItem(bg_binding, "source_expr");
                if (source_expr && cJSON_IsString(source_expr)) {
                    const char* resolved = convert_binding_to_array_access(
                        source_expr->valuestring, array_base_name, binding_buffer, sizeof(binding_buffer));
                    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \" -bg \" + %s", resolved);
                }
            }
        }

        if (strlen(options) > 0) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \"%s", options);
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\";\n");
        } else {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\";\n");
        }

        // Execute widget creation command BEFORE processing children
        // (otherwise cmd will be overwritten by children's commands)
        for (int i = 0; i < depth; i++) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
        }
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, cmd);\n");
    } else {
        // No dynamic bindings - ALWAYS use cmd variable pattern
        // (Limbo doesn't support string concatenation in function arguments)
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity,
                     use_reassignment ? "cmd = \"" : "cmd := \"");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s \" + %s", tk_widget, widget_var_name);

        if (strlen(options) > 0) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " + \"%s", options);
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\";\n");
        } else {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ";\n");
        }

        // Execute command
        for (int i = 0; i < depth; i++) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
        }
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, cmd);\n");
    }

    // Mark cmd as declared for subsequent widgets in this loop
    if (!use_reassignment) {
        ctx->cmd_declared_in_loop = true;
    }

    // Process children recursively
    cJSON* children = cJSON_GetObjectItem(template, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            // Pass current widget's path so children become descendants, not siblings
            // Extract layout from this template for children
            LayoutProperties child_layout = extract_layout_from_bindings(template);
            generate_template_with_runtime_bindings(ctx, child, depth, parent_bg,
                                                   widget_type, array_base_name, widget_var_name, &child_layout);
        }
    }

    // Pack the widget
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    bool has_explicit_size = (width != NULL) || (height != NULL);
    bool parent_is_center = (parent_type && strcmp(parent_type, "Center") == 0);
    bool parent_is_row = (parent_type && strcmp(parent_type, "Row") == 0);
    bool parent_is_column = (parent_type && strcmp(parent_type, "Column") == 0);

    // Generate pack command using cmd variable (Limbo doesn't support concatenation in function args)
    // Use = if cmd already declared, := otherwise
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity,
                 ctx->cmd_declared_in_loop ? "cmd = \"pack \" + " : "cmd := \"pack \" + ");

    // Use layout-aware packing FIRST (before other checks) if parent has layout properties
    if (parent_layout && parent_is_row) {
        // Row layout - for runtime loops use child_index=-1 to add gap
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s", widget_var_name);
        apply_row_layout(ctx, parent_layout, -1, 0);
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\";\n");
    } else if (parent_layout && parent_is_column) {
        // Column layout
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s", widget_var_name);
        apply_column_layout(ctx, parent_layout, -1, 0);
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\";\n");
    } else if (depth == 2) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s + \" -fill both -expand 1\";\n", widget_var_name);
    } else if (parent_is_row) {
        // Widget inside Row - pack left-to-right with -side left
        if (has_explicit_size) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s + \" -side left -padx 5 -pady 5\";\n", widget_var_name);
        } else {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s + \" -side left\";\n", widget_var_name);
        }
    } else if (parent_is_center) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s + \" -expand 1\";\n", widget_var_name);
    } else if (is_text && depth > 2) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s + \" -expand 1\";\n", widget_var_name);
    } else if (has_explicit_size) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s + \" -padx 20 -pady 20\";\n", widget_var_name);
    } else {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s + \" -fill both -expand 1\";\n", widget_var_name);
    }

    // Execute pack command
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, cmd);\n");

    // Mark cmd as declared
    ctx->cmd_declared_in_loop = true;
}

// Generate For loop as runtime iteration
static void generate_for_loop_runtime(LimboContext* ctx, cJSON* for_comp,
                                      const char* parent_path, int depth,
                                      const char* parent_bg, const char* parent_type,
                                      LayoutProperties* parent_layout,
                                      cJSON* source_structures) {
    // Extract For definition
    cJSON* for_def = cJSON_GetObjectItem(for_comp, "for");
    if (!for_def) {
        fprintf(stderr, "Warning: For component has no 'for' definition\n");
        return;
    }

    // Get array name from source
    cJSON* source = cJSON_GetObjectItem(for_def, "source");
    if (!source) return;

    cJSON* expr = cJSON_GetObjectItem(source, "expression");
    if (!expr || !cJSON_IsString(expr)) return;

    const char* array_name = expr->valuestring;

    // Get template (first child)
    cJSON* children_array = cJSON_GetObjectItem(for_comp, "children");
    if (!children_array || cJSON_GetArraySize(children_array) == 0) {
        fprintf(stderr, "Warning: For loop has no template children\n");
        return;
    }

    cJSON* template = cJSON_GetArrayItem(children_array, 0);
    if (!template) {
        fprintf(stderr, "Warning: For loop template is NULL\n");
        return;
    }

    // Add comment
    if (ctx->include_comments) {
        for (int i = 0; i < depth; i++) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
        }
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                  "# For loop: iterate over %s\n", array_name);
    }

    // Generate runtime for loop
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
              "for (i := 0; i < len(%s_name); i++) {\n", array_name);

    // Reset cmd declaration flag for new for loop scope
    ctx->cmd_declared_in_loop = false;

    // For loop: widgets are direct children of parent_path
    // Each widget gets a unique name from the global widget_counter
    // Don't create intermediate container_path - it causes issues with non-existent parent frames

    // Generate template widgets with runtime bindings
    // Pass parent_path as the parent_expr so root widgets are direct children
    generate_template_with_runtime_bindings(ctx, template, depth + 1,
                                            parent_bg, parent_type, array_name, parent_path, parent_layout);

    // Close the loop
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "}\n");
}

// Generate widget creation code
static bool generate_widget(LimboContext* ctx, cJSON* component, const char* parent_path, int depth, const char* parent_bg, const char* parent_type, LayoutProperties* parent_layout, int child_index, int total_children) {
    // KIR uses "type" not "elementType"
    cJSON* element_type_obj = cJSON_GetObjectItem(component, "type");
    if (!element_type_obj || element_type_obj->type != cJSON_String) {
        return false;
    }

    const char* element_type = element_type_obj->valuestring;

    // Debug
    fprintf(stderr, "[GEN_WIDGET] type=%s, parent_type=%s\n", element_type, parent_type ? parent_type : "NULL");
    const char* tk_widget = codegen_map_kir_to_tk_widget(element_type);

    // Extract this widget's layout properties for its children
    LayoutProperties my_layout = extract_layout_properties(component);

    // Debug: show layout properties
    if (my_layout.gap > 0 || my_layout.align_items) {
        fprintf(stderr, "[WIDGET DEBUG] type=%s, gap=%d, align_items=%s\n",
                element_type, my_layout.gap,
                my_layout.align_items ? my_layout.align_items : "NULL");
    }

    ctx->widget_counter++;
    char widget_path[256];
    generate_widget_path(widget_path, sizeof(widget_path), ctx->widget_counter, parent_path);

    // Add indentation
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    if (ctx->include_comments) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "# Create %s widget\n", element_type);
        for (int i = 0; i < depth; i++) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
        }
    }

    // Get this widget's background color for passing to children
    cJSON* my_background = cJSON_GetObjectItem(component, "background");
    const char* my_bg = NULL;
    if (my_background && my_background->type == cJSON_String) {
        my_bg = convert_to_tk_color(my_background->valuestring);
    }

    // Generate Tk command
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"%s %s", tk_widget, widget_path);

    // Add properties as Tk options (KIR has properties directly on component)
    generate_tk_options(ctx, component, parent_bg);

    // Check for event handlers in events array (KIR structure)
    cJSON* events = cJSON_GetObjectItem(component, "events");
    if (events && cJSON_IsArray(events)) {
        cJSON* event;
        cJSON_ArrayForEach(event, events) {
            cJSON* event_type = cJSON_GetObjectItem(event, "type");
            cJSON* logic_id = cJSON_GetObjectItem(event, "logic_id");
            if (event_type && cJSON_IsString(event_type) &&
                logic_id && cJSON_IsString(logic_id) &&
                strcmp(event_type->valuestring, "click") == 0) {

                ctx->event_handler_counter++;

                // Register the handler name with its number
                register_event_handler(ctx->event_handler_counter, logic_id->valuestring);

                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                          " -command {send eventch click%d}", ctx->event_handler_counter);
            }
        }
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\");\n");

    // Process children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        int child_count = cJSON_GetArraySize(children);
        cJSON* child = NULL;
        int idx = 0;
        cJSON_ArrayForEach(child, children) {
            process_component(ctx, child, widget_path, depth, my_bg, element_type, &my_layout);
            idx++;
        }
    }

    // Pack or place the widget
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    // Check if widget has explicit dimensions
    cJSON* width = cJSON_GetObjectItem(component, "width");
    cJSON* height = cJSON_GetObjectItem(component, "height");
    bool has_explicit_size = (width != NULL) || (height != NULL);

    cJSON* type = cJSON_GetObjectItem(component, "type");
    const char* widget_type = (type && type->type == cJSON_String) ? type->valuestring : "";
    bool is_text = (strcmp(widget_type, "Text") == 0 || strcmp(widget_type, "Label") == 0);

    // Check for positioning properties
    cJSON* left = cJSON_GetObjectItem(component, "left");
    cJSON* top = cJSON_GetObjectItem(component, "top");
    bool has_position = (left && left->type == cJSON_Number) && (top && top->type == cJSON_Number);

    // Check if parent is a Center widget
    bool parent_is_center = (parent_type && strcmp(parent_type, "Center") == 0);

    // Check if parent is a Row widget (needs -side left for children)
    bool parent_is_row = (parent_type && strcmp(parent_type, "Row") == 0);

    bool parent_is_column = (parent_type && strcmp(parent_type, "Column") == 0);

    // Use layout-aware packing if parent has layout properties
    if (parent_layout && parent_is_row) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack ");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s", widget_path);
        apply_row_layout(ctx, parent_layout, child_index, total_children);
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\");\n");
    } else if (parent_layout && parent_is_column) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack ");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s", widget_path);
        apply_column_layout(ctx, parent_layout, child_index, total_children);
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\");\n");
    } else if (depth == 1) {
        // Root widget fills the window
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -fill both -expand 1\");\n", widget_path);
    } else if (parent_is_row) {
        // Widget inside Row - pack left-to-right with -side left
        if (has_explicit_size) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -side left -padx 5 -pady 5\");\n", widget_path);
        } else {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -side left\");\n", widget_path);
        }
    } else if (parent_is_center) {
        // Widget inside Center - use expand to center both X and Y
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -expand 1\");\n", widget_path);
    } else if (is_text && depth > 2) {
        // Text inside a container - use pack with expand (not place, which doesn't work)
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -expand 1\");\n", widget_path);
    } else if (has_position && has_explicit_size) {
        // Widget with explicit position - use pack with anchor and padding
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -anchor nw -padx %d -pady %d\");\n",
                  widget_path, (int)left->valueint, (int)top->valueint);
    } else if (has_explicit_size) {
        // Widget with explicit dimensions but no position - pack with default padding
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -padx 20 -pady 20\");\n", widget_path);
    } else {
        // Widget without explicit size - pack with expand
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -fill both -expand 1\");\n", widget_path);
    }

    return true;
}

// Process component with property binding resolution (for For loop templates)
static bool process_component_with_bindings(LimboContext* ctx, cJSON* comp,
                                           const char* parent_path, int depth,
                                           const char* parent_bg, const char* parent_type,
                                           const char* item_name, cJSON* data_item,
                                           LayoutProperties* parent_layout) {
    if (!comp) return false;

    // Check for static_block - skip and process children directly
    cJSON* type = cJSON_GetObjectItem(comp, "type");
    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "static_block") == 0) {
        cJSON* children = cJSON_GetObjectItem(comp, "children");
        if (children && cJSON_IsArray(children)) {
            for (int i = 0; i < cJSON_GetArraySize(children); i++) {
                cJSON* child = cJSON_GetArrayItem(children, i);
                process_component_with_bindings(ctx, child, parent_path, depth,
                                              parent_bg, parent_type, item_name, data_item, parent_layout);
            }
        }
        return true;
    }

    // Check for nested For loop
    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "For") == 0) {
        generate_for_loop_runtime(ctx, comp, parent_path, depth, parent_bg, parent_type, parent_layout, NULL);
        return true;
    }

    // Regular widget - generate with binding resolution
    return generate_widget_with_bindings(ctx, comp, parent_path, depth, parent_bg, parent_type, item_name, data_item, parent_layout, -1, 0);
}

// Generate widget with property binding resolution
static bool generate_widget_with_bindings(LimboContext* ctx, cJSON* component,
                                         const char* parent_path, int depth,
                                         const char* parent_bg, const char* parent_type,
                                         const char* item_name, cJSON* data_item,
                                         LayoutProperties* parent_layout,
                                         int child_index, int total_children) {
    cJSON* element_type_obj = cJSON_GetObjectItem(component, "type");
    if (!element_type_obj || !cJSON_IsString(element_type_obj)) {
        return false;
    }

    const char* element_type = element_type_obj->valuestring;
    const char* tk_widget = codegen_map_kir_to_tk_widget(element_type);

    // Extract this widget's layout for its children
    LayoutProperties my_layout = extract_layout_from_bindings(component);

    ctx->widget_counter++;
    char widget_path[256];
    generate_widget_path(widget_path, sizeof(widget_path), ctx->widget_counter, parent_path);

    // Add indentation
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    if (ctx->include_comments) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "# Create %s widget (loop iteration)\n", element_type);
        for (int i = 0; i < depth; i++) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
        }
    }

    // Get this widget's background color for passing to children
    cJSON* my_background = cJSON_GetObjectItem(component, "background");
    const char* my_bg = NULL;
    if (my_background && cJSON_IsString(my_background)) {
        my_bg = convert_to_tk_color(my_background->valuestring);
    }

    // Generate Tk command
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"%s %s", tk_widget, widget_path);

    // Add properties with binding resolution
    generate_tk_options_with_bindings(ctx, component, parent_bg, item_name, data_item);

    // Check for event handlers in events array (KIR structure)
    cJSON* events = cJSON_GetObjectItem(component, "events");
    if (events && cJSON_IsArray(events)) {
        cJSON* event;
        cJSON_ArrayForEach(event, events) {
            cJSON* event_type = cJSON_GetObjectItem(event, "type");
            cJSON* logic_id = cJSON_GetObjectItem(event, "logic_id");
            if (event_type && cJSON_IsString(event_type) &&
                logic_id && cJSON_IsString(logic_id) &&
                strcmp(event_type->valuestring, "click") == 0) {

                ctx->event_handler_counter++;
                register_event_handler(ctx->event_handler_counter, logic_id->valuestring);
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                          " -command {send eventch click%d}", ctx->event_handler_counter);
            }
        }
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\");\n");

    // Process children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            process_component_with_bindings(ctx, child, widget_path, depth, my_bg, element_type, item_name, data_item, &my_layout);
        }
    }

    // Pack the widget
    for (int i = 0; i < depth; i++) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t");
    }

    cJSON* width = cJSON_GetObjectItem(component, "width");
    cJSON* height = cJSON_GetObjectItem(component, "height");
    bool has_explicit_size = (width != NULL) || (height != NULL);

    bool is_text = (strcmp(element_type, "Text") == 0 || strcmp(element_type, "Label") == 0);

    cJSON* left = cJSON_GetObjectItem(component, "left");
    cJSON* top = cJSON_GetObjectItem(component, "top");
    bool has_position = (left && cJSON_IsNumber(left)) && (top && cJSON_IsNumber(top));

    bool parent_is_center = (parent_type && strcmp(parent_type, "Center") == 0);
    bool parent_is_row = (parent_type && strcmp(parent_type, "Row") == 0);
    bool parent_is_column = (parent_type && strcmp(parent_type, "Column") == 0);

    // Use layout-aware packing if parent has layout properties
    if (parent_layout && parent_is_row) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack ");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s", widget_path);
        apply_row_layout(ctx, parent_layout, child_index, total_children);
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\");\n");
    } else if (parent_layout && parent_is_column) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack ");
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s", widget_path);
        apply_column_layout(ctx, parent_layout, child_index, total_children);
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\");\n");
    } else if (depth == 1) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -fill both -expand 1\");\n", widget_path);
    } else if (parent_is_center) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -expand 1\");\n", widget_path);
    } else if (is_text && depth > 2) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -expand 1\");\n", widget_path);
    } else if (has_position && has_explicit_size) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -anchor nw -padx %d -pady %d\");\n",
                  widget_path, (int)left->valueint, (int)top->valueint);
    } else if (has_explicit_size) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -padx 20 -pady 20\");\n", widget_path);
    } else {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "tk->cmd(t, \"pack %s -fill both -expand 1\");\n", widget_path);
    }

    return true;
}

// Helper to resolve property value from bindings and data_item
static const char* resolve_property_value(cJSON* component, cJSON* data_item, const char* property_name, char* buffer, size_t buffer_size) {
    buffer[0] = '\0';

    // Check for property bindings
    cJSON* bindings = cJSON_GetObjectItem(component, "property_bindings");
    if (bindings && data_item) {
        cJSON* binding = cJSON_GetObjectItem(bindings, property_name);
        if (binding) {
            cJSON* resolved = cJSON_GetObjectItem(binding, "resolved_value");
            if (resolved) {
                // If we have a data_item, try to get the actual value
                // The binding source expression tells us which field to extract
                cJSON* source_expr = cJSON_GetObjectItem(binding, "source_expr");
                if (source_expr && cJSON_IsString(source_expr)) {
                    const char* expr = source_expr->valuestring;

                    // Parse expressions like "item.name", "item.colors[0]", "item.gap"
                    if (strncmp(expr, "item.", 5) == 0) {
                        const char* field_path = expr + 5;

                        // Handle simple field access: "item.name"
                        if (strchr(field_path, '[') == NULL) {
                            cJSON* field = cJSON_GetObjectItem(data_item, field_path);
                            if (field) {
                                if (cJSON_IsString(field)) {
                                    strncpy(buffer, field->valuestring, buffer_size - 1);
                                    buffer[buffer_size - 1] = '\0';
                                    return buffer;
                                } else if (cJSON_IsNumber(field)) {
                                    snprintf(buffer, buffer_size, "%d", field->valueint);
                                    return buffer;
                                }
                            }
                        }
                        // Handle array access: "item.colors[0]"
                        else {
                            char field_name[64];
                            int index = 0;
                            if (sscanf(field_path, "%63[^[][%d]", field_name, &index) == 2) {
                                cJSON* field = cJSON_GetObjectItem(data_item, field_name);
                                if (field && cJSON_IsArray(field)) {
                                    cJSON* elem = cJSON_GetArrayItem(field, index);
                                    if (elem && cJSON_IsString(elem)) {
                                        strncpy(buffer, elem->valuestring, buffer_size - 1);
                                        buffer[buffer_size - 1] = '\0';
                                        return buffer;
                                    }
                                }
                            }
                        }
                    }
                }

                // Fall back to resolved_value if available
                if (cJSON_IsString(resolved)) {
                    strncpy(buffer, resolved->valuestring, buffer_size - 1);
                    buffer[buffer_size - 1] = '\0';
                    return buffer;
                }
            }
        }
    }

    return NULL;
}

// Generate Tk options with property binding resolution
static void generate_tk_options_with_bindings(LimboContext* ctx, cJSON* component,
                                              const char* parent_bg, const char* item_name,
                                              cJSON* data_item) {
    if (!component) return;

    char value_buffer[256];

    // Text content - check bindings first
    const char* resolved_text = resolve_property_value(component, data_item, "text", value_buffer, sizeof(value_buffer));
    if (resolved_text) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -text {%s}", resolved_text);
    } else {
        cJSON* text = cJSON_GetObjectItem(component, "text");
        if (text && cJSON_IsString(text)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -text {%s}", text->valuestring);
        }
    }

    // Width (can be string like "200.0px" or number)
    cJSON* width = cJSON_GetObjectItem(component, "width");
    if (width) {
        if (cJSON_IsString(width)) {
            int w = codegen_parse_size_value(width->valuestring);
            if (w > 0) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -width %d", w);
            }
        } else if (cJSON_IsNumber(width)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -width %d", width->valueint);
        }
    }

    // Height
    cJSON* height = cJSON_GetObjectItem(component, "height");
    if (height) {
        if (cJSON_IsString(height)) {
            int h = codegen_parse_size_value(height->valuestring);
            if (h > 0) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -height %d", h);
            }
        } else if (cJSON_IsNumber(height)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -height %d", height->valueint);
        }
    }

    // Background color - check bindings FIRST
    const char* resolved_bg = resolve_property_value(component, data_item, "background", value_buffer, sizeof(value_buffer));
    const char* bg_value = NULL;

    if (resolved_bg) {
        // Use the resolved value from bindings
        bg_value = resolved_bg;
    } else {
        // Fall back to direct background field
        cJSON* background_obj = cJSON_GetObjectItem(component, "background");
        if (background_obj && cJSON_IsString(background_obj)) {
            bg_value = background_obj->valuestring;
        }
    }

    cJSON* type = cJSON_GetObjectItem(component, "type");
    const char* widget_type = (type && cJSON_IsString(type)) ? type->valuestring : "";
    bool is_text = (strcmp(widget_type, "Text") == 0 || strcmp(widget_type, "Label") == 0);

    if (bg_value) {
        const char* tk_color = convert_to_tk_color(bg_value);
        if (tk_color) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -bg %s", tk_color);
        } else if (is_text && parent_bg) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -bg %s", parent_bg);
        }
    } else if (is_text && parent_bg) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -bg %s", parent_bg);
    }

    // Foreground color (text color)
    cJSON* color = cJSON_GetObjectItem(component, "color");
    if (color && cJSON_IsString(color)) {
        const char* tk_color = convert_to_tk_color(color->valuestring);
        if (tk_color) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -fg %s", tk_color);
        }
    }

    // Border
    cJSON* border = cJSON_GetObjectItem(component, "border");
    if (border && cJSON_IsObject(border)) {
        cJSON* border_width = cJSON_GetObjectItem(border, "width");
        cJSON* border_color = cJSON_GetObjectItem(border, "color");

        if (border_width && cJSON_IsNumber(border_width)) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -borderwidth %d", border_width->valueint);
        }
        if (border_color && cJSON_IsString(border_color)) {
            append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " -relief raised");
        }
    }

    // Font
    cJSON* font = cJSON_GetObjectItem(component, "font");
    if (font && cJSON_IsString(font)) {
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " -font {%s}", font->valuestring);
    } else {
        if (type && cJSON_IsString(type)) {
            if (strcmp(type->valuestring, "Text") == 0 || strcmp(type->valuestring, "Label") == 0) {
                append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " -font /fonts/lucidasans/latin1.7.font");
            }
        }
    }

    if (is_text) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " -padx 5 -pady 5");
    }
}

// Process a component recursively
static bool process_component(LimboContext* ctx, cJSON* component, const char* parent_path, int depth, const char* parent_bg, const char* parent_type, LayoutProperties* parent_layout) {
    // Extract this component's layout for its children
    LayoutProperties my_layout = extract_layout_properties(component);
    cJSON* type = cJSON_GetObjectItem(component, "type");
    const char* my_type = (type && cJSON_IsString(type)) ? type->valuestring : NULL;

    // Check for static_block - skip and process children directly
    if (my_type && strcmp(my_type, "static_block") == 0) {
        cJSON* children = cJSON_GetObjectItem(component, "children");
        if (children && cJSON_IsArray(children)) {
            for (int i = 0; i < cJSON_GetArraySize(children); i++) {
                cJSON* child = cJSON_GetArrayItem(children, i);
                process_component(ctx, child, parent_path, depth, parent_bg, parent_type, parent_layout);
            }
        }
        return true;
    }

    // Check for For loop - generate runtime iteration
    if (my_type && strcmp(my_type, "For") == 0) {
        // For loops should use the PARENT's layout and type (since For itself has no layout)
        // The parent could be a Row, Column, etc.
        generate_for_loop_runtime(ctx, component, parent_path, depth, parent_bg, parent_type, parent_layout, ctx->source_structures);
        return true;
    }

    // Regular widget
    return generate_widget(ctx, component, parent_path, depth + 1, parent_bg, parent_type, parent_layout, -1, 0);
}

// Generate custom Limbo functions from logic_block
// NOTE: This function now only stores the handler code for later inlining
// It does NOT output the functions directly (which caused Limbo syntax errors)
static void generate_limbo_custom_functions(LimboContext* ctx, cJSON* logic_block) {
    if (!ctx || !logic_block) return;

    // Extract functions array from logic_block
    cJSON* functions = cJSON_GetObjectItem(logic_block, "functions");
    if (!functions || !cJSON_IsArray(functions)) {
        return;  // No functions defined
    }

    // Iterate through each function
    int func_count = cJSON_GetArraySize(functions);
    for (int i = 0; i < func_count; i++) {
        cJSON* func = cJSON_GetArrayItem(functions, i);
        if (!func) continue;

        // Get function name
        cJSON* name_obj = cJSON_GetObjectItem(func, "name");
        if (!name_obj || !name_obj->valuestring) continue;
        const char* func_name = name_obj->valuestring;

        // Skip if already processed
        if (is_handler_already_generated(ctx, func_name)) {
            continue;
        }

        // Get sources array
        cJSON* sources = cJSON_GetObjectItem(func, "sources");
        if (!sources || !cJSON_IsArray(sources)) continue;

        // Look for Limbo source
        const char* limbo_source = NULL;
        int source_count = cJSON_GetArraySize(sources);

        for (int j = 0; j < source_count; j++) {
            cJSON* src = cJSON_GetArrayItem(sources, j);
            if (!src) continue;

            cJSON* lang = cJSON_GetObjectItem(src, "language");
            if (!lang || !lang->valuestring) continue;

            if (strcmp(lang->valuestring, "limbo") == 0) {
                cJSON* src_code = cJSON_GetObjectItem(src, "source");
                if (src_code && src_code->valuestring) {
                    limbo_source = src_code->valuestring;
                    break;  // Found Limbo source, use it
                }
            }
        }

        // Extract and store the function body for inlining
        if (limbo_source) {
            mark_handler_generated(ctx, func_name);
            fprintf(stderr, "[LIMBO] Processing Limbo source for func %s\n", func_name);

            // Parse the function to extract the handler name and body
            // Expected format: "funcName: fn() {\n\tbody\n};"

            // Extract handler name first (before the colon)
            char handler_name_from_source[256] = {0};
            const char* colon_pos = strchr(limbo_source, ':');
            if (colon_pos && colon_pos > limbo_source) {
                size_t name_len = colon_pos - limbo_source;
                if (name_len < sizeof(handler_name_from_source) - 1) {
                    strncpy(handler_name_from_source, limbo_source, name_len);
                    handler_name_from_source[name_len] = '\0';

                    // Trim trailing whitespace from name
                    char* end = handler_name_from_source + name_len - 1;
                    while (end >= handler_name_from_source && (*end == ' ' || *end == '\t')) {
                        *end = '\0';
                        end--;
                    }
                    fprintf(stderr, "[LIMBO] Extracted handler name: '%s'\n", handler_name_from_source);
                }
            }

            // Extract the body by finding the content between { and }
            const char* body_start = strchr(limbo_source, '{');
            const char* body_end = strrchr(limbo_source, '}');

            if (body_start && body_end && body_end > body_start && handler_name_from_source[0] != '\0') {
                body_start++;  // Skip the '{'

                // Calculate body length
                size_t body_len = body_end - body_start;

                // Skip leading newlines/tabs
                while (body_len > 0 && (*body_start == '\n' || *body_start == '\r' || *body_start == '\t')) {
                    body_start++;
                    body_len--;
                }

                // Skip trailing whitespace (but keep semicolons - they're statement terminators in Limbo)
                while (body_len > 0 && (body_start[body_len - 1] == '\n' || body_start[body_len - 1] == '\r' ||
                                       body_start[body_len - 1] == ' ' || body_start[body_len - 1] == '\t' ||
                                       body_start[body_len - 1] == '}')) {
                    body_len--;
                }

                fprintf(stderr, "[LIMBO] Extracted body (len=%d): '%s'\n", (int)body_len, body_len > 0 ? body_start : "");

                // Find which handler number this function corresponds to
                if (body_len > 0) {
                    fprintf(stderr, "[LIMBO] Looking for handler with name '%s' (total handlers: %d)\n",
                            handler_name_from_source, event_handler_mapping_count);
                    for (int h = 0; h < event_handler_mapping_count; h++) {
                        fprintf(stderr, "[LIMBO] Checking handler %d: name='%s', number=%d\n",
                                h, event_handlers[h].handler_name, event_handlers[h].handler_number);
                        if (event_handlers[h].handler_name &&
                            strcmp(event_handlers[h].handler_name, handler_name_from_source) == 0) {

                            fprintf(stderr, "[LIMBO] MATCH! Storing handler code for number %d\n", event_handlers[h].handler_number);

                            // Allocate and store the body
                            char* body = malloc(body_len + 1);
                            if (body) {
                                strncpy(body, body_start, body_len);
                                body[body_len] = '\0';
                                set_event_handler_code(event_handlers[h].handler_number, body);
                                free(body);  // set_event_handler_code makes a copy
                            }
                            break;
                        }
                    }
                }
            } else {
                fprintf(stderr, "[LIMBO] Failed to extract body: colon=%p, body_start=%p, body_end=%p, name='%s'\n",
                        (void*)colon_pos, (void*)body_start, (void*)body_end, handler_name_from_source);
            }
        }
    }
}

// Generate module header
static bool generate_module_header(LimboContext* ctx, const char* module_name) {
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "implement %s;\n\n", module_name);

    // Always include core modules
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "include \"sys.m\";\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\tsys: Sys;\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "include \"draw.m\";\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\tdraw: Draw;\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "include \"tk.m\";\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttk: Tk;\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "include \"tkclient.m\";\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttkclient: Tkclient;\n");

    // Include additional modules from config
    if (ctx->modules && ctx->modules_count > 0) {
        for (int i = 0; i < ctx->modules_count; i++) {
            const char* module_alias = ctx->modules[i];
            if (!module_alias) continue;

            // Generate include: include "string.m";
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "include \"%s.m\";\n", module_alias);

            // Generate capitalized module name for type declaration
            // Convert "string" -> "String"
            char module_type[256];
            snprintf(module_type, sizeof(module_type), "%s", module_alias);
            if (module_type[0]) {
                module_type[0] = toupper(module_type[0]);
            }

            // Generate alias:     string: String;
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "\t%s: %s;\n", module_alias, module_type);
        }
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");

    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "%s: module {\n", module_name);
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\tinit: fn(ctxt: ref Draw->Context, argv: list of string);\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "};\n\n");

    return true;
}

// Generate init function start
static bool generate_init_start(LimboContext* ctx, const char* title) {
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "init(ctxt: ref Draw->Context, argv: list of string) {\n");

    if (ctx->include_comments) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t# Load required modules\n");
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\tsys = load Sys Sys->PATH;\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\tdraw = load Draw Draw->PATH;\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttk = load Tk Tk->PATH;\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttkclient = load Tkclient Tkclient->PATH;\n");

    // Load additional modules from config
    if (ctx->modules && ctx->modules_count > 0) {
        for (int i = 0; i < ctx->modules_count; i++) {
            const char* module_alias = ctx->modules[i];
            if (!module_alias) continue;

            // Generate capitalized module name
            // Convert "string" -> "String"
            char module_type[256];
            snprintf(module_type, sizeof(module_type), "%s", module_alias);
            if (module_type[0]) {
                module_type[0] = toupper(module_type[0]);
            }

            // Generate load statement: string = load String String->PATH;
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                      "\t%s = load %s %s->PATH;\n", module_alias, module_type, module_type);
        }
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttkclient->init();\n\n");

    if (ctx->include_comments) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t# Create toplevel window\n");
    }

    const char* window_title = title ? title : "Kryon App";
    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
              "\t(t, titlech) := tkclient->toplevel(ctxt, \"\", \"%s\", Tkclient->Appl);\n\n",
              window_title);

    if (ctx->include_comments) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t# Create widgets\n");
    }
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");

    return true;
}

// Generate event loop
static bool generate_event_loop(LimboContext* ctx, int num_event_handlers) {
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");
    if (ctx->include_comments) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t# Update display\n");
    }
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttk->cmd(t, \"update\");\n\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttkclient->onscreen(t, nil);\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttkclient->startinput(t, \"kbd\" :: \"ptr\" :: nil);\n\n");

    if (num_event_handlers > 0) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\teventch := chan of string;\n");
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\ttk->namechan(t, eventch, \"eventch\");\n\n");
    }

    if (ctx->include_comments) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t# Event loop\n");
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\tfor(;;) alt {\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\ts := <-t.ctxt.kbd =>\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\t\ttk->keyboard(t, s);\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\ts := <-t.ctxt.ptr =>\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\t\ttk->pointer(t, *s);\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\ts := <-t.ctxt.ctl or s = <-t.wreq or s = <-titlech =>\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\t\ttkclient->wmctl(t, s);\n");

    if (num_event_handlers > 0) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\ts := <-eventch =>\n");
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\t\tif (s != nil) {\n");

        for (int i = 1; i <= num_event_handlers; i++) {
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                      "\t\t\t\tif (s == \"click%d\") {\n", i);

            // Get handler code for this number
            const char* handler_code = get_handler_code_by_number(i);

            if (handler_code) {
                // Inline the handler code
                // Split by newlines and add proper indentation
                char* code_copy = strdup(handler_code);
                char* line = strtok(code_copy, "\n");
                while (line) {
                    // Skip leading whitespace from the original line
                    const char* trimmed = line;
                    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

                    // Add indented line
                    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\t\t\t\t");
                    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, trimmed);
                    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");

                    line = strtok(NULL, "\n");
                }
                free(code_copy);
            } else {
                // Fallback to debug print
                const char* handler_name = get_handler_name_by_number(i);
                if (handler_name) {
                    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                              "\t\t\t\t\tsys->print(\"Event handler: %s\\n\");\n", handler_name);
                } else {
                    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                              "\t\t\t\t\tsys->print(\"Button %d clicked\\n\");\n", i);
                }
            }

            append_string(&ctx->buffer, &ctx->size, &ctx->capacity,
                         "\t\t\t\t}\n");
        }

        append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t\t\t}\n");
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\t}\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "}\n");

    return true;
}

// Generate Limbo code from KIR JSON
char* limbo_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    cJSON* kir_root = cJSON_Parse(kir_json);
    if (!kir_root) return NULL;

    // Extract logic_block for custom functions
    cJSON* logic_block = codegen_extract_logic_block(kir_root);

    LimboContext* ctx = create_limbo_context(NULL);
    if (!ctx) {
        cJSON_Delete(kir_root);
        return NULL;
    }

    // Extract module name (default to "KryonApp")
    const char* module_name = ctx->module_name ? ctx->module_name : "KryonApp";

    // Get window title from app section
    cJSON* app = cJSON_GetObjectItem(kir_root, "app");
    const char* window_title = NULL;
    if (app) {
        cJSON* title_obj = cJSON_GetObjectItem(app, "windowTitle");
        if (title_obj && title_obj->type == cJSON_String) {
            window_title = title_obj->valuestring;
        }
    }

    // Generate module header
    generate_module_header(ctx, module_name);

    // Generate init function start
    generate_init_start(ctx, window_title);

    // Process root component (this registers event handlers)
    cJSON* root = cJSON_GetObjectItem(kir_root, "root");
    if (root) {
        process_component(ctx, root, "", 0, NULL, NULL, NULL);
    }

    // NOW generate custom Limbo functions (AFTER event handlers are registered)
    // This extracts and stores the handler code for inlining
    generate_limbo_custom_functions(ctx, logic_block);

    // Generate event loop (this inlines the handler code)
    generate_event_loop(ctx, ctx->event_handler_counter);

    // Cleanup handler mappings
    cleanup_event_handler_mappings();

    // Extract buffer
    char* result = ctx->buffer;
    ctx->buffer = NULL;  // Don't free the buffer when destroying context

    destroy_limbo_context(ctx);
    cJSON_Delete(kir_root);

    return result;
}

// Generate Limbo file from KIR file
bool limbo_codegen_generate(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) return false;

    // Read KIR file directly as JSON (NO IR, NO EXPANSION)
    printf("[LIMBO CODEGEN] Loading KIR file: %s\n", kir_path); fflush(stdout);

    FILE* kir_file = fopen(kir_path, "r");
    if (!kir_file) {
        fprintf(stderr, "Failed to open KIR file: %s\n", kir_path);
        return false;
    }

    // Read entire file
    fseek(kir_file, 0, SEEK_END);
    long file_size = ftell(kir_file);
    fseek(kir_file, 0, SEEK_SET);

    char* kir_json = malloc(file_size + 1);
    if (!kir_json) {
        fclose(kir_file);
        return false;
    }

    size_t read_size = fread(kir_json, 1, file_size, kir_file);
    kir_json[read_size] = '\0';
    fclose(kir_file);

    printf("[LIMBO CODEGEN] KIR file loaded (%zu bytes)\n", read_size); fflush(stdout);

    // Generate Limbo code directly from JSON
    char* limbo_code = limbo_codegen_from_json(kir_json);
    free(kir_json);

    if (!limbo_code) {
        fprintf(stderr, "Failed to generate Limbo code\n");
        return false;
    }

    // Write output
    FILE* output_file = fopen(output_path, "w");
    if (!output_file) {
        fprintf(stderr, "Failed to open output file: %s\n", output_path);
        free(limbo_code);
        return false;
    }

    fputs(limbo_code, output_file);
    fclose(output_file);
    free(limbo_code);

    return true;
}

// Generate with options
bool limbo_codegen_generate_with_options(const char* kir_path,
                                          const char* output_path,
                                          LimboCodegenOptions* options) {
    if (!kir_path || !output_path) return false;

    // Read KIR file directly as JSON (NO IR, NO EXPANSION)
    FILE* kir_file = fopen(kir_path, "r");
    if (!kir_file) {
        fprintf(stderr, "Failed to open KIR file: %s\n", kir_path);
        return false;
    }

    // Read entire file
    fseek(kir_file, 0, SEEK_END);
    long file_size = ftell(kir_file);
    fseek(kir_file, 0, SEEK_SET);

    char* kir_json = malloc(file_size + 1);
    if (!kir_json) {
        fclose(kir_file);
        return false;
    }

    size_t read_size = fread(kir_json, 1, file_size, kir_file);
    kir_json[read_size] = '\0';
    fclose(kir_file);

    // Parse JSON
    cJSON* kir_root = cJSON_Parse(kir_json);
    free(kir_json);

    if (!kir_root) {
        fprintf(stderr, "Failed to parse KIR JSON\n");
        return false;
    }

    // Extract logic_block for custom functions
    cJSON* logic_block = codegen_extract_logic_block(kir_root);

    LimboContext* ctx = create_limbo_context(options);
    if (!ctx) {
        cJSON_Delete(kir_root);
        return false;
    }

    // Extract module name (default to "KryonApp")
    const char* module_name = ctx->module_name ? ctx->module_name : "KryonApp";

    // Get window title from app section
    cJSON* app = cJSON_GetObjectItem(kir_root, "app");
    const char* window_title = NULL;
    if (app) {
        cJSON* title_obj = cJSON_GetObjectItem(app, "windowTitle");
        if (title_obj && cJSON_IsString(title_obj)) {
            window_title = title_obj->valuestring;
        }
    }

    // Extract source_structures for const data
    cJSON* source_structures_json = cJSON_GetObjectItem(kir_root, "source_structures");

    // Store in context for For loop resolution
    ctx->source_structures = source_structures_json;

    // Generate module header
    generate_module_header(ctx, module_name);

    // Generate data arrays from const declarations
    generate_data_arrays(ctx, source_structures_json);
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");

    // Generate init function start
    generate_init_start(ctx, window_title);

    // Process root component (this registers event handlers)
    cJSON* root = cJSON_GetObjectItem(kir_root, "root");
    if (root) {
        process_component(ctx, root, "", 0, NULL, NULL, NULL);
    }

    // NOW generate custom Limbo functions (AFTER event handlers are registered)
    generate_limbo_custom_functions(ctx, logic_block);

    // Generate event loop (this inlines the handler code)
    generate_event_loop(ctx, ctx->event_handler_counter);

    // Cleanup handler mappings
    cleanup_event_handler_mappings();

    cJSON_Delete(kir_root);

    // Write output file
    FILE* output_file = fopen(output_path, "w");
    if (!output_file) {
        fprintf(stderr, "Failed to open output file: %s\n", output_path);
        destroy_limbo_context(ctx);
        return false;
    }

    fputs(ctx->buffer, output_file);
    fclose(output_file);

    destroy_limbo_context(ctx);
    return true;
}

// Generate multiple files (currently just generates one file)
bool limbo_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) return false;

    // Create output directory if it doesn't exist
    if (!codegen_mkdir_p(output_dir)) {
        fprintf(stderr, "Failed to create output directory: %s\n", output_dir);
        return false;
    }

    // For now, just generate a single main.b file
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/main.b", output_dir);

    return limbo_codegen_generate(kir_path, output_path);
}
