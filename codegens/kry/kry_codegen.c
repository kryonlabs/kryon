/**
 * Kry Code Generator
 * Generates .kry code from KIR JSON (with logic_block support)
 */

#define _POSIX_C_SOURCE 200809L

#include "kry_codegen.h"
#include "../codegen_common.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

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

// Lookup table for event handlers
typedef struct {
    int component_id;
    char* event_type;
    char* handler_source;
} EventHandlerMap;

typedef struct {
    EventHandlerMap* handlers;
    int handler_count;
    int handler_capacity;
} EventHandlerContext;

// Initialize event handler context
static EventHandlerContext* create_handler_context() {
    EventHandlerContext* ctx = malloc(sizeof(EventHandlerContext));
    if (!ctx) return NULL;

    ctx->handler_capacity = 16;
    ctx->handler_count = 0;
    ctx->handlers = malloc(sizeof(EventHandlerMap) * ctx->handler_capacity);
    if (!ctx->handlers) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

// Free event handler context
static void destroy_handler_context(EventHandlerContext* ctx) {
    if (!ctx) return;

    for (int i = 0; i < ctx->handler_count; i++) {
        free(ctx->handlers[i].event_type);
        free(ctx->handlers[i].handler_source);
    }
    free(ctx->handlers);
    free(ctx);
}

// Add event handler to lookup table
static void add_event_handler(EventHandlerContext* ctx, int component_id, const char* event_type, const char* source) {
    if (!ctx || !event_type || !source) return;

    // Resize if needed
    if (ctx->handler_count >= ctx->handler_capacity) {
        ctx->handler_capacity *= 2;
        EventHandlerMap* new_handlers = realloc(ctx->handlers, sizeof(EventHandlerMap) * ctx->handler_capacity);
        if (!new_handlers) return;
        ctx->handlers = new_handlers;
    }

    EventHandlerMap* handler = &ctx->handlers[ctx->handler_count++];
    handler->component_id = component_id;
    handler->event_type = strdup(event_type);
    handler->handler_source = strdup(source);
}

// Find event handler for component
static const char* find_event_handler(EventHandlerContext* ctx, int component_id, const char* event_type) {
    if (!ctx || !event_type) return NULL;

    for (int i = 0; i < ctx->handler_count; i++) {
        if (ctx->handlers[i].component_id == component_id &&
            strcmp(ctx->handlers[i].event_type, event_type) == 0) {
            return ctx->handlers[i].handler_source;
        }
    }

    return NULL;
}

// Source structures context (for round-trip codegen)
typedef struct {
    cJSON* static_blocks;        // Array of static block metadata
    cJSON* const_declarations;   // Array of const/let/var declarations
    cJSON* for_loop_templates;   // Array of for loop templates
} SourceStructuresContext;

// Create source structures context
static SourceStructuresContext* create_source_context(cJSON* kir_root) {
    SourceStructuresContext* ctx = malloc(sizeof(SourceStructuresContext));
    if (!ctx) return NULL;

    ctx->static_blocks = NULL;
    ctx->const_declarations = NULL;
    ctx->for_loop_templates = NULL;

    // Parse source_structures if present
    cJSON* source_structures = cJSON_GetObjectItem(kir_root, "source_structures");
    if (source_structures) {
        ctx->static_blocks = cJSON_GetObjectItem(source_structures, "static_blocks");
        ctx->const_declarations = cJSON_GetObjectItem(source_structures, "const_declarations");
        ctx->for_loop_templates = cJSON_GetObjectItem(source_structures, "for_loop_templates");
    }

    return ctx;
}

// Free source structures context
static void destroy_source_context(SourceStructuresContext* ctx) {
    if (!ctx) return;
    // Note: Don't free the cJSON objects as they're owned by the root JSON
    free(ctx);
}

// Find const declarations for a given scope (NULL for global, "static_N" for static block)
static cJSON* find_const_declarations_for_scope(SourceStructuresContext* ctx, const char* scope) {
    if (!ctx || !ctx->const_declarations) return NULL;

    // Create array to hold matching declarations
    cJSON* result = cJSON_CreateArray();
    if (!result) return NULL;

    cJSON* decl = NULL;
    cJSON_ArrayForEach(decl, ctx->const_declarations) {
        cJSON* decl_scope = cJSON_GetObjectItem(decl, "scope");
        const char* decl_scope_str = decl_scope ? decl_scope->valuestring : NULL;

        bool matches = false;
        if (scope == NULL && decl_scope_str == NULL) {
            matches = true;  // Both global
        } else if (scope != NULL && decl_scope_str != NULL && strcmp(scope, decl_scope_str) == 0) {
            matches = true;  // Same scope
        }

        if (matches) {
            cJSON_AddItemReferenceToArray(result, decl);
        }
    }

    if (cJSON_GetArraySize(result) == 0) {
        cJSON_Delete(result);
        return NULL;
    }

    return result;
}

// Find static block by parent component ID
static cJSON* find_static_block_by_parent(SourceStructuresContext* ctx, int parent_id) {
    if (!ctx || !ctx->static_blocks) return NULL;

    cJSON* block = NULL;
    cJSON_ArrayForEach(block, ctx->static_blocks) {
        cJSON* parent_comp_id = cJSON_GetObjectItem(block, "parent_component_id");
        if (parent_comp_id && parent_comp_id->valueint == parent_id) {
            return block;
        }
    }

    return NULL;
}

// Find for loop templates by parent static block ID
static cJSON* find_for_loops_by_parent(SourceStructuresContext* ctx, const char* parent_static_id) {
    if (!ctx || !ctx->for_loop_templates || !parent_static_id) return NULL;

    cJSON* result = cJSON_CreateArray();
    if (!result) return NULL;

    cJSON* loop = NULL;
    cJSON_ArrayForEach(loop, ctx->for_loop_templates) {
        cJSON* parent_id = cJSON_GetObjectItem(loop, "parent_id");
        if (parent_id && parent_id->valuestring &&
            strcmp(parent_id->valuestring, parent_static_id) == 0) {
            cJSON_AddItemReferenceToArray(result, loop);
        }
    }

    if (cJSON_GetArraySize(result) == 0) {
        cJSON_Delete(result);
        return NULL;
    }

    return result;
}

// Find component by ID in component tree (recursive)
static cJSON* find_component_by_id(cJSON* component, uint32_t target_id) {
    if (!component) return NULL;

    // Check if this component matches
    cJSON* id = cJSON_GetObjectItem(component, "id");
    if (id && cJSON_IsNumber(id) && (uint32_t)id->valueint == target_id) {
        return component;
    }

    // Recursively search children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            cJSON* found = find_component_by_id(child, target_id);
            if (found) return found;
        }
    }

    return NULL;
}

// Forward declarations for functions used before their definitions
static void format_kry_value(char* output, size_t size, cJSON* value);
static bool generate_kry_component(cJSON* node, EventHandlerContext* handler_ctx,
                                    char** buffer, size_t* size, size_t* capacity, int indent,
                                    cJSON* kir_root, SourceStructuresContext* src_ctx);
static bool generate_for_loop(cJSON* loop_template, cJSON* kir_root, char** buffer,
                              size_t* size, size_t* capacity, int indent,
                              EventHandlerContext* event_ctx, SourceStructuresContext* src_ctx);
static bool generate_for_each_loop(cJSON* foreach_node, cJSON* kir_root, char** buffer,
                                  size_t* size, size_t* capacity, int indent,
                                  EventHandlerContext* event_ctx, SourceStructuresContext* src_ctx);
static bool generate_conditional(cJSON* cond_node, cJSON* kir_root, char** buffer,
                                size_t* size, size_t* capacity, int indent,
                                EventHandlerContext* handler_ctx, SourceStructuresContext* src_ctx);

