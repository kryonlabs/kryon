/**
 * Lua Code Generator
 * Generates Lua DSL code from KIR JSON
 */

#define _POSIX_C_SOURCE 200809L

#include "lua_codegen.h"
#include "../../ir/third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// String builder helper
typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
    int indent;
} LuaCodeGen;

static LuaCodeGen* lua_gen_create(void) {
    LuaCodeGen* gen = malloc(sizeof(LuaCodeGen));
    if (!gen) return NULL;

    gen->capacity = 8192;
    gen->size = 0;
    gen->indent = 0;
    gen->buffer = malloc(gen->capacity);
    if (!gen->buffer) {
        free(gen);
        return NULL;
    }
    gen->buffer[0] = '\0';
    return gen;
}

static void lua_gen_free(LuaCodeGen* gen) {
    if (gen) {
        free(gen->buffer);
        free(gen);
    }
}

static bool lua_gen_append(LuaCodeGen* gen, const char* str) {
    if (!gen || !str) return false;

    size_t len = strlen(str);
    while (gen->size + len >= gen->capacity) {
        gen->capacity *= 2;
        char* new_buffer = realloc(gen->buffer, gen->capacity);
        if (!new_buffer) return false;
        gen->buffer = new_buffer;
    }

    strcpy(gen->buffer + gen->size, str);
    gen->size += len;
    return true;
}

static bool lua_gen_append_fmt(LuaCodeGen* gen, const char* fmt, ...) {
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    return lua_gen_append(gen, temp);
}

static void lua_gen_add_line(LuaCodeGen* gen, const char* line) {
    // Add indentation
    for (int i = 0; i < gen->indent; i++) {
        lua_gen_append(gen, "  ");
    }
    lua_gen_append(gen, line);
    lua_gen_append(gen, "\n");
}

static void lua_gen_add_line_fmt(LuaCodeGen* gen, const char* fmt, ...) {
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    lua_gen_add_line(gen, temp);
}

// Generate reactive variables
static void generate_reactive_vars(LuaCodeGen* gen, cJSON* manifest) {
    if (!manifest || !cJSON_IsObject(manifest)) return;

    cJSON* variables = cJSON_GetObjectItem(manifest, "variables");
    if (!variables || !cJSON_IsArray(variables) || cJSON_GetArraySize(variables) == 0) {
        return;
    }

    lua_gen_add_line(gen, "");
    lua_gen_add_line(gen, "-- Reactive State");

    cJSON* var = NULL;
    cJSON_ArrayForEach(var, variables) {
        cJSON* name_node = cJSON_GetObjectItem(var, "name");
        cJSON* type_node = cJSON_GetObjectItem(var, "type");
        cJSON* initial_node = cJSON_GetObjectItem(var, "initial_value");

        if (!name_node || !cJSON_IsString(name_node)) continue;

        const char* name = cJSON_GetStringValue(name_node);
        const char* type = type_node ? cJSON_GetStringValue(type_node) : "any";
        const char* initial = initial_node ? cJSON_GetStringValue(initial_node) : "nil";

        // Format value based on type
        if (strcmp(type, "string") == 0) {
            lua_gen_add_line_fmt(gen, "local %s = Reactive.state(\"%s\")", name, initial);
        } else {
            lua_gen_add_line_fmt(gen, "local %s = Reactive.state(%s)", name, initial);
        }
    }

    lua_gen_add_line(gen, "");
}

// Event handler context for storing logic_block data
typedef struct {
    cJSON* functions;      // logic_block.functions array
    cJSON* event_bindings; // logic_block.event_bindings array
} EventHandlerContext;

