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
 * Widget Conversion
 * ============================================================================ */

/**
 * Convert a Tcl widget to KIR component JSON
 */
static cJSON* convert_widget(TclWidget* widget) {
    if (!widget) return NULL;

    // Get KIR widget type
    const char* kir_type = tcl_widget_to_kir_type(widget->base.command_name);

    cJSON* component = cJSON_CreateObject();
    cJSON_AddNumberToObject(component, "id", generate_id());
    cJSON_AddStringToObject(component, "type", kir_type);

    // Add widget path as metadata for debugging
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

    return component;
}

/* ============================================================================
 * Widget Hierarchy Building
 * ============================================================================ */

/**
 * Build widget hierarchy from flat list of widgets
 * Returns the root widget (usually "." or deepest common parent)
 */
static cJSON* build_widget_hierarchy(TclWidget** widgets, int widget_count) {
    if (!widgets || widget_count == 0) {
        return cJSON_CreateObject();
    }

    // For now, create a simple hierarchy
    // TODO: Implement proper parent-child relationship building

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "Container");
    cJSON_AddNumberToObject(root, "id", generate_id());
    cJSON_AddStringToObject(root, "background", "#00000000");
    cJSON_AddNumberToObject(root, "lineHeight", 1.5);

    cJSON* children_array = cJSON_CreateArray();

    // Convert all widgets and add to root
    for (int i = 0; i < widget_count; i++) {
        cJSON* widget = convert_widget(widgets[i]);
        if (widget) {
            cJSON_AddItemToArray(children_array, widget);
        }
    }

    if (cJSON_GetArraySize(children_array) > 0) {
        cJSON_AddItemToObject(root, "children", children_array);
    } else {
        cJSON_Delete(children_array);
    }

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