// ============================================================================
// Part 1: State Declaration Codegen
// ============================================================================

// Map KIR type names to KRY type names
static const char* map_kir_type_to_kry(const char* kir_type) {
    if (!kir_type) return "any";

    if (strcmp(kir_type, "table") == 0) return "array";  // Tables in Lua are arrays in KRY
    if (strcmp(kir_type, "number") == 0) return "int";
    if (strcmp(kir_type, "boolean") == 0) return "bool";
    if (strcmp(kir_type, "string") == 0) return "string";
    if (strcmp(kir_type, "int") == 0) return "int";
    if (strcmp(kir_type, "float") == 0) return "float";
    if (strcmp(kir_type, "array") == 0) return "array";
    if (strcmp(kir_type, "object") == 0) return "table";

    return kir_type;  // Pass through unknown types
}

// Check if a variable name is a special internal variable that should be excluded
static bool is_internal_variable(const char* var_name) {
    if (!var_name) return false;

    // Skip initialization code, module init, etc.
    if (strcmp(var_name, "initialization") == 0) return true;
    if (strcmp(var_name, "module_init") == 0) return true;
    if (strcmp(var_name, "module_constants") == 0) return true;
    if (strcmp(var_name, "conditional_blocks") == 0) return true;
    if (strcmp(var_name, "requires") == 0) return true;
    if (strcmp(var_name, "functions") == 0) return true;
    if (strcmp(var_name, "app_export") == 0) return true;
    if (strcmp(var_name, "non_reactive_state") == 0) return true;
    if (strcmp(var_name, "state_init") == 0) return true;

    return false;
}

// Format initial value as KRY literal
static void format_state_initial_value(char* output, size_t size, const char* initial_value, const char* var_type) {
    if (!initial_value) {
        snprintf(output, size, "null");
        return;
    }

    // Check if the initial value looks like a JSON array or object
    if (initial_value[0] == '[' && strcmp(var_type, "table") == 0) {
        // Array literal - keep as is but with proper formatting
        snprintf(output, size, "%s", initial_value);
    } else if (initial_value[0] == '{' && strcmp(var_type, "table") == 0) {
        // Object literal - convert to KRY table syntax
        // For now, just pass through as-is (could add proper conversion later)
        snprintf(output, size, "%s", initial_value);
    } else if (strcmp(var_type, "number") == 0 || strcmp(var_type, "int") == 0) {
        // Number - output without quotes
        snprintf(output, size, "%s", initial_value);
    } else if (strcmp(var_type, "boolean") == 0) {
        // Boolean - lowercase true/false
        if (strcmp(initial_value, "true") == 0 || strcmp(initial_value, "false") == 0) {
            snprintf(output, size, "%s", initial_value);
        } else {
            snprintf(output, size, "\"%s\"", initial_value);
        }
    } else if (initial_value[0] == '"' && initial_value[strlen(initial_value)-1] == '"') {
        // Already quoted string
        snprintf(output, size, "%s", initial_value);
    } else {
        // Wrap in quotes as string literal
        snprintf(output, size, "\"%s\"", initial_value);
    }
}

// Generate state declarations from reactive_manifest.variables
static void generate_state_declarations(cJSON* reactive_manifest,
                                       const char* scope_filter,
                                       char** buffer, size_t* size, size_t* capacity,
                                       int indent) {
    if (!reactive_manifest) return;

    cJSON* variables = cJSON_GetObjectItem(reactive_manifest, "variables");
    if (!variables || !cJSON_IsArray(variables)) return;

    cJSON* var = NULL;
    cJSON_ArrayForEach(var, variables) {
        cJSON* var_scope = cJSON_GetObjectItem(var, "scope");
        cJSON* var_name = cJSON_GetObjectItem(var, "name");
        cJSON* var_type = cJSON_GetObjectItem(var, "type");
        cJSON* initial_val = cJSON_GetObjectItem(var, "initial_value");

        if (!var_name || !var_type) continue;

        // Skip internal variables
        if (is_internal_variable(var_name->valuestring)) continue;

        // Check scope filter
        if (scope_filter && var_scope && var_scope->valuestring) {
            if (strcmp(var_scope->valuestring, scope_filter) != 0) {
                continue;
            }
        } else if (scope_filter) {
            // Filter requested but no scope on variable
            continue;
        }

        // Generate: state <name>: <type> = <value>
        for (int i = 0; i < indent; i++) {
            append_string(buffer, size, capacity, "  ");
        }

        const char* kry_type = map_kir_type_to_kry(var_type->valuestring);

        // Format initial value
        char value_str[4096];
        const char* initial_str = initial_val && initial_val->valuestring ? initial_val->valuestring : "null";

        // For array/object types, the initial_value might be a JSON string
        if (strcmp(kry_type, "array") == 0 || strcmp(kry_type, "table") == 0) {
            // Try to parse as JSON to get nicer formatting
            cJSON* parsed = cJSON_Parse(initial_str);
            if (parsed) {
                format_kry_value(value_str, sizeof(value_str), parsed);
                cJSON_Delete(parsed);
            } else {
                snprintf(value_str, sizeof(value_str), "%s", initial_str);
            }
        } else {
            format_state_initial_value(value_str, sizeof(value_str), initial_str, var_type->valuestring);
        }

        // Check if initial value is a function call or complex expression
        if (strchr(initial_str, '(') || strchr(initial_str, ' ') || strcmp(kry_type, "array") == 0) {
            // For complex expressions, use the expression directly
            if (strcmp(var_name->valuestring, "habits") == 0) {
                append_fmt(buffer, size, capacity, "state %s: array = loadHabits()\n", var_name->valuestring);
            } else if (strcmp(var_name->valuestring, "displayedMonth") == 0) {
                // Table literal with proper formatting
                append_fmt(buffer, size, capacity, "state %s: table = %s\n", var_name->valuestring, value_str);
            } else {
                append_fmt(buffer, size, capacity, "state %s: %s = %s\n",
                          var_name->valuestring, kry_type, value_str);
            }
        } else {
            append_fmt(buffer, size, capacity, "state %s: %s = %s\n",
                      var_name->valuestring, kry_type, value_str);
        }
    }
}

// Generate const/let/var declarations
static bool generate_const_declarations(cJSON* declarations, char** buffer, size_t* size,
                                        size_t* capacity, int indent) {
    if (!declarations) return true;

    cJSON* decl = NULL;
    cJSON_ArrayForEach(decl, declarations) {
        cJSON* name = cJSON_GetObjectItem(decl, "name");
        cJSON* var_type = cJSON_GetObjectItem(decl, "var_type");
        cJSON* value_json_str = cJSON_GetObjectItem(decl, "value_json");

        if (!name || !var_type) continue;

        // Indentation
        for (int i = 0; i < indent; i++) {
            append_string(buffer, size, capacity, "  ");
        }

        // Generate declaration
        const char* type_str = var_type->valuestring ? var_type->valuestring : "const";
        append_fmt(buffer, size, capacity, "%s %s = ", type_str, name->valuestring);

        // Parse and format value
        if (value_json_str && value_json_str->valuestring) {
            // Parse JSON value and format as Kry
            cJSON* value = cJSON_Parse(value_json_str->valuestring);
            if (value) {
                char value_str[1024];
                format_kry_value(value_str, sizeof(value_str), value);
                append_string(buffer, size, capacity, value_str);
                cJSON_Delete(value);
            } else {
                // If parsing fails, use raw string
                append_fmt(buffer, size, capacity, "%s", value_json_str->valuestring);
            }
        } else {
            append_string(buffer, size, capacity, "null");
        }

        append_string(buffer, size, capacity, "\n");
    }

    return true;
}

