/**
 * Tcl to KIR Converter Implementation
 * Converts Tcl AST to KIR JSON format
 */

#define _POSIX_C_SOURCE 200809L

#include "tcl_to_ir.h"
#include "tcl_parser.h"
#include "tcl_widget_registry.h"
#include "../../../codegens/codegen_common.h"
#include "../../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declaration - set_error is defined in tcl_parser.c */
extern void set_error(TclParseError code, const char* message);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Generate next unique ID
 */
static int next_id = 1;

static int generate_id(void) {
    return next_id++;
}

static void reset_id_generator(void) {
    next_id = 1;
}

/**
 * Parse widget path to extract depth and parent
 * Example: ".w1.w2.w3" -> depth=3, parent=".w1.w2"
 */
static int parse_widget_depth(const char* path) {
    if (!path || path[0] != '.') return 0;

    int depth = 0;
    for (const char* p = path; *p; p++) {
        if (*p == '.') depth++;
    }

    return depth;
}

/**
 * Get parent path from widget path
 * Example: ".w1.w2.w3" -> ".w1.w2"
 * Example: ".w1" -> "."
 */
static char* get_parent_path(const char* path) {
    if (!path || strcmp(path, ".") == 0) {
        return strdup(".");
    }

    // Find last dot
    const char* last_dot = strrchr(path, '.');
    if (!last_dot || last_dot == path) {
        return strdup(".");
    }

    size_t len = last_dot - path;
    char* parent = malloc(len + 1);
    if (parent) {
        memcpy(parent, path, len);
        parent[len] = '\0';
    }

    return parent;
}

/**
 * Parse widget path to get depth and parent path
 * Example: ".w1.w2.w3" -> depth=4, parent=".w1.w2"
 */
static void parse_widget_path(const char* path, int* out_depth, char** out_parent) {
    if (!path) {
        if (out_depth) *out_depth = 0;
        if (out_parent) *out_parent = strdup(".");
        return;
    }

    if (out_depth) {
        *out_depth = parse_widget_depth(path);
    }

    if (out_parent) {
        *out_parent = get_parent_path(path);
    }
}

/**
 * Convert Tcl color to KIR color format
 * Tcl colors can be: names (red, blue), hex (#ff0000), or RGB
 */
static const char* convert_color(const char* tcl_color) {
    if (!tcl_color) return "#000000";

    // If already hex, return as-is
    if (tcl_color[0] == '#') {
        return tcl_color;
    }

    // TODO: Map color names to hex
    // For now, return as-is and let KIR handle it
    return tcl_color;
}

/* ============================================================================
 * Widget Hierarchy Building
 * ============================================================================ */

/**
 * Parse widget path to get depth and parent path
 */
static void parse_widget_path(const char* path, int* out_depth, char** out_parent);

/* ============================================================================
 * Widget Conversion
 * ============================================================================ */

/**
 * Convert a Tcl widget to KIR component JSON (with children)
 */
static cJSON* convert_widget_with_children(TclWidget* widget, TclWidget** all_widgets, int widget_count, bool* processed);

/**
 * Find and convert children of a widget
 */
static cJSON* find_and_convert_children(TclWidget* parent, TclWidget** all_widgets, int widget_count, bool* processed) {
    cJSON* children = cJSON_CreateArray();
    if (!children) return NULL;

    const char* parent_path = parent->widget_path;
    size_t parent_path_len = strlen(parent_path);

    for (int i = 0; i < widget_count; i++) {
        if (processed[i]) continue;  // Already added to a parent

        TclWidget* child = all_widgets[i];
        const char* child_path = child->widget_path;

        // Check if this widget is a direct child
        // Direct child means: path starts with parent_path + "."
        // and doesn't contain any more dots after that
        if (strncmp(child_path, parent_path, parent_path_len) == 0 &&
            child_path[parent_path_len] == '.') {

            // Check if there are no more dots (direct child, not grandchild)
            bool has_more_dots = false;
            for (const char* p = child_path + parent_path_len + 1; *p; p++) {
                if (*p == '.') {
                    has_more_dots = true;
                    break;
                }
            }

            if (!has_more_dots) {
                // This is a direct child
                cJSON* child_json = convert_widget_with_children(child, all_widgets, widget_count, processed);
                if (child_json) {
                    cJSON_AddItemToArray(children, child_json);
                    processed[i] = true;  // Mark as processed
                }
            }
        }
    }

    if (cJSON_GetArraySize(children) > 0) {
        return children;
    } else {
        cJSON_Delete(children);
        return NULL;
    }
}

