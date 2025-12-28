/**
 * Lua Code Generator
 * Generates Lua DSL from KIR JSON (with logic_block and reactive support)
 */

#define _POSIX_C_SOURCE 200809L

#include "lua_codegen.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/ir_logic.h"
#include "../../ir/third_party/cJSON/cJSON.h"
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
        if (*capacity < *size + len + 1) {
            *capacity = *size + len + 1024;
        }
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

// Escape string for Lua
static char* escape_lua_string(const char* str) {
    if (!str) return strdup("");
    
    size_t len = strlen(str);
    char* escaped = malloc(len * 2 + 1);
    char* dst = escaped;
    
    for (const char* src = str; *src; src++) {
        if (*src == '"') {
            *dst++ = '\\';
            *dst++ = '"';
        } else if (*src == '\\') {
            *dst++ = '\\';
            *dst++ = '\\';
        } else if (*src == '\n') {
            *dst++ = '\\';
            *dst++ = 'n';
        } else if (*src == '\r') {
            *dst++ = '\\';
            *dst++ = 'r';
        } else if (*src == '\t') {
            *dst++ = '\\';
            *dst++ = 't';
        } else {
            *dst++ = *src;
        }
    }
    *dst = '\0';
    
    return escaped;
}

// Capitalize first letter (for event handler names)
static char* capitalize(const char* str) {
    if (!str || !*str) return strdup("");
    
    char* result = strdup(str);
    result[0] = toupper(result[0]);
    return result;
}

// Context for Lua generation
typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
    IRLogicBlock* logic_block;
    int indent_level;
} LuaContext;

// Initialize Lua context
static LuaContext* create_lua_context(IRLogicBlock* logic_block) {
    LuaContext* ctx = malloc(sizeof(LuaContext));
    if (!ctx) return NULL;

    ctx->capacity = 8192;
    ctx->size = 0;
    ctx->buffer = malloc(ctx->capacity);
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }
    ctx->buffer[0] = '\0';
    ctx->logic_block = logic_block;
    ctx->indent_level = 0;

    return ctx;
}

// Free Lua context (doesn't free logic_block)
static void destroy_lua_context(LuaContext* ctx) {
    if (!ctx) return;
    free(ctx->buffer);
    free(ctx);
}

// Write indentation
static bool write_indent(LuaContext* ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "  ")) {
            return false;
        }
    }
    return true;
}

// Forward declarations
static bool generate_component(LuaContext* ctx, IRComponent* component);

// Generate reactive state declarations
static bool generate_reactive_state(LuaContext* ctx, cJSON* reactive_manifest) {
    if (!reactive_manifest) return true;

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "-- Reactive State\n");

    cJSON* states = cJSON_GetObjectItem(reactive_manifest, "states");
    if (states && cJSON_IsArray(states)) {
        int state_count = cJSON_GetArraySize(states);
        for (int i = 0; i < state_count; i++) {
            cJSON* state = cJSON_GetArrayItem(states, i);
            if (!state) continue;

            cJSON* name = cJSON_GetObjectItem(state, "name");
            cJSON* type = cJSON_GetObjectItem(state, "type");
            cJSON* initial = cJSON_GetObjectItem(state, "initial_value");

            if (!name || !name->valuestring) continue;

            const char* state_type = type ? type->valuestring : "state";

            if (strcmp(state_type, "array") == 0) {
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                          "local %s = Reactive.array({})\n", name->valuestring);
            } else if (strcmp(state_type, "computed") == 0) {
                // Skip computed for now, handle after dependencies
                continue;
            } else {
                const char* init_val = initial ? initial->valuestring : "nil";
                append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                          "local %s = Reactive.state(%s)\n", name->valuestring, init_val);
            }
        }
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");
    return true;
}

// Generate event handler functions
static bool generate_event_handlers(LuaContext* ctx) {
    if (!ctx->logic_block || ctx->logic_block->function_count == 0) {
        return true;
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "-- Event Handlers\n");

    for (int i = 0; i < ctx->logic_block->function_count; i++) {
        IRLogicFunction* func = &ctx->logic_block->functions[i];
        
        // Find JavaScript/Lua source
        const char* source = NULL;
        for (int j = 0; j < func->source_count; j++) {
            if (strcmp(func->sources[j].language, "lua") == 0 ||
                strcmp(func->sources[j].language, "javascript") == 0) {
                source = func->sources[j].source;
                break;
            }
        }

        if (!source && func->source_count > 0) {
            source = func->sources[0].source;
        }

        if (!source) continue;

        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                  "local function %s()\n  %s\nend\n\n",
                  func->name, source);
    }

    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");
    return true;
}

// Generate component properties
static bool generate_properties(LuaContext* ctx, IRComponent* component) {
    if (!component) return true;

    // Background color
    if (component->background) {
        write_indent(ctx);
        char* escaped = escape_lua_string(component->background);
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                  "backgroundColor = \"%s\",\n", escaped);
        free(escaped);
    }

    // Text content
    if (component->text_content) {
        write_indent(ctx);
        char* escaped = escape_lua_string(component->text_content);
        append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                  "text = \"%s\",\n", escaped);
        free(escaped);
    }

    // Width/height (simplified for now)
    // TODO: Full dimension support

    return true;
}

// Get event handler function name for component
static const char* get_handler_name(LuaContext* ctx, uint32_t component_id, const char* event_type) {
    if (!ctx->logic_block) return NULL;

    const char* handler_name = ir_logic_block_get_handler(ctx->logic_block, component_id, event_type);
    return handler_name;
}