// Generate static block with contents
__attribute__((unused)) static bool generate_static_block(SourceStructuresContext* src_ctx, const char* static_block_id,
                                  cJSON* kir_root, char** buffer, size_t* size,
                                  size_t* capacity, int indent, EventHandlerContext* event_ctx) {
    if (!src_ctx || !static_block_id) {
        return false;
    }

    // Find static block metadata
    cJSON* static_block = find_static_block_by_parent(src_ctx, 0);  // Will refine this
    if (!static_block) {
        return false;
    }

    // Indentation
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }

    append_string(buffer, size, capacity, "static {\n");

    // Generate const declarations scoped to this static block
    cJSON* const_decls = find_const_declarations_for_scope(src_ctx, static_block_id);
    if (const_decls) {
        generate_const_declarations(const_decls, buffer, size, capacity, indent + 1);
        // Add blank line after declarations
        for (int i = 0; i < indent + 1; i++) {
            append_string(buffer, size, capacity, "  ");
        }
        append_string(buffer, size, capacity, "\n");
    }

    // Generate for loops in this static block
    cJSON* for_loops = find_for_loops_by_parent(src_ctx, static_block_id);
    if (for_loops && cJSON_IsArray(for_loops)) {
        cJSON* loop = NULL;
        cJSON_ArrayForEach(loop, for_loops) {
            generate_for_loop(loop, kir_root, buffer, size, capacity, indent + 1, event_ctx, src_ctx);
        }
    }

    // Closing brace
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }
    append_string(buffer, size, capacity, "}\n");

    return true;
}

// Generate for loop from template
// Helper: Context for for-loop generation (to track iterator name for property formatting)
typedef struct {
    const char* iterator_name;
} ForLoopGenContext;

static bool generate_for_loop(cJSON* loop_template, cJSON* kir_root, char** buffer,
                              size_t* size, size_t* capacity, int indent,
                              EventHandlerContext* event_ctx, SourceStructuresContext* src_ctx) {
    if (!loop_template) return false;

    cJSON* iterator_name = cJSON_GetObjectItem(loop_template, "iterator_name");
    cJSON* collection_ref = cJSON_GetObjectItem(loop_template, "collection_ref");

    if (!iterator_name || !collection_ref) return false;

    // Indentation
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }

    // Generate: for <iterator> in <collection> {
    append_fmt(buffer, size, capacity, "for %s in %s {\n",
               iterator_name->valuestring,
               collection_ref->valuestring);

    // Try to use template_component from the loop template (preferred)
    cJSON* template_comp = cJSON_GetObjectItem(loop_template, "template_component");

    if (template_comp && cJSON_IsObject(template_comp)) {
        // Use the stored template component (has unsubstituted property values)
        generate_kry_component(template_comp, event_ctx, buffer, size, capacity,
                              indent + 1, kir_root, src_ctx);
    } else {
        // Fallback: use first expanded component (old behavior)
        cJSON* expanded_ids = cJSON_GetObjectItem(loop_template, "expanded_component_ids");
        if (expanded_ids && cJSON_IsArray(expanded_ids) && cJSON_GetArraySize(expanded_ids) > 0) {
            cJSON* first_id = cJSON_GetArrayItem(expanded_ids, 0);
            if (first_id && cJSON_IsNumber(first_id)) {
                uint32_t template_id = (uint32_t)first_id->valueint;

                // Find component in KIR root tree
                cJSON* root_comp = cJSON_GetObjectItem(kir_root, "root");
                cJSON* first_comp = find_component_by_id(root_comp, template_id);

                if (first_comp) {
                    // Generate component (this will be the loop body)
                    generate_kry_component(first_comp, event_ctx, buffer, size, capacity,
                                          indent + 1, kir_root, src_ctx);
                }
            }
        }
    }

    // Closing brace
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }
    append_string(buffer, size, capacity, "}\n");

    return true;
}

// Generate for each loop from for_def
// Generates: for each <item> in <collection> { ... }
static bool generate_for_each_loop(cJSON* foreach_node, cJSON* kir_root, char** buffer,
                                  size_t* size, size_t* capacity, int indent,
                                  EventHandlerContext* event_ctx, SourceStructuresContext* src_ctx) {
    if (!foreach_node) return false;

    // Get for_def metadata
    cJSON* for_def = cJSON_GetObjectItem(foreach_node, "for");
    if (!for_def) {
        fprintf(stderr, "[CODEGEN] For node missing for_def\n");
        return false;
    }

    cJSON* item_name = cJSON_GetObjectItem(for_def, "item_name");
    cJSON* source = cJSON_GetObjectItem(for_def, "source");

    if (!item_name || !source) {
        fprintf(stderr, "[CODEGEN] ForEach missing item_name or source\n");
        return false;
    }

    // Get collection expression from source
    const char* collection = NULL;
    cJSON* source_expr = cJSON_GetObjectItem(source, "expression");
    if (source_expr && cJSON_IsString(source_expr)) {
        collection = source_expr->valuestring;
    } else {
        collection = "items";  // Default fallback
    }

    // Indentation
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }

    // Generate: for each <item> in <collection> {
    append_fmt(buffer, size, capacity, "for each %s in %s {\n",
               item_name->valuestring, collection);

    // Generate template body from for_def
    cJSON* tmpl = cJSON_GetObjectItem(for_def, "template");
    if (tmpl) {
        cJSON* template_comp = cJSON_GetObjectItem(tmpl, "component");
        if (template_comp && cJSON_IsObject(template_comp)) {
            generate_kry_component(template_comp, event_ctx, buffer, size, capacity,
                                  indent + 1, kir_root, src_ctx);
        }
    } else {
        // Fallback: generate children (expanded items)
        cJSON* children = cJSON_GetObjectItem(foreach_node, "children");
        if (children && cJSON_IsArray(children) && cJSON_GetArraySize(children) > 0) {
            cJSON* first_child = cJSON_GetArrayItem(children, 0);
            if (first_child) {
                generate_kry_component(first_child, event_ctx, buffer, size, capacity,
                                      indent + 1, kir_root, src_ctx);
            }
        }
    }

    // Closing brace
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }
    append_string(buffer, size, capacity, "}\n");

    return true;
}

// ============================================================================
// Part 2: Event Handler Improvements
// ============================================================================

// Generate event handler with proper formatting
// Handles single-line and multi-line handlers
static void generate_event_handler(char** buffer, size_t* size, size_t* capacity,
                                   int indent, const char* event_name, const char* handler_code) {
    if (!handler_code) return;

    // Check if handler has newlines (multi-line)
    bool is_multiline = strchr(handler_code, '\n') != NULL;

    // Generate indentation
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }

    if (is_multiline) {
        // Multi-line handler: output with proper indentation
        append_fmt(buffer, size, capacity, "%s = {\n", event_name);

        // Split handler code by lines and indent each
        const char* line_start = handler_code;
        while (*line_start) {
            const char* line_end = strchr(line_start, '\n');
            size_t line_len;

            if (line_end) {
                line_len = line_end - line_start;
                line_end++;  // Skip the newline
            } else {
                line_len = strlen(line_start);
                line_end = line_start + line_len;
            }

            // Skip leading whitespace from the line
            while (line_len > 0 && (line_start[0] == ' ' || line_start[0] == '\t')) {
                line_start++;
                line_len--;
            }

            // Generate indented line
            for (int i = 0; i < indent + 1; i++) {
                append_string(buffer, size, capacity, "  ");
            }
            append_fmt(buffer, size, capacity, "%.*s\n", (int)line_len, line_start);

            line_start = line_end;
        }

        // Close handler
        for (int i = 0; i < indent; i++) {
            append_string(buffer, size, capacity, "  ");
        }
        append_string(buffer, size, capacity, "}\n");
    } else {
        // Single-line handler
        append_fmt(buffer, size, capacity, "%s = { %s }\n", event_name, handler_code);
    }
}

// ============================================================================
// Part 3: If/Else Conditional Codegen
// ============================================================================

