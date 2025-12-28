/**
 * Kry Code Generator
 * Generates .kry code from KIR JSON (with logic_block support)
 */

#define _POSIX_C_SOURCE 200809L

#include "kry_codegen.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

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
            // TODO: For now, use the JSON string directly (placeholder)
            // In a full implementation, parse the JSON and format it as Kry
            append_fmt(buffer, size, capacity, "%s", value_json_str->valuestring);
        } else {
            append_string(buffer, size, capacity, "null");
        }

        append_string(buffer, size, capacity, "\n");
    }

    return true;
}

// Generate for loop from template
static bool generate_for_loop(cJSON* loop_template, cJSON* kir_root, char** buffer,
                              size_t* size, size_t* capacity, int indent,
                              EventHandlerContext* event_ctx);  // Forward decl

// Generate static block with contents
static bool generate_static_block(SourceStructuresContext* src_ctx, const char* static_block_id,
                                  cJSON* kir_root, char** buffer, size_t* size,
                                  size_t* capacity, int indent, EventHandlerContext* event_ctx) {
    if (!src_ctx || !static_block_id) return false;

    // Find static block metadata
    cJSON* static_block = find_static_block_by_parent(src_ctx, 0);  // Will refine this
    if (!static_block) return false;

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
            generate_for_loop(loop, kir_root, buffer, size, capacity, indent + 1, event_ctx);
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
static bool generate_for_loop(cJSON* loop_template, cJSON* kir_root, char** buffer,
                              size_t* size, size_t* capacity, int indent,
                              EventHandlerContext* event_ctx) {
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

    // Get expanded component IDs to find the template
    cJSON* expanded_ids = cJSON_GetObjectItem(loop_template, "expanded_component_ids");
    if (expanded_ids && cJSON_IsArray(expanded_ids) && cJSON_GetArraySize(expanded_ids) > 0) {
        // Get first expanded component as template
        cJSON* first_id = cJSON_GetArrayItem(expanded_ids, 0);
        if (first_id && cJSON_IsNumber(first_id)) {
            uint32_t template_id = (uint32_t)first_id->valueint;

            // Find component in KIR root tree
            cJSON* root_comp = cJSON_GetObjectItem(kir_root, "root");
            cJSON* template_comp = find_component_by_id(root_comp, template_id);

            if (template_comp) {
                // Generate component (this will be the loop body)
                generate_kry_component(template_comp, buffer, size, capacity, indent + 1,
                                      kir_root, event_ctx);
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

    // Get component ID for event handler lookup
    cJSON* id_item = cJSON_GetObjectItem(node, "id");
    int component_id = id_item ? id_item->valueint : -1;

    // Generate indentation
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }

    // Write component type
    append_fmt(buffer, size, capacity, "%s {\n", type);

    // Iterate through all properties (KIR has flattened props)
    cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, node) {
        const char* key = prop->string;

        // Skip special keys
        if (strcmp(key, "type") == 0 ||
            strcmp(key, "id") == 0 ||
            strcmp(key, "children") == 0 ||
            strcmp(key, "events") == 0) {  // Skip events, we use logic_block
            continue;
        }

        // Convert property name
        const char* kry_key = kir_to_kry_property(key);

        // Write property
        for (int i = 0; i < indent + 1; i++) {
            append_string(buffer, size, capacity, "  ");
        }

        // Handle special formatting for pixel values
        if (cJSON_IsString(prop)) {
            int pixel_val;
            if (parse_pixel_value(prop->valuestring, &pixel_val)) {
                // It's a pixel value like "800.0px", output as number
                append_fmt(buffer, size, capacity, "%s = %d\n", kry_key, pixel_val);
            } else if (strcmp(prop->valuestring, "#00000000") == 0) {
                // Skip transparent color (default)
                continue;
            } else {
                // Regular string
                append_fmt(buffer, size, capacity, "%s = \"%s\"\n", kry_key, prop->valuestring);
            }
        } else {
            char value_str[512];
            format_kry_value(value_str, sizeof(value_str), prop);
            append_fmt(buffer, size, capacity, "%s = %s\n", kry_key, value_str);
        }
    }

    // Add event handlers from logic_block
    if (component_id >= 0) {
        const char* click_handler = find_event_handler(handler_ctx, component_id, "click");
        if (click_handler) {
            for (int i = 0; i < indent + 1; i++) {
                append_string(buffer, size, capacity, "  ");
            }
            append_fmt(buffer, size, capacity, "onClick = {%s\n", click_handler);
        }

        const char* hover_handler = find_event_handler(handler_ctx, component_id, "hover");
        if (hover_handler) {
            for (int i = 0; i < indent + 1; i++) {
                append_string(buffer, size, capacity, "  ");
            }
            append_fmt(buffer, size, capacity, "onHover = {%s\n", hover_handler);
        }

        const char* change_handler = find_event_handler(handler_ctx, component_id, "change");
        if (change_handler) {
            for (int i = 0; i < indent + 1; i++) {
                append_string(buffer, size, capacity, "  ");
            }
            append_fmt(buffer, size, capacity, "onChange = {%s\n", change_handler);
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
                generate_static_block(src_ctx, id_obj->valuestring, kir_root,
                                     buffer, size, capacity, indent + 1, handler_ctx);
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
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }
    append_string(buffer, size, capacity, "}\n");

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

    // Get root component
    cJSON* root_comp = cJSON_GetObjectItem(root_json, "root");
    if (root_comp) {
        // KIR format with root component
        if (!generate_kry_component(root_comp, handler_ctx, &buffer, &size, &capacity, 0)) {
            free(buffer);
            destroy_handler_context(handler_ctx);
            cJSON_Delete(root_json);
            return NULL;
        }
    } else {
        // Legacy format or direct component
        if (!generate_kry_component(root_json, handler_ctx, &buffer, &size, &capacity, 0)) {
            free(buffer);
            destroy_handler_context(handler_ctx);
            cJSON_Delete(root_json);
            return NULL;
        }
    }

    destroy_handler_context(handler_ctx);
    cJSON_Delete(root_json);
    return buffer;
}

// Generate .kry code from KIR file
bool kry_codegen_generate(const char* kir_path, const char* output_path) {
    // Read KIR file
    FILE* kir_file = fopen(kir_path, "r");
    if (!kir_file) {
        fprintf(stderr, "Error: Could not open KIR file: %s\n", kir_path);
        return false;
    }

    fseek(kir_file, 0, SEEK_END);
    long file_size = ftell(kir_file);
    fseek(kir_file, 0, SEEK_SET);

    char* kir_json = malloc(file_size + 1);
    if (!kir_json) {
        fclose(kir_file);
        return false;
    }

    size_t bytes_read = fread(kir_json, 1, file_size, kir_file);
    kir_json[bytes_read] = '\0';
    fclose(kir_file);

    // Generate .kry code
    char* kry_code = kry_codegen_from_json(kir_json);
    free(kir_json);

    if (!kry_code) {
        return false;
    }

    // Write output file
    FILE* output_file = fopen(output_path, "w");
    if (!output_file) {
        fprintf(stderr, "Error: Could not write output file: %s\n", output_path);
        free(kry_code);
        return false;
    }

    fprintf(output_file, "%s", kry_code);
    fclose(output_file);
    free(kry_code);

    return true;
}

// Generate .kry code with options
bool kry_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        KryCodegenOptions* options) {
    // Generate base .kry code
    return kry_codegen_generate(kir_path, output_path);
}