/**
 * Convert a Tcl widget to KIR component JSON
 */
static cJSON* convert_widget_with_children(TclWidget* widget, TclWidget** all_widgets, int widget_count, bool* processed) {
    if (!widget) return NULL;

    // Get KIR widget type
    const char* kir_type = tcl_widget_to_kir_type(widget->base.command_name);

    cJSON* component = cJSON_CreateObject();
    cJSON_AddNumberToObject(component, "id", generate_id());
    cJSON_AddStringToObject(component, "type", kir_type);

    // Add widget path as metadata
    cJSON_AddStringToObject(component, "_widgetPath", widget->widget_path);

    // Convert options to properties
    if (widget->options) {
        cJSON* opt = widget->options;
        while (opt) {
            if (opt->string && opt->valuestring) {
                // Special handling for colors
                if (strcmp(opt->string, "background") == 0 ||
                    strcmp(opt->string, "color") == 0) {
                    cJSON_AddStringToObject(component, opt->string,
                                          convert_color(opt->valuestring));
                }
                // Text content
                else if (strcmp(opt->string, "text") == 0) {
                    cJSON_AddStringToObject(component, "text", opt->valuestring);
                }
                // Numbers
                else if (strcmp(opt->string, "width") == 0 ||
                         strcmp(opt->string, "height") == 0) {
                    cJSON_AddNumberToObject(component, opt->string,
                                          atof(opt->valuestring));
                }
                // Everything else
                else {
                    cJSON_AddStringToObject(component, opt->string, opt->valuestring);
                }
            }
            opt = opt->next;
        }
    }

    // Add default properties
    if (!cJSON_GetObjectItem(component, "background")) {
        cJSON_AddStringToObject(component, "background", "#00000000");
    }

    if (!cJSON_GetObjectItem(component, "lineHeight")) {
        cJSON_AddNumberToObject(component, "lineHeight", 1.5);
    }

    // Find and add children
    cJSON* children = find_and_convert_children(widget, all_widgets, widget_count, processed);
    if (children) {
        cJSON_AddItemToObject(component, "children", children);
    }

    return component;
}

/**
 * Build widget hierarchy from flat list of widgets
 * Creates proper parent-child relationships based on widget paths
 */
static cJSON* build_widget_hierarchy(TclWidget** widgets, int widget_count) {
    if (!widgets || widget_count == 0) {
        // Return empty root container
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "Container");
        cJSON_AddNumberToObject(root, "id", generate_id());
        cJSON_AddStringToObject(root, "background", "#00000000");
        cJSON_AddNumberToObject(root, "lineHeight", 1.5);
        return root;
    }

    // Track which widgets have been processed
    bool* processed = calloc(widget_count, sizeof(bool));
    if (!processed) return cJSON_CreateObject();

    // Find root-level widgets (direct children of ".")
    cJSON* root_children = cJSON_CreateArray();
    int root_id = generate_id();

    for (int i = 0; i < widget_count; i++) {
        if (processed[i]) continue;

        TclWidget* w = widgets[i];
        int depth;
        char* parent_path;

        parse_widget_path(w->widget_path, &depth, &parent_path);

        // Root-level widgets have depth 2 (".w1")
        if (depth == 2 || (depth == 1 && strcmp(w->widget_path, ".") == 0)) {
            cJSON* widget_json = convert_widget_with_children(w, widgets, widget_count, processed);
            if (widget_json) {
                cJSON_AddItemToArray(root_children, widget_json);
                processed[i] = true;
            }
        }

        if (parent_path) free(parent_path);
    }

    // Create root container
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", root_id);
    cJSON_AddStringToObject(root, "type", "Container");
    cJSON_AddStringToObject(root, "background", "#00000000");
    cJSON_AddNumberToObject(root, "lineHeight", 1.5);

    if (cJSON_GetArraySize(root_children) > 0) {
        cJSON_AddItemToObject(root, "children", root_children);
    } else {
        cJSON_Delete(root_children);
    }

    free(processed);
    return root;
}