// Generate if/else conditional from Conditional node
// Generates: if <condition> { ... } else { ... }
static bool generate_conditional(cJSON* cond_node, cJSON* kir_root, char** buffer,
                                size_t* size, size_t* capacity, int indent,
                                EventHandlerContext* handler_ctx, SourceStructuresContext* src_ctx) {
    if (!cond_node) return false;

    // Get condition expression
    cJSON* condition = cJSON_GetObjectItem(cond_node, "condition");
    if (!condition) {
        // Try alternate field names
        condition = cJSON_GetObjectItem(cond_node, "cond");
        if (!condition) {
            condition = cJSON_GetObjectItem(cond_node, "test");
        }
    }

    // Get true and false branches
    cJSON* true_branch = cJSON_GetObjectItem(cond_node, "true_branch");
    cJSON* false_branch = cJSON_GetObjectItem(cond_node, "false_branch");
    if (!true_branch) {
        true_branch = cJSON_GetObjectItem(cond_node, "then");
    }
    if (!false_branch) {
        false_branch = cJSON_GetObjectItem(cond_node, "else");
    }

    // Get condition string
    const char* cond_str = NULL;
    if (condition && cJSON_IsString(condition)) {
        cond_str = condition->valuestring;
    } else if (condition && cJSON_IsObject(condition)) {
        // Condition might be an expression object
        cJSON* expr = cJSON_GetObjectItem(condition, "expression");
        if (expr && cJSON_IsString(expr)) {
            cond_str = expr->valuestring;
        }
    }

    if (!cond_str) {
        cond_str = "true";  // Default to true if no condition
    }

    // Generate: if <condition> {
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }
    append_fmt(buffer, size, capacity, "if %s {\n", cond_str);

    // Generate true branch
    if (true_branch) {
        if (cJSON_IsArray(true_branch)) {
            // Array of components
            cJSON* child = NULL;
            cJSON_ArrayForEach(child, true_branch) {
                generate_kry_component(child, handler_ctx, buffer, size, capacity,
                                      indent + 1, kir_root, src_ctx);
            }
        } else if (cJSON_IsObject(true_branch)) {
            generate_kry_component(true_branch, handler_ctx, buffer, size, capacity,
                                  indent + 1, kir_root, src_ctx);
        }
    }

    // Generate else branch if present
    if (false_branch) {
        // Close if branch
        for (int i = 0; i < indent; i++) {
            append_string(buffer, size, capacity, "  ");
        }
        append_string(buffer, size, capacity, "} else {\n");

        if (cJSON_IsArray(false_branch)) {
            // Array of components
            cJSON* child = NULL;
            cJSON_ArrayForEach(child, false_branch) {
                generate_kry_component(child, handler_ctx, buffer, size, capacity,
                                      indent + 1, kir_root, src_ctx);
            }
        } else if (cJSON_IsObject(false_branch)) {
            generate_kry_component(false_branch, handler_ctx, buffer, size, capacity,
                                  indent + 1, kir_root, src_ctx);
        }
    }

    // Close
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }
    append_string(buffer, size, capacity, "}\n");

    return true;
}

// Parse logic_block and build event handler lookup table
static void parse_logic_block(cJSON* kir_root, EventHandlerContext* ctx) {
    if (!kir_root || !ctx) return;

    cJSON* logic_block = cJSON_GetObjectItem(kir_root, "logic_block");
    if (!logic_block) return;

    // Parse event_bindings to map component_id -> event_type -> handler_name
    cJSON* bindings = cJSON_GetObjectItem(logic_block, "event_bindings");
    if (!bindings || !cJSON_IsArray(bindings)) return;

    // Parse functions to map handler_name -> source code
    cJSON* functions = cJSON_GetObjectItem(logic_block, "functions");
    if (!functions || !cJSON_IsArray(functions)) return;

    // Build handler_name -> source map
    typedef struct {
        char* name;
        char* source;
    } FunctionMap;

    int func_count = cJSON_GetArraySize(functions);
    FunctionMap* func_map = malloc(sizeof(FunctionMap) * func_count);
    if (!func_map) return;

    int func_idx = 0;
    cJSON* func = NULL;
    cJSON_ArrayForEach(func, functions) {
        cJSON* name_item = cJSON_GetObjectItem(func, "name");
        cJSON* sources = cJSON_GetObjectItem(func, "sources");

        if (name_item && sources && cJSON_IsArray(sources)) {
            // Look for kry language first
            cJSON* kry_source = NULL;
            cJSON* fallback_source = NULL;

            cJSON* source_item = NULL;
            cJSON_ArrayForEach(source_item, sources) {
                cJSON* lang = cJSON_GetObjectItem(source_item, "language");
                cJSON* src = cJSON_GetObjectItem(source_item, "source");

                if (lang && src) {
                    if (strcmp(lang->valuestring, "kry") == 0) {
                        kry_source = src;
                        break;
                    } else if (!fallback_source) {
                        fallback_source = src;
                    }
                }
            }

            // Use kry source if available, otherwise translate from other languages
            if (kry_source) {
                func_map[func_idx].name = strdup(name_item->valuestring);
                func_map[func_idx].source = strdup(kry_source->valuestring);
                func_idx++;
            } else if (fallback_source) {
                // Translate from TypeScript/JavaScript to kry
                const char* src = fallback_source->valuestring;
                char translated[1024];

                // Simple translation: console.log(...) -> print(...)
                if (strstr(src, "console.log(") != NULL) {
                    const char* start = strstr(src, "console.log(");
                    const char* args_start = start + 12; // After "console.log("
                    const char* args_end = strchr(args_start, ')');

                    if (args_end) {
                        size_t args_len = args_end - args_start;
                        char args[512];
                        strncpy(args, args_start, args_len);
                        args[args_len] = '\0';

                        snprintf(translated, sizeof(translated), "print(%s)", args);
                        func_map[func_idx].name = strdup(name_item->valuestring);
                        func_map[func_idx].source = strdup(translated);
                        func_idx++;
                    }
                } else {
                    // Use as-is if no translation available
                    func_map[func_idx].name = strdup(name_item->valuestring);
                    func_map[func_idx].source = strdup(src);
                    func_idx++;
                }
            }
        }
    }

    // Now process bindings
    cJSON* binding = NULL;
    cJSON_ArrayForEach(binding, bindings) {
        cJSON* comp_id = cJSON_GetObjectItem(binding, "component_id");
        cJSON* event_type = cJSON_GetObjectItem(binding, "event_type");
        cJSON* handler_name = cJSON_GetObjectItem(binding, "handler_name");

        if (comp_id && event_type && handler_name) {
            // Find source for this handler
            for (int i = 0; i < func_idx; i++) {
                if (strcmp(func_map[i].name, handler_name->valuestring) == 0) {
                    add_event_handler(ctx, comp_id->valueint, event_type->valuestring, func_map[i].source);
                    break;
                }
            }
        }
    }

    // Cleanup function map
    for (int i = 0; i < func_idx; i++) {
        free(func_map[i].name);
        free(func_map[i].source);
    }
    free(func_map);
}

// Forward declaration for recursive formatting
static void format_kry_value(char* output, size_t size, cJSON* value);

// Format array as Kry literal
static void format_kry_array(char* output, size_t size, cJSON* array) {
    size_t pos = 0;
    pos += snprintf(output + pos, size - pos, "[");

    cJSON* item = NULL;
    int idx = 0;
    cJSON_ArrayForEach(item, array) {
        if (idx > 0) pos += snprintf(output + pos, size - pos, ", ");

        char item_str[1024];
        format_kry_value(item_str, sizeof(item_str), item);
        pos += snprintf(output + pos, size - pos, "%s", item_str);
        idx++;
    }

    pos += snprintf(output + pos, size - pos, "]");
}