// Generate event bindings for component
static bool generate_event_bindings(LuaContext* ctx, IRComponent* component) {
    if (!ctx->logic_block) return true;

    const char* events[] = {"click", "change", "submit", "input", NULL};
    
    for (int i = 0; events[i]; i++) {
        const char* handler_name = get_handler_name(ctx, component->id, events[i]);
        if (handler_name) {
            write_indent(ctx);
            char* event_name_cap = capitalize(events[i]);
            append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity,
                      "on%s = %s,\n", event_name_cap, handler_name);
            free(event_name_cap);
        }
    }

    return true;
}

// Generate component tree
static bool generate_component(LuaContext* ctx, IRComponent* component) {
    if (!component) return true;

    write_indent(ctx);

    // Component type
    const char* type_name = "Container";
    switch (component->type) {
        case IR_COMPONENT_COLUMN: type_name = "Column"; break;
        case IR_COMPONENT_ROW: type_name = "Row"; break;
        case IR_COMPONENT_TEXT: type_name = "Text"; break;
        case IR_COMPONENT_BUTTON: type_name = "Button"; break;
        case IR_COMPONENT_INPUT: type_name = "Input"; break;
        case IR_COMPONENT_HEADING: type_name = "Text"; break; // Map heading to Text
        default: type_name = "Container"; break;
    }

    append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, "UI.%s {\n", type_name);
    
    ctx->indent_level++;

    // Properties
    generate_properties(ctx, component);

    // Event bindings
    generate_event_bindings(ctx, component);

    // Children
    if (component->child_count > 0) {
        for (uint32_t i = 0; i < component->child_count; i++) {
            generate_component(ctx, component->children[i]);
            if (i < component->child_count - 1) {
                append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ",\n");
            } else {
                append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");
            }
        }
    }

    ctx->indent_level--;
    write_indent(ctx);
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "}");

    return true;
}

// Main codegen function
char* lua_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    // Parse KIR JSON
    cJSON* kir = cJSON_Parse(kir_json);
    if (!kir) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return NULL;
    }

    // Deserialize IR component tree
    extern IRComponent* ir_deserialize_json(const char* json);
    IRComponent* root = ir_deserialize_json(kir_json);
    if (!root) {
        fprintf(stderr, "Error: Failed to deserialize IR from JSON\n");
        cJSON_Delete(kir);
        return NULL;
    }

    // Extract logic_block if present
    IRLogicBlock* logic_block = NULL;
    cJSON* logic_json = cJSON_GetObjectItem(kir, "logic_block");
    if (logic_json) {
        extern IRLogicBlock* ir_logic_block_from_json(cJSON* json);
        logic_block = ir_logic_block_from_json(logic_json);
    }

    // Create Lua context
    LuaContext* ctx = create_lua_context(logic_block);
    if (!ctx) {
        extern void ir_destroy_component(IRComponent* component);
        ir_destroy_component(root);
        if (logic_block) {
            extern void ir_logic_block_free(IRLogicBlock* block);
            ir_logic_block_free(logic_block);
        }
        cJSON_Delete(kir);
        return NULL;
    }

    // Generate header
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity,
                 "-- Generated from KIR by Kryon Lua Codegen\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity,
                 "local UI = require(\"kryon.dsl\")\n");
    
    bool has_reactive = cJSON_GetObjectItem(kir, "reactive_manifest") != NULL;
    if (has_reactive || logic_block) {
        append_string(&ctx->buffer, &ctx->size, &ctx->capacity,
                     "local Reactive = require(\"kryon.reactive\")\n");
    }
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n");

    // Generate reactive state
    cJSON* reactive_manifest = cJSON_GetObjectItem(kir, "reactive_manifest");
    generate_reactive_state(ctx, reactive_manifest);

    // Generate event handlers
    generate_event_handlers(ctx);

    // Generate UI tree
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "-- UI Tree\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "local root = ");
    generate_component(ctx, root);
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n\n");

    // Generate return statement
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "-- Return app\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "return {\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "  root = root\n");
    append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "}\n");

    // Clean up
    char* result = strdup(ctx->buffer);

    destroy_lua_context(ctx);
    extern void ir_destroy_component(IRComponent* component);
    ir_destroy_component(root);
    if (logic_block) {
        extern void ir_logic_block_free(IRLogicBlock* block);
        ir_logic_block_free(logic_block);
    }
    cJSON_Delete(kir);

    return result;
}

// Generate from file
bool lua_codegen_generate(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) return false;

    // Read KIR file
    FILE* f = fopen(kir_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open KIR file: %s\n", kir_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* kir_json = malloc(size + 1);
    if (!kir_json) {
        fclose(f);
        return false;
    }

    fread(kir_json, 1, size, f);
    kir_json[size] = '\0';
    fclose(f);

    // Generate Lua code
    char* lua_code = lua_codegen_from_json(kir_json);
    free(kir_json);

    if (!lua_code) {
        fprintf(stderr, "Error: Failed to generate Lua from KIR\n");
        return false;
    }

    // Write output
    FILE* out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot write output file: %s\n", output_path);
        free(lua_code);
        return false;
    }

    fprintf(out, "%s", lua_code);
    fclose(out);
    free(lua_code);

    return true;
}

// Generate with options
bool lua_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        LuaCodegenOptions* options) {
    (void)options; // Ignore options for now
    return lua_codegen_generate(kir_path, output_path);
}