// Find event handler source for a component
static char* find_event_handler(EventHandlerContext* ctx, int component_id, const char* event_type) {
    if (!ctx || !ctx->functions || !ctx->event_bindings) return NULL;

    // Find binding for this component and event type
    cJSON* binding = NULL;
    cJSON_ArrayForEach(binding, ctx->event_bindings) {
        cJSON* comp_id = cJSON_GetObjectItem(binding, "component_id");
        cJSON* evt_type = cJSON_GetObjectItem(binding, "event_type");

        if (comp_id && cJSON_IsNumber(comp_id) &&
            (int)cJSON_GetNumberValue(comp_id) == component_id &&
            evt_type && cJSON_IsString(evt_type) &&
            strcmp(cJSON_GetStringValue(evt_type), event_type) == 0) {

            // Found the binding, now get the handler function
            cJSON* handler_name_node = cJSON_GetObjectItem(binding, "handler_name");
            if (!handler_name_node || !cJSON_IsString(handler_name_node)) continue;

            const char* handler_name = cJSON_GetStringValue(handler_name_node);

            // Find function in functions array
            cJSON* func = NULL;
            cJSON_ArrayForEach(func, ctx->functions) {
                cJSON* name_node = cJSON_GetObjectItem(func, "name");
                if (name_node && cJSON_IsString(name_node) &&
                    strcmp(cJSON_GetStringValue(name_node), handler_name) == 0) {

                    // Get source code
                    cJSON* sources = cJSON_GetObjectItem(func, "sources");
                    if (sources && cJSON_IsArray(sources)) {
                        // Look for Lua source first
                        cJSON* source = NULL;
                        cJSON_ArrayForEach(source, sources) {
                            cJSON* lang = cJSON_GetObjectItem(source, "language");
                            if (lang && cJSON_IsString(lang) &&
                                strcmp(cJSON_GetStringValue(lang), "lua") == 0) {
                                cJSON* code = cJSON_GetObjectItem(source, "source");
                                if (code && cJSON_IsString(code)) {
                                    return strdup(cJSON_GetStringValue(code));
                                }
                            }
                        }

                        // Fall back to kry source (Lua uses print() so minimal translation needed)
                        cJSON* kry_source = cJSON_GetArrayItem(sources, 0);
                        if (kry_source) {
                            cJSON* code_node = cJSON_GetObjectItem(kry_source, "source");
                            if (code_node && cJSON_IsString(code_node)) {
                                const char* code = cJSON_GetStringValue(code_node);

                                // Clean up whitespace and braces
                                while (*code && (*code == ' ' || *code == '\t' || *code == '{')) code++;
                                const char* end = code + strlen(code) - 1;
                                while (end > code && (*end == ' ' || *end == '\t' || *end == '}')) end--;

                                size_t len = end - code + 1;
                                char cleaned[1024];
                                strncpy(cleaned, code, len);
                                cleaned[len] = '\0';

                                // Lua already uses print(), so just return cleaned code
                                return strdup(cleaned);
                            }
                        }
                    }
                }
            }
        }
    }

    return NULL;
}

// Forward declaration
static void generate_component_tree(LuaCodeGen* gen, cJSON* comp, const char* var_name, EventHandlerContext* handler_ctx, bool is_inline);