// Format object as Kry literal
static void format_kry_object(char* output, size_t size, cJSON* obj) {
    size_t pos = 0;
    pos += snprintf(output + pos, size - pos, "{");

    cJSON* item = NULL;
    int idx = 0;
    cJSON_ArrayForEach(item, obj) {
        if (idx > 0) pos += snprintf(output + pos, size - pos, ", ");

        const char* key = item->string;
        char value_str[1024];
        format_kry_value(value_str, sizeof(value_str), item);
        pos += snprintf(output + pos, size - pos, "%s: %s", key, value_str);
        idx++;
    }

    pos += snprintf(output + pos, size - pos, "}");
}

// Format a JSON value as .kry literal
static void format_kry_value(char* output, size_t size, cJSON* value) {
    if (cJSON_IsString(value)) {
        snprintf(output, size, "\"%s\"", value->valuestring);
    } else if (cJSON_IsNumber(value)) {
        // Check if it's a pixel value string
        if (value->valuedouble == (int)value->valuedouble) {
            snprintf(output, size, "%d", (int)value->valuedouble);
        } else {
            snprintf(output, size, "%.2f", value->valuedouble);
        }
    } else if (cJSON_IsBool(value)) {
        snprintf(output, size, "%s", cJSON_IsTrue(value) ? "true" : "false");
    } else if (cJSON_IsNull(value)) {
        snprintf(output, size, "null");
    } else if (cJSON_IsArray(value)) {
        format_kry_array(output, size, value);
    } else if (cJSON_IsObject(value)) {
        format_kry_object(output, size, value);
    } else {
        snprintf(output, size, "null");  // Fallback
    }
}

// Convert property name from KIR format to .kry format
static const char* kir_to_kry_property(const char* kir_name) {
    // KIR already uses Kry property names, no mapping needed
    // Previously had incorrect mappings like background->backgroundColor
    return kir_name;
}

// ============================================================================
// Part 7: Formatting Improvements
// ============================================================================

// Property ordering categories for better code generation
typedef enum {
    PROP_CATEGORY_LAYOUT,
    PROP_CATEGORY_SPACING,
    PROP_CATEGORY_STYLE,
    PROP_CATEGORY_CONTENT,
    PROP_CATEGORY_BEHAVIOR,
    PROP_CATEGORY_MISC
} PropertyCategory;

// Get the category for a property name
static PropertyCategory get_property_category(const char* prop_name) {
    if (!prop_name) return PROP_CATEGORY_MISC;

    // Layout properties
    if (strcmp(prop_name, "width") == 0 || strcmp(prop_name, "height") == 0 ||
        strcmp(prop_name, "flexDirection") == 0 || strcmp(prop_name, "direction") == 0 ||
        strcmp(prop_name, "justifyContent") == 0 || strcmp(prop_name, "alignItems") == 0 ||
        strcmp(prop_name, "alignContent") == 0 || strcmp(prop_name, "flexShrink") == 0 ||
        strcmp(prop_name, "flexGrow") == 0 || strcmp(prop_name, "flex") == 0 ||
        strcmp(prop_name, "display") == 0 || strcmp(prop_name, "position") == 0 ||
        strcmp(prop_name, "overflow") == 0 || strcmp(prop_name, "wrap") == 0) {
        return PROP_CATEGORY_LAYOUT;
    }

    // Spacing properties
    if (strcmp(prop_name, "padding") == 0 || strcmp(prop_name, "margin") == 0 ||
        strcmp(prop_name, "gap") == 0 || strcmp(prop_name, "rowGap") == 0 ||
        strcmp(prop_name, "columnGap") == 0) {
        return PROP_CATEGORY_SPACING;
    }

    // Style properties
    if (strcmp(prop_name, "background") == 0 || strcmp(prop_name, "backgroundColor") == 0 ||
        strcmp(prop_name, "color") == 0 || strcmp(prop_name, "border") == 0 ||
        strcmp(prop_name, "borderColor") == 0 || strcmp(prop_name, "borderWidth") == 0 ||
        strcmp(prop_name, "borderRadius") == 0 || strcmp(prop_name, "fontSize") == 0 ||
        strcmp(prop_name, "fontFamily") == 0 || strcmp(prop_name, "fontWeight") == 0 ||
        strcmp(prop_name, "lineHeight") == 0 || strcmp(prop_name, "opacity") == 0 ||
        strcmp(prop_name, "shadow") == 0 || strcmp(prop_name, "shadowColor") == 0 ||
        strcmp(prop_name, "shadowOffset") == 0 || strcmp(prop_name, "shadowRadius") == 0 ||
        strcmp(prop_name, "shadowOpacity") == 0) {
        return PROP_CATEGORY_STYLE;
    }

    // Content properties
    if (strcmp(prop_name, "text") == 0 || strcmp(prop_name, "src") == 0 ||
        strcmp(prop_name, "source") == 0 || strcmp(prop_name, "html") == 0 ||
        strcmp(prop_name, "content") == 0 || strcmp(prop_name, "alt") == 0 ||
        strcmp(prop_name, "href") == 0 || strcmp(prop_name, "url") == 0) {
        return PROP_CATEGORY_CONTENT;
    }

    // Behavior properties
    if (strcmp(prop_name, "disabled") == 0 || strcmp(prop_name, "readonly") == 0 ||
        strcmp(prop_name, "editable") == 0 || strcmp(prop_name, "visible") == 0 ||
        strcmp(prop_name, "hidden") == 0 || strcmp(prop_name, "checked") == 0 ||
        strcmp(prop_name, "selected") == 0 || strcmp(prop_name, "placeholder") == 0) {
        return PROP_CATEGORY_BEHAVIOR;
    }

    return PROP_CATEGORY_MISC;
}

// Compare two properties for ordering
// Returns < 0 if a should come before b, > 0 if b should come before a
static int compare_properties(cJSON* a, cJSON* b) {
    const char* key_a = a->string;
    const char* key_b = b->string;

    // Skip special keys (they should be filtered before calling this)
    if (strcmp(key_a, "type") == 0) return -1;
    if (strcmp(key_b, "type") == 0) return 1;

    PropertyCategory cat_a = get_property_category(key_a);
    PropertyCategory cat_b = get_property_category(key_b);

    if (cat_a != cat_b) {
        return (int)cat_a - (int)cat_b;
    }

    // Within same category, sort alphabetically
    return strcmp(key_a, key_b);
}

// Check if a property value should be skipped (default/unimportant values)
static bool should_skip_property(const char* key, cJSON* value) {
    if (!key || !value) return false;

    // Skip transparent colors (default background)
    if (cJSON_IsString(value)) {
        const char* val_str = value->valuestring;
        if (strcmp(val_str, "#00000000") == 0 || strcmp(val_str, "transparent") == 0) {
            if (strcmp(key, "background") == 0 || strcmp(key, "backgroundColor") == 0 ||
                strcmp(key, "color") == 0 || strcmp(key, "borderColor") == 0) {
                return true;
            }
        }

        // Skip default values
        if (strcmp(val_str, "auto") == 0 && strcmp(key, "direction") == 0) {
            return true;
        }
    }

    // Skip default numeric values
    if (cJSON_IsNumber(value)) {
        if (value->valuedouble == 1.0 &&
            (strcmp(key, "flexGrow") == 0 || strcmp(key, "flexShrink") == 0)) {
            return true;
        }
        if (value->valuedouble == 0.0 &&
            (strcmp(key, "padding") == 0 || strcmp(key, "margin") == 0 ||
             strcmp(key, "gap") == 0 || strcmp(key, "borderWidth") == 0)) {
            return true;
        }
    }

    return false;
}