/* ============================================================================
 * Main Conversion Functions
 * ============================================================================ */

cJSON* tcl_ast_to_kir(TclAST* ast) {
    if (!ast) {
        return NULL;
    }

    reset_id_generator();

    // Create KIR structure
    cJSON* kir = cJSON_CreateObject();
    cJSON_AddStringToObject(kir, "format", "kir");

    // Metadata
    cJSON* metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(metadata, "source_language", "tcl");
    cJSON_AddStringToObject(metadata, "compiler_version", "kryon-1.0.0");
    cJSON_AddItemToObject(kir, "metadata", metadata);

    // App metadata (empty for now)
    cJSON* app = cJSON_CreateObject();
    cJSON_AddItemToObject(kir, "app", app);

    // Logic block (empty for now)
    cJSON* logic_block = cJSON_CreateObject();
    cJSON_AddItemToObject(kir, "logic_block", logic_block);

    // Build widget hierarchy
    cJSON* root = build_widget_hierarchy(ast->widgets, ast->widget_count);
    cJSON_AddItemToObject(kir, "root", root);

    return kir;
}

cJSON* tcl_parse_to_kir(const char* tcl_source) {
    if (!tcl_source) {
        return NULL;
    }

    // Parse Tcl source
    TclAST* ast = tcl_parse(tcl_source);
    if (!ast) {
        return NULL;
    }

    // Convert to KIR
    cJSON* kir = tcl_ast_to_kir(ast);

    // Cleanup
    tcl_ast_free(ast);

    return kir;
}

cJSON* tcl_ast_to_kir_ex(TclAST* ast, const TclToKirOptions* options) {
    // For now, just call the basic version
    (void)options;  // Unused
    return tcl_ast_to_kir(ast);
}

/* ============================================================================
 * File Parsing
 * ============================================================================ */

cJSON* tcl_parse_file_to_kir(const char* filepath) {
    if (!filepath) {
        return NULL;
    }

    // Read file
    FILE* f = fopen(filepath, "r");
    if (!f) {
        char error[256];
        snprintf(error, sizeof(error), "Failed to open file: %s", filepath);
        set_error(TCL_PARSE_ERROR_IO, error);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read content
    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        set_error(TCL_PARSE_ERROR_MEMORY, "Failed to allocate buffer");
        return NULL;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Parse
    cJSON* kir = tcl_parse_to_kir(content);

    free(content);

    return kir;
}

/* ============================================================================
 * Validation
 * ============================================================================ */

bool tcl_validate_syntax(const char* tcl_source) {
    if (!tcl_source) return false;

    TclAST* ast = tcl_parse(tcl_source);
    if (!ast) return false;

    tcl_ast_free(ast);
    return true;
}

cJSON* tcl_extract_widget_tree(const char* tcl_source) {
    if (!tcl_source) return NULL;

    TclAST* ast = tcl_parse(tcl_source);
    if (!ast) return NULL;

    cJSON* tree = cJSON_CreateArray();
    for (int i = 0; i < ast->widget_count; i++) {
        TclWidget* w = ast->widgets[i];
        cJSON* widget_info = cJSON_CreateObject();
        cJSON_AddStringToObject(widget_info, "type", w->base.command_name);
        cJSON_AddStringToObject(widget_info, "path", w->widget_path);
        cJSON_AddItemToObject(widget_info, "options",
                             cJSON_Duplicate(w->options, true));
        cJSON_AddItemToArray(tree, widget_info);
    }

    tcl_ast_free(ast);
    return tree;
}

/* ============================================================================
 * External declarations
 * ============================================================================ */

extern TclAST* tcl_parse(const char* source);
extern void tcl_ast_free(TclAST* ast);