// Generate component tree recursively
static void generate_component_tree(LuaCodeGen* gen, cJSON* comp, const char* var_name, EventHandlerContext* handler_ctx, bool is_inline) {
    if (!comp || !cJSON_IsObject(comp)) return;

    const char* comp_type = "Container";
    cJSON* type_node = cJSON_GetObjectItem(comp, "type");
    if (type_node && cJSON_IsString(type_node)) {
        comp_type = cJSON_GetStringValue(type_node);
    }

    // Get component ID for event handler lookup
    int component_id = -1;
    cJSON* id_node = cJSON_GetObjectItem(comp, "id");
    if (id_node && cJSON_IsNumber(id_node)) {
        component_id = (int)cJSON_GetNumberValue(id_node);
    }

    // Map IR type to Lua DSL function
    const char* dsl_func = "Container";
    if (strcmp(comp_type, "Row") == 0) dsl_func = "Row";
    else if (strcmp(comp_type, "Column") == 0) dsl_func = "Column";
    else if (strcmp(comp_type, "Text") == 0) dsl_func = "Text";
    else if (strcmp(comp_type, "Button") == 0) dsl_func = "Button";
    else if (strcmp(comp_type, "Checkbox") == 0) dsl_func = "Checkbox";
    else if (strcmp(comp_type, "Input") == 0) dsl_func = "Input";
    else if (strcmp(comp_type, "TabGroup") == 0) dsl_func = "TabGroup";
    else if (strcmp(comp_type, "TabBar") == 0) dsl_func = "TabBar";
    else if (strcmp(comp_type, "TabContent") == 0) dsl_func = "TabContent";
    else if (strcmp(comp_type, "TabPanel") == 0) dsl_func = "TabPanel";

    if (is_inline) {
        lua_gen_add_line_fmt(gen, "UI.%s {", dsl_func);
    } else {
        lua_gen_add_line_fmt(gen, "local %s = UI.%s {", var_name, dsl_func);
    }
    gen->indent++;

    // Generate properties
    cJSON* width = cJSON_GetObjectItem(comp, "width");
    if (width && cJSON_IsString(width)) {
        const char* w = cJSON_GetStringValue(width);
        if (strcmp(w, "0.0px") != 0 && strcmp(w, "0px") != 0) {
            lua_gen_add_line_fmt(gen, "width = \"%s\",", w);
        }
    }

    cJSON* height = cJSON_GetObjectItem(comp, "height");
    if (height && cJSON_IsString(height)) {
        const char* h = cJSON_GetStringValue(height);
        if (strcmp(h, "0.0px") != 0 && strcmp(h, "0px") != 0) {
            lua_gen_add_line_fmt(gen, "height = \"%s\",", h);
        }
    }

    cJSON* padding = cJSON_GetObjectItem(comp, "padding");
    if (padding && cJSON_IsNumber(padding) && cJSON_GetNumberValue(padding) > 0) {
        lua_gen_add_line_fmt(gen, "padding = %d,", (int)cJSON_GetNumberValue(padding));
    }

    cJSON* fontSize = cJSON_GetObjectItem(comp, "fontSize");
    if (fontSize && cJSON_IsNumber(fontSize) && cJSON_GetNumberValue(fontSize) > 0) {
        lua_gen_add_line_fmt(gen, "fontSize = %d,", (int)cJSON_GetNumberValue(fontSize));
    }

    cJSON* fontWeight = cJSON_GetObjectItem(comp, "fontWeight");
    if (fontWeight && cJSON_IsNumber(fontWeight) && cJSON_GetNumberValue(fontWeight) != 400) {
        lua_gen_add_line_fmt(gen, "fontWeight = %d,", (int)cJSON_GetNumberValue(fontWeight));
    }

    cJSON* text = cJSON_GetObjectItem(comp, "text");
    if (text && cJSON_IsString(text)) {
        lua_gen_add_line_fmt(gen, "text = \"%s\",", cJSON_GetStringValue(text));
    }

    cJSON* backgroundColor = cJSON_GetObjectItem(comp, "backgroundColor");
    if (backgroundColor && cJSON_IsString(backgroundColor)) {
        lua_gen_add_line_fmt(gen, "backgroundColor = \"%s\",", cJSON_GetStringValue(backgroundColor));
    }

    cJSON* color = cJSON_GetObjectItem(comp, "color");
    if (color && cJSON_IsString(color)) {
        lua_gen_add_line_fmt(gen, "color = \"%s\",", cJSON_GetStringValue(color));
    }

    cJSON* flexDirection = cJSON_GetObjectItem(comp, "flexDirection");
    if (flexDirection && cJSON_IsString(flexDirection)) {
        const char* flexDir = cJSON_GetStringValue(flexDirection);
        if (strcmp(flexDir, "column") == 0) {
            lua_gen_add_line(gen, "layoutDirection = \"column\",");
        }
    }

    // Add event handlers if available
    if (component_id >= 0 && handler_ctx) {
        char* click_handler = find_event_handler(handler_ctx, component_id, "click");
        if (click_handler) {
            lua_gen_add_line_fmt(gen, "onClick = function()");
            gen->indent++;
            lua_gen_add_line_fmt(gen, "%s", click_handler);
            gen->indent--;
            lua_gen_add_line(gen, "end,");
            free(click_handler);
        }
    }

    // Generate children
    cJSON* children = cJSON_GetObjectItem(comp, "children");
    if (children && cJSON_IsArray(children) && cJSON_GetArraySize(children) > 0) {
        lua_gen_add_line(gen, "");

        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            generate_component_tree(gen, child, NULL, handler_ctx, true);
            lua_gen_add_line(gen, ",");
        }
    }

    gen->indent--;
    lua_gen_add_line(gen, "}");
}