// Check if a string looks like an identifier (for template variables)
static bool is_identifier_like(const char* str) {
    if (!str || str[0] == '\0') return false;

    // Must start with lowercase letter or underscore
    if (!((str[0] >= 'a' && str[0] <= 'z') || str[0] == '_')) {
        return false;
    }

    // Rest must be alphanumeric or underscore, and no special characters
    for (size_t i = 1; str[i] != '\0'; i++) {
        char c = str[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '.')) {  // Allow dots for property access like item.value
            return false;
        }
    }

    return true;
}

// Parse pixel value from string like "800.0px" or "600.0px"
static bool parse_pixel_value(const char* str, int* out_value) {
    if (!str) return false;

    // Try to parse as "123.0px" or "123px"
    float val;
    if (sscanf(str, "%fpx", &val) == 1) {
        *out_value = (int)val;
        return true;
    }

    return false;
}

// Generate .kry component code recursively
static bool generate_kry_component(cJSON* node, EventHandlerContext* handler_ctx,
                                    char** buffer, size_t* size, size_t* capacity, int indent,
                                    cJSON* kir_root, SourceStructuresContext* src_ctx) {
    if (!node || !cJSON_IsObject(node)) return true;

    // Get component type
    cJSON* type_item = cJSON_GetObjectItem(node, "type");
    if (!type_item) return true;
    const char* type = type_item->valuestring;

    // Handle ForEach components (for each loops)
    if (strcmp(type, "ForEach") == 0) {
        return generate_for_each_loop(node, kir_root, buffer, size, capacity, indent,
                                     handler_ctx, src_ctx);
    }

    // Handle Conditional components (if/else)
    if (strcmp(type, "Conditional") == 0) {
        return generate_conditional(node, kir_root, buffer, size, capacity, indent,
                                   handler_ctx, src_ctx);
    }

    // Get component ID for event handler lookup
    cJSON* id_item = cJSON_GetObjectItem(node, "id");
    int component_id = id_item ? id_item->valueint : -1;

    // Check if this component has a scope (indicates it's an instance of a custom component)
    // When custom components are expanded, the scope field contains "ComponentName#instance"
    // We need to extract the original component name to preserve custom component syntax
    cJSON* scope_item = cJSON_GetObjectItem(node, "scope");
    const char* original_component_name = type;  // Default to the actual type
    bool has_custom_scope = false;

    if (scope_item && scope_item->valuestring) {
        const char* scope = scope_item->valuestring;
        // Check if scope matches pattern "ComponentName#instance_number"
        const char* hash_pos = strchr(scope, '#');
        if (hash_pos && hash_pos > scope) {
            // Extract component name from scope (before the #)
            size_t name_len = hash_pos - scope;
            char* extracted_name = malloc(name_len + 1);
            if (extracted_name) {
                strncpy(extracted_name, scope, name_len);
                extracted_name[name_len] = '\0';

                // Check if this is NOT a built-in component type
                static const char* built_in_types[] = {
                    "Container", "Row", "Column", "Text", "Button", "Input", "CheckBox",
                    "Dropdown", "Modal", "TabPanel", "TabGroup", "TabBar", "TabContent",
                    "Image", "Canvas", "Table", "ScrollView", "ForEach", "Conditional",
                    NULL
                };

                bool is_built_in = false;
                for (int i = 0; built_in_types[i]; i++) {
                    if (strcmp(extracted_name, built_in_types[i]) == 0) {
                        is_built_in = true;
                        break;
                    }
                }

                // If not built-in, use this as the original component name
                if (!is_built_in) {
                    original_component_name = extracted_name;
                    has_custom_scope = true;
                } else {
                    free(extracted_name);
                }
            }
        }
    }

    // Determine if this is a custom or built-in component
    bool is_custom = has_custom_scope;  // Custom if we found a non-built-in scope

    // Generate indentation
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }

    // Write component type or custom component instantiation
    if (is_custom) {
        // Custom component: use function call syntax with original component name
        // TODO: Add argument support in future
        append_fmt(buffer, size, capacity, "%s()\n", original_component_name);
    } else {
        // Built-in component: use normal syntax
        append_fmt(buffer, size, capacity, "%s {\n", type);
    }

    // Iterate through all properties (KIR has flattened props)
    cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, node) {
        const char* key = prop->string;

        // Skip special keys
        if (strcmp(key, "type") == 0 ||
            strcmp(key, "id") == 0 ||
            strcmp(key, "scope") == 0 ||  // Skip internal scope metadata
            strcmp(key, "children") == 0 ||
            strcmp(key, "events") == 0 ||
            strcmp(key, "property_bindings") == 0 ||
            strcmp(key, "for_def") == 0 ||
            strcmp(key, "foreach_def") == 0 ||  // Legacy check
            strcmp(key, "actual_type") == 0 ||
            strcmp(key, "custom_data") == 0) {  // Skip metadata
            continue;
        }

        // Skip properties with default/unimportant values
        if (should_skip_property(key, prop)) {
            continue;
        }

        // Convert property name
        const char* kry_key = kir_to_kry_property(key);

        // Write property indentation
        for (int i = 0; i < indent + 1; i++) {
            append_string(buffer, size, capacity, "  ");
        }

        // Check if this property has a binding (for unresolved template expressions)
        cJSON* bindings_obj = cJSON_GetObjectItem(node, "property_bindings");
        cJSON* binding = bindings_obj ? cJSON_GetObjectItem(bindings_obj, key) : NULL;

        if (binding) {
            // Use source_expr from binding (preserves original expression like "item.value")
            cJSON* source_expr = cJSON_GetObjectItem(binding, "source_expr");
            if (source_expr && source_expr->valuestring) {
                append_fmt(buffer, size, capacity, "%s = %s\n", kry_key, source_expr->valuestring);
                continue;  // Skip normal property handling
            }
        }

        // Normal property handling for resolved values
        if (cJSON_IsString(prop)) {
            int pixel_val;
            if (parse_pixel_value(prop->valuestring, &pixel_val)) {
                // It's a pixel value like "800.0px", output as number
                append_fmt(buffer, size, capacity, "%s = %d\n", kry_key, pixel_val);
            } else if (is_identifier_like(prop->valuestring)) {
                // Looks like an identifier (e.g., "item" or "item.value"), output without quotes
                append_fmt(buffer, size, capacity, "%s = %s\n", kry_key, prop->valuestring);
            } else {
                // Regular string literal
                append_fmt(buffer, size, capacity, "%s = \"%s\"\n", kry_key, prop->valuestring);
            }
        } else {
            char value_str[512];
            format_kry_value(value_str, sizeof(value_str), prop);
            append_fmt(buffer, size, capacity, "%s = %s\n", kry_key, value_str);
        }
    }

    // Add event handlers from logic_block (using improved formatting)
    if (component_id >= 0) {
        const char* click_handler = find_event_handler(handler_ctx, component_id, "click");
        if (click_handler) {
            generate_event_handler(buffer, size, capacity, indent + 1, "onClick", click_handler);
        }

        const char* hover_handler = find_event_handler(handler_ctx, component_id, "hover");
        if (hover_handler) {
            generate_event_handler(buffer, size, capacity, indent + 1, "onHover", hover_handler);
        }

        const char* change_handler = find_event_handler(handler_ctx, component_id, "change");
        if (change_handler) {
            generate_event_handler(buffer, size, capacity, indent + 1, "onChange", change_handler);
        }

        const char* submit_handler = find_event_handler(handler_ctx, component_id, "submit");
        if (submit_handler) {
            generate_event_handler(buffer, size, capacity, indent + 1, "onSubmit", submit_handler);
        }

        const char* input_handler = find_event_handler(handler_ctx, component_id, "input");
        if (input_handler) {
            generate_event_handler(buffer, size, capacity, indent + 1, "onInput", input_handler);
        }
    }

    // Process children - check for static blocks first
    cJSON* children = cJSON_GetObjectItem(node, "children");
    if (children && cJSON_IsArray(children)) {
        // Add blank line before children for readability
        append_string(buffer, size, capacity, "\n");

        // Check if this component has a static block (from source_structures)
        bool has_static_block = false;
        cJSON* static_block = NULL;
        if (src_ctx && src_ctx->static_blocks && component_id >= 0) {
            static_block = find_static_block_by_parent(src_ctx, (uint32_t)component_id);
            has_static_block = (static_block != NULL);
        }

        if (has_static_block && static_block) {
            // Generate static block with its const declarations and for loops
            cJSON* id_obj = cJSON_GetObjectItem(static_block, "id");
            if (id_obj && id_obj->valuestring) {
                // Generate the static block
                for (int i = 0; i < indent + 1; i++) {
                    append_string(buffer, size, capacity, "  ");
                }
                append_string(buffer, size, capacity, "static {\n");

                // Generate const declarations scoped to this static block
                cJSON* const_decls = find_const_declarations_for_scope(src_ctx, id_obj->valuestring);
                if (const_decls) {
                    generate_const_declarations(const_decls, buffer, size, capacity, indent + 2);
                    append_string(buffer, size, capacity, "\n");
                }

                // Generate for loops in this static block
                cJSON* for_loops = find_for_loops_by_parent(src_ctx, id_obj->valuestring);
                if (for_loops && cJSON_IsArray(for_loops)) {
                    cJSON* loop = NULL;
                    cJSON_ArrayForEach(loop, for_loops) {
                        generate_for_loop(loop, kir_root, buffer, size, capacity, indent + 2, handler_ctx, src_ctx);
                    }
                }

                // Closing brace
                for (int i = 0; i < indent + 1; i++) {
                    append_string(buffer, size, capacity, "  ");
                }
                append_string(buffer, size, capacity, "}\n");
            }
        } else {
            // Regular children (no static block)
            cJSON* child = NULL;
            cJSON_ArrayForEach(child, children) {
                if (!generate_kry_component(child, handler_ctx, buffer, size, capacity,
                                           indent + 1, kir_root, src_ctx)) {
                    return false;
                }
            }
        }
    }

    // Close component
    // For custom components (using function call syntax), no closing brace needed
    // For built-in components, close with }
    if (!is_custom) {
        for (int i = 0; i < indent; i++) {
            append_string(buffer, size, capacity, "  ");
        }
        append_string(buffer, size, capacity, "}\n");
    }

    return true;
}

// Generate .kry code from KIR JSON
char* kry_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    // Parse KIR JSON
    cJSON* root_json = cJSON_Parse(kir_json);
    if (!root_json) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return NULL;
    }

    // Create event handler context and parse logic_block
    EventHandlerContext* handler_ctx = create_handler_context();
    if (!handler_ctx) {
        cJSON_Delete(root_json);
        return NULL;
    }

    parse_logic_block(root_json, handler_ctx);

    // Create source structures context for round-trip support
    SourceStructuresContext* src_ctx = create_source_context(root_json);
    if (!src_ctx) {
        destroy_handler_context(handler_ctx);
        cJSON_Delete(root_json);
        return NULL;
    }

    // Allocate output buffer
    size_t capacity = 8192;
    size_t size = 0;
    char* buffer = malloc(capacity);
    if (!buffer) {
        destroy_handler_context(handler_ctx);
        cJSON_Delete(root_json);
        return NULL;
    }
    buffer[0] = '\0';

    // Get reactive_manifest for state declarations
    cJSON* reactive_manifest = cJSON_GetObjectItem(root_json, "reactive_manifest");

    // Get root component
    cJSON* root_comp = cJSON_GetObjectItem(root_json, "root");
    // Note: cJSON_GetObjectItem returns non-NULL even for JSON null values,
    // so we need to check if it's actually an object
    if (root_comp && cJSON_IsObject(root_comp)) {
        // KIR format with root component

        // Generate state declarations before the root component
        generate_state_declarations(reactive_manifest, "state", &buffer, &size, &capacity, 0);

        // Add blank line between state declarations and component
        if (reactive_manifest) {
            append_string(&buffer, &size, &capacity, "\n");
        }

        if (!generate_kry_component(root_comp, handler_ctx, &buffer, &size, &capacity, 0,
                                   root_json, src_ctx)) {
            free(buffer);
            destroy_source_context(src_ctx);
            destroy_handler_context(handler_ctx);
            cJSON_Delete(root_json);
            return NULL;
        }
    } else {
        // Check for component definitions (library modules)
        cJSON* component_defs = cJSON_GetObjectItem(root_json, "component_definitions");
        if (component_defs && cJSON_IsArray(component_defs) && cJSON_GetArraySize(component_defs) > 0) {
            // Library module with component definitions
            cJSON* comp_def = NULL;
            int def_count = cJSON_GetArraySize(component_defs);
            int i = 0;
            cJSON_ArrayForEach(comp_def, component_defs) {
                cJSON* name = cJSON_GetObjectItem(comp_def, "name");
                cJSON* template_field = cJSON_GetObjectItem(comp_def, "template");
                cJSON* source_field = cJSON_GetObjectItem(comp_def, "source");

                if (name && cJSON_IsString(name)) {
                    // Check for template field (UI component definitions from KRY/DSL)
                    if (template_field) {
                        cJSON* template_comp = NULL;
                        bool should_free_template = false;

                        // Template can be either a JSON string (from Lua parser) or a JSON object (from KRY parser)
                        if (cJSON_IsString(template_field)) {
                            // Parse the template JSON string to get the component tree
                            const char* tmpl_json_str = cJSON_GetStringValue(template_field);
                            template_comp = cJSON_Parse(tmpl_json_str);
                            should_free_template = true;
                        } else if (cJSON_IsObject(template_field)) {
                            // Template is already a JSON object
                            template_comp = template_field;
                            should_free_template = false;
                        }

                        if (template_comp && cJSON_IsObject(template_comp)) {
                            // ============================================================================
                            // Part 4: Component Parameter Codegen
                            // ============================================================================

                            // Generate: component <name>(<params>) {
                            const char* comp_name = cJSON_GetStringValue(name);
                            append_fmt(&buffer, &size, &capacity, "component %s", comp_name);

                            // Check for props/parameters
                            cJSON* props = cJSON_GetObjectItem(comp_def, "props");
                            if (props && cJSON_IsArray(props) && cJSON_GetArraySize(props) > 0) {
                                append_string(&buffer, &size, &capacity, "(");

                                cJSON* prop = NULL;
                                int prop_idx = 0;
                                cJSON_ArrayForEach(prop, props) {
                                    cJSON* prop_name = cJSON_GetObjectItem(prop, "name");
                                    cJSON* prop_type = cJSON_GetObjectItem(prop, "type");
                                    cJSON* prop_default = cJSON_GetObjectItem(prop, "default");

                                    if (prop_name && prop_name->valuestring) {
                                        // Generate: name: type
                                        append_fmt(&buffer, &size, &capacity, "%s: %s",
                                                  prop_name->valuestring,
                                                  prop_type && prop_type->valuestring ? prop_type->valuestring : "any");

                                        // Add default value if present
                                        if (prop_default) {
                                            const char* default_val = prop_default->valuestring;
                                            if (default_val) {
                                                if (prop_default->type == cJSON_String) {
                                                    append_fmt(&buffer, &size, &capacity, " = \"%s\"", default_val);
                                                } else if (prop_default->type == cJSON_Number) {
                                                    append_fmt(&buffer, &size, &capacity, " = %d", prop_default->valueint);
                                                } else if (prop_default->type == cJSON_True) {
                                                    append_string(&buffer, &size, &capacity, " = true");
                                                } else if (prop_default->type == cJSON_False) {
                                                    append_string(&buffer, &size, &capacity, " = false");
                                                } else {
                                                    append_fmt(&buffer, &size, &capacity, " = %s", default_val);
                                                }
                                            }
                                        }

                                        if (prop_idx < cJSON_GetArraySize(props) - 1) {
                                            append_string(&buffer, &size, &capacity, ", ");
                                        }
                                        prop_idx++;
                                    }
                                }
                                append_string(&buffer, &size, &capacity, ")");
                            }

                            append_string(&buffer, &size, &capacity, " {\n");

                            // Generate component body from parsed template
                            generate_kry_component(template_comp, handler_ctx, &buffer, &size, &capacity,
                                                  1, root_json, src_ctx);

                            // Close component
                            append_string(&buffer, &size, &capacity, "}\n");

                            if (should_free_template) {
                                cJSON_Delete(template_comp);
                            }
                        }
                    } else if (source_field && cJSON_IsString(source_field)) {
                        // No template, but has source field (Lua helper functions)
                        // Output the Lua source code
                        const char* func_source = cJSON_GetStringValue(source_field);
                        append_string(&buffer, &size, &capacity, func_source);
                        append_string(&buffer, &size, &capacity, "\n");
                    }

                    if (i < def_count - 1) {
                        append_string(&buffer, &size, &capacity, "\n\n");
                    }
                }
                i++;
            }

            destroy_source_context(src_ctx);
            destroy_handler_context(handler_ctx);
            cJSON_Delete(root_json);
            return buffer;  // Caller takes ownership and frees the buffer
        }
    }

    destroy_source_context(src_ctx);
    destroy_handler_context(handler_ctx);
    cJSON_Delete(root_json);
    return buffer;
}