char* lua_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    // Parse JSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return NULL;
    }

    LuaCodeGen* gen = lua_gen_create();
    if (!gen) {
        cJSON_Delete(root);
        return NULL;
    }

    // Header
    lua_gen_add_line(gen, "-- Generated from .kir by Kryon Code Generator");
    lua_gen_add_line(gen, "-- Uses Smart DSL syntax for clean, readable code");
    lua_gen_add_line(gen, "-- Do not edit manually - regenerate from source");
    lua_gen_add_line(gen, "");
    lua_gen_add_line(gen, "local Reactive = require(\"kryon.reactive\")");
    lua_gen_add_line(gen, "local UI = require(\"kryon.dsl\")");

    // Generate reactive variables if manifest exists
    cJSON* manifest = cJSON_GetObjectItem(root, "reactive_manifest");
    if (manifest) {
        generate_reactive_vars(gen, manifest);
    }

    // Extract logic_block for event handlers
    EventHandlerContext handler_ctx = {0};
    cJSON* logic_block = cJSON_GetObjectItem(root, "logic_block");
    if (logic_block && cJSON_IsObject(logic_block)) {
        handler_ctx.functions = cJSON_GetObjectItem(logic_block, "functions");
        handler_ctx.event_bindings = cJSON_GetObjectItem(logic_block, "event_bindings");
    }

    // Generate component tree
    lua_gen_add_line(gen, "-- UI Component Tree");

    cJSON* component = cJSON_GetObjectItem(root, "root");
    if (!component) component = cJSON_GetObjectItem(root, "component");

    if (component) {
        generate_component_tree(gen, component, "root", &handler_ctx, false);
    }

    // Footer
    lua_gen_add_line(gen, "");
    lua_gen_add_line(gen, "return root");

    char* result = strdup(gen->buffer);
    lua_gen_free(gen);
    cJSON_Delete(root);
    return result;
}

bool lua_codegen_generate(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) return false;

    // Read KIR file
    FILE* f = fopen(kir_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open KIR file: %s\n", kir_path);
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
        return false;
    }

    // Write output
    f = fopen(output_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not write output file: %s\n", output_path);
        free(lua_code);
        return false;
    }

    fputs(lua_code, f);
    fclose(f);
    free(lua_code);

    printf("✓ Generated Lua code: %s\n", output_path);
    return true;
}

bool lua_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        LuaCodegenOptions* options) {
    if (!kir_path || !output_path) {
        return false;
    }

    // Use defaults if no options provided
    LuaCodegenOptions default_opts = {
        .format = true,
        .optimize = false
    };
    if (!options) {
        options = &default_opts;
    }

    // Read KIR file
    FILE* f = fopen(kir_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open KIR file: %s\n", kir_path);
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
        return false;
    }

    // Write output
    f = fopen(output_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not write output file: %s\n", output_path);
        free(lua_code);
        return false;
    }

    fputs(lua_code, f);
    fclose(f);
    free(lua_code);

    printf("✓ Generated Lua code: %s\n", output_path);

    // Apply formatting if requested
    if (options->format) {
        // Lua formatter - could use lua-format or stylua if available
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "stylua \"%s\" 2>/dev/null || lua-format -i \"%s\" 2>/dev/null",
                 output_path, output_path);
        int result = system(cmd);
        if (result == 0) {
            printf("  Formatted with lua-format/stylua\n");
        }
    }

    // Note: optimize option affects runtime, not codegen
    // Could be used in future to generate optimized Lua patterns

    return true;
}