// Generate .kry code from KIR file
bool kry_codegen_generate(const char* kir_path, const char* output_path) {
    // Set error prefix for this codegen
    codegen_set_error_prefix("Kry");

    // Read KIR file using shared utility
    char* kir_json = codegen_read_kir_file(kir_path, NULL);
    if (!kir_json) {
        return false;
    }

    // Generate .kry code
    char* kry_code = kry_codegen_from_json(kir_json);
    free(kir_json);

    if (!kry_code) {
        return false;
    }

    // Write output file using shared utility
    bool success = codegen_write_output_file(output_path, kry_code);
    free(kry_code);

    return success;
}

// Generate .kry code with options
bool kry_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        KryCodegenOptions* options) {
    (void)options;
    // Generate base .kry code
    return kry_codegen_generate(kir_path, output_path);
}

/**
 * Recursively process a module and its transitive imports
 * (Uses shared utilities from codegen_common)
 */
static int process_module_recursive_kry(const char* module_id, const char* kir_dir,
                                        const char* output_dir, CodegenProcessedModules* processed) {
    // Skip if already processed
    if (codegen_processed_modules_contains(processed, module_id)) return 0;
    codegen_processed_modules_add(processed, module_id);

    // Skip internal modules and external plugins
    if (codegen_is_internal_module(module_id)) return 0;
    if (codegen_is_external_plugin(module_id)) return 0;

    // Build path to component's KIR file
    char component_kir_path[2048];
    snprintf(component_kir_path, sizeof(component_kir_path),
             "%s/%s.kir", kir_dir, module_id);

    // Read component's KIR
    char* component_kir_json = codegen_read_kir_file(component_kir_path, NULL);
    if (!component_kir_json) {
        fprintf(stderr, "Warning: Cannot find KIR for '%s' at %s\n",
                module_id, component_kir_path);
        return 0;
    }

    // Parse to get transitive imports before generating
    cJSON* component_root = cJSON_Parse(component_kir_json);
    int files_written = 0;

    // Generate KRY from KIR
    char* component_kry = kry_codegen_from_json(component_kir_json);
    free(component_kir_json);

    if (component_kry) {
        // Write to output directory maintaining hierarchy
        char output_path[2048];
        snprintf(output_path, sizeof(output_path),
                 "%s/%s.kry", output_dir, module_id);

        if (codegen_write_file_with_mkdir(output_path, component_kry)) {
            printf("✓ Generated: %s.kry\n", module_id);
            files_written++;
        }
        free(component_kry);
    } else {
        fprintf(stderr, "Warning: Failed to generate KRY for '%s'\n", module_id);
    }

    // Process transitive imports from this component
    if (component_root) {
        cJSON* imports = cJSON_GetObjectItem(component_root, "imports");
        if (imports && cJSON_IsArray(imports)) {
            cJSON* import_item = NULL;
            cJSON_ArrayForEach(import_item, imports) {
                if (!cJSON_IsString(import_item)) continue;
                const char* sub_module_id = cJSON_GetStringValue(import_item);
                if (sub_module_id) {
                    files_written += process_module_recursive_kry(sub_module_id, kir_dir,
                                                                  output_dir, processed);
                }
            }
        }
        cJSON_Delete(component_root);
    }

    return files_written;
}

/**
 * Generate multi-file .kry code from multi-file KIR by reading linked KIR files
 *
 * This function reads the main.kir file, generates .kry code from its KIR representation,
 * then follows the imports array to find and process linked component KIR files.
 * Transitive imports are processed recursively.
 * Each KIR file is transformed to .kry using kry_codegen_from_json().
 */
bool kry_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: Invalid arguments to kry_codegen_generate_multi\n");
        return false;
    }

    // Set error prefix for this codegen
    codegen_set_error_prefix("Kry");

    // Read main KIR file
    char* main_kir_json = codegen_read_kir_file(kir_path, NULL);
    if (!main_kir_json) {
        return false;
    }

    // Parse main KIR JSON
    cJSON* main_root = cJSON_Parse(main_kir_json);
    if (!main_root) {
        fprintf(stderr, "Error: Failed to parse main KIR JSON\n");
        free(main_kir_json);
        return false;
    }

    // Create output directory if it doesn't exist
    if (!codegen_mkdir_p(output_dir)) {
        fprintf(stderr, "Error: Could not create output directory: %s\n", output_dir);
        cJSON_Delete(main_root);
        free(main_kir_json);
        return false;
    }

    int files_written = 0;

    // 1. Generate main.kry from main.kir
    char* main_kry = kry_codegen_from_json(main_kir_json);
    free(main_kir_json);

    if (main_kry) {
        char main_output_path[2048];
        snprintf(main_output_path, sizeof(main_output_path), "%s/main.kry", output_dir);

        if (codegen_write_file_with_mkdir(main_output_path, main_kry)) {
            printf("✓ Generated: main.kry\n");
            files_written++;
        }
        free(main_kry);
    } else {
        fprintf(stderr, "Warning: Failed to generate main.kry from KIR\n");
    }

    // 2. Get the KIR directory (parent of kir_path)
    char kir_dir[2048];
    codegen_get_parent_dir(kir_path, kir_dir, sizeof(kir_dir));

    // 3. Track processed modules to avoid duplicates
    CodegenProcessedModules processed = {0};
    codegen_processed_modules_add(&processed, "main");  // Mark main as processed

    // 4. Process each import recursively (including transitive imports)
    cJSON* imports = cJSON_GetObjectItem(main_root, "imports");
    if (imports && cJSON_IsArray(imports)) {
        cJSON* import_item = NULL;
        cJSON_ArrayForEach(import_item, imports) {
            if (!cJSON_IsString(import_item)) continue;

            const char* module_id = cJSON_GetStringValue(import_item);
            if (module_id) {
                files_written += process_module_recursive_kry(module_id, kir_dir,
                                                              output_dir, &processed);
            }
        }
    }

    codegen_processed_modules_free(&processed);
    cJSON_Delete(main_root);

    if (files_written == 0) {
        fprintf(stderr, "Warning: No .kry files were generated\n");
        return false;
    }

    printf("✓ Generated %d .kry files in %s\n", files_written, output_dir);
    return true;
}
