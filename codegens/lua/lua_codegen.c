/**
 * Lua Code Generator
 * Generates Lua DSL code from KIR JSON
 */

#define _POSIX_C_SOURCE 200809L

#include "lua_codegen.h"
#include "../../ir/ir_string_builder.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

// String builder wrapper using IRStringBuilder
typedef struct {
    IRStringBuilder* sb;
    int indent;
} LuaCodeGen;

static LuaCodeGen* lua_gen_create(void) {
    LuaCodeGen* gen = calloc(1, sizeof(LuaCodeGen));
    if (!gen) return NULL;

    gen->sb = ir_sb_create(8192);
    if (!gen->sb) {
        free(gen);
        return NULL;
    }
    gen->indent = 0;
    return gen;
}

static void lua_gen_free(LuaCodeGen* gen) {
    if (gen) {
        ir_sb_free(gen->sb);
        free(gen);
    }
}

static bool lua_gen_append(LuaCodeGen* gen, const char* str) __attribute__((unused));
static bool lua_gen_append(LuaCodeGen* gen, const char* str) {
    if (!gen || !gen->sb || !str) return false;
    return ir_sb_append(gen->sb, str);
}

static bool lua_gen_append_fmt(LuaCodeGen* gen, const char* fmt, ...) __attribute__((unused));
static bool lua_gen_append_fmt(LuaCodeGen* gen, const char* fmt, ...) {
    if (!gen || !gen->sb || !fmt) return false;

    va_list args;
    va_start(args, fmt);
    char temp[4096];
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    return ir_sb_append(gen->sb, temp);
}

static void lua_gen_add_line(LuaCodeGen* gen, const char* line) {
    if (!gen || !gen->sb) return;

    // Add indentation
    ir_sb_indent(gen->sb, gen->indent);
    ir_sb_append_line(gen->sb, line);
}

static void lua_gen_add_line_fmt(LuaCodeGen* gen, const char* fmt, ...) {
    if (!gen || !gen->sb || !fmt) return;

    va_list args;
    va_start(args, fmt);
    char temp[4096];
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    ir_sb_indent(gen->sb, gen->indent);
    ir_sb_append_line(gen->sb, temp);
}

/**
 * Convert cJSON to Lua table syntax
 * This converts JSON format to valid Lua table syntax:
 * - Objects: {"key":value} -> {key=value}
 * - Arrays: [1,2,3] -> {1,2,3}
 * - true/false/null -> true/false/nil
 */
static char* cJSON_to_lua_syntax(const cJSON* item) {
    if (!item) return strdup("nil");

    char* result = NULL;
    size_t result_size = 0;
    size_t result_capacity = 256;

    result = malloc(result_capacity);
    if (!result) return NULL;

    // Helper to append to result
    int append_str(const char* str) {
        size_t len = strlen(str);
        while (result_size + len >= result_capacity) {
            result_capacity *= 2;
            char* new_result = realloc(result, result_capacity);
            if (!new_result) {
                free(result);
                return -1;
            }
            result = new_result;
        }
        strcpy(result + result_size, str);
        result_size += len;
        return 0;
    }

    int append_char(char c) {
        char str[2] = {c, '\0'};
        return append_str(str);
    }

    switch (cJSON_IsInvalid(item) ? 100 : item->type) {
        case cJSON_NULL:
            append_str("nil");
            break;

        case cJSON_False:
        case cJSON_True:
            append_str(cJSON_IsTrue(item) ? "true" : "false");
            break;

        case cJSON_Number:
            if (item->valuedouble == (double)(int)item->valuedouble) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%d", (int)item->valuedouble);
                append_str(buf);
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", item->valuedouble);
                append_str(buf);
            }
            break;

        case cJSON_String: {
            append_char('"');
            const char* str = cJSON_GetStringValue(item);
            for (size_t i = 0; str && str[i]; i++) {
                if (str[i] == '"' || str[i] == '\\') {
                    append_char('\\');
                }
                append_char(str[i]);
            }
            append_char('"');
            break;
        }

        case cJSON_Array: {
            append_char('{');
            const cJSON* child = item->child;
            bool first = true;
            while (child) {
                if (!first) append_char(',');
                first = false;
                char* child_lua = cJSON_to_lua_syntax(child);
                if (child_lua) {
                    append_str(child_lua);
                    free(child_lua);
                }
                child = child->next;
            }
            append_char('}');
            break;
        }

        case cJSON_Object: {
            append_char('{');
            const cJSON* child = item->child;
            bool first = true;
            while (child) {
                if (!first) append_char(',');
                first = false;

                // Key (always a string in JSON objects)
                if (child->string && child->string[0]) {
                    // Check if key is a valid Lua identifier
                    bool is_identifier = true;
                    for (size_t i = 0; child->string[i]; i++) {
                        char c = child->string[i];
                        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                              (c == '_') || (i > 0 && c >= '0' && c <= '9'))) {
                            is_identifier = false;
                            break;
                        }
                    }

                    if (is_identifier) {
                        append_str(child->string);
                    } else {
                        // Quote the key
                        append_char('[');
                        append_char('"');
                        append_str(child->string);
                        append_char('"');
                        append_char(']');
                    }
                }

                // Value separator (Lua uses =, JSON uses :)
                append_char('=');

                // Value
                char* child_lua = cJSON_to_lua_syntax(child);
                if (child_lua) {
                    append_str(child_lua);
                    free(child_lua);
                }
                child = child->next;
            }
            append_char('}');
            break;
        }

        default:
            append_str("nil");
            break;
    }

    append_char('\0');
    return result;
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
    bool is_foreach = false;
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
    else if (strcmp(comp_type, "ForEach") == 0) {
        dsl_func = "ForEach";
        is_foreach = true;
    }

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

    cJSON* background = cJSON_GetObjectItem(comp, "background");
    if (background && cJSON_IsString(background)) {
        lua_gen_add_line_fmt(gen, "background = \"%s\",", cJSON_GetStringValue(background));
    }

    cJSON* color = cJSON_GetObjectItem(comp, "color");
    if (color && cJSON_IsString(color)) {
        lua_gen_add_line_fmt(gen, "color = \"%s\",", cJSON_GetStringValue(color));
    }

    cJSON* borderRadius = cJSON_GetObjectItem(comp, "borderRadius");
    if (borderRadius && cJSON_IsNumber(borderRadius) && cJSON_GetNumberValue(borderRadius) > 0) {
        lua_gen_add_line_fmt(gen, "borderRadius = %d,", (int)cJSON_GetNumberValue(borderRadius));
    }

    cJSON* borderWidth = cJSON_GetObjectItem(comp, "borderWidth");
    if (borderWidth && cJSON_IsNumber(borderWidth) && cJSON_GetNumberValue(borderWidth) > 0) {
        lua_gen_add_line_fmt(gen, "borderWidth = %d,", (int)cJSON_GetNumberValue(borderWidth));
    }

    cJSON* borderColor = cJSON_GetObjectItem(comp, "borderColor");
    if (borderColor && cJSON_IsString(borderColor)) {
        lua_gen_add_line_fmt(gen, "borderColor = \"%s\",", cJSON_GetStringValue(borderColor));
    }

    cJSON* gap = cJSON_GetObjectItem(comp, "gap");
    if (gap && cJSON_IsNumber(gap) && cJSON_GetNumberValue(gap) > 0) {
        lua_gen_add_line_fmt(gen, "gap = %d,", (int)cJSON_GetNumberValue(gap));
    }

    cJSON* margin = cJSON_GetObjectItem(comp, "margin");
    if (margin && cJSON_IsNumber(margin) && cJSON_GetNumberValue(margin) > 0) {
        lua_gen_add_line_fmt(gen, "margin = %d,", (int)cJSON_GetNumberValue(margin));
    }

    cJSON* opacity = cJSON_GetObjectItem(comp, "opacity");
    if (opacity && cJSON_IsNumber(opacity)) {
        double op = cJSON_GetNumberValue(opacity);
        if (op < 1.0) {  // Only output if not fully opaque
            lua_gen_add_line_fmt(gen, "opacity = %.2f,", op);
        }
    }

    cJSON* alignItems = cJSON_GetObjectItem(comp, "alignItems");
    if (alignItems && cJSON_IsString(alignItems)) {
        lua_gen_add_line_fmt(gen, "alignItems = \"%s\",", cJSON_GetStringValue(alignItems));
    }

    cJSON* justifyContent = cJSON_GetObjectItem(comp, "justifyContent");
    if (justifyContent && cJSON_IsString(justifyContent)) {
        lua_gen_add_line_fmt(gen, "justifyContent = \"%s\",", cJSON_GetStringValue(justifyContent));
    }

    cJSON* textAlign = cJSON_GetObjectItem(comp, "textAlign");
    if (textAlign && cJSON_IsString(textAlign)) {
        lua_gen_add_line_fmt(gen, "textAlign = \"%s\",", cJSON_GetStringValue(textAlign));
    }

    cJSON* placeholder = cJSON_GetObjectItem(comp, "placeholder");
    if (placeholder && cJSON_IsString(placeholder)) {
        lua_gen_add_line_fmt(gen, "placeholder = \"%s\",", cJSON_GetStringValue(placeholder));
    }

    cJSON* value = cJSON_GetObjectItem(comp, "value");
    if (value && cJSON_IsString(value)) {
        lua_gen_add_line_fmt(gen, "value = \"%s\",", cJSON_GetStringValue(value));
    }

    cJSON* checked = cJSON_GetObjectItem(comp, "checked");
    if (checked && cJSON_IsBool(checked)) {
        lua_gen_add_line_fmt(gen, "checked = %s,", cJSON_IsTrue(checked) ? "true" : "false");
    }

    cJSON* disabled = cJSON_GetObjectItem(comp, "disabled");
    if (disabled && cJSON_IsBool(disabled)) {
        lua_gen_add_line_fmt(gen, "disabled = %s,", cJSON_IsTrue(disabled) ? "true" : "false");
    }

    cJSON* visible = cJSON_GetObjectItem(comp, "visible");
    if (visible && cJSON_IsBool(visible) && !cJSON_IsTrue(visible)) {
        lua_gen_add_line(gen, "visible = false,");
    }

    cJSON* windowTitle = cJSON_GetObjectItem(comp, "windowTitle");
    if (windowTitle && cJSON_IsString(windowTitle)) {
        lua_gen_add_line_fmt(gen, "windowTitle = \"%s\",", cJSON_GetStringValue(windowTitle));
    }

    cJSON* minWidth = cJSON_GetObjectItem(comp, "minWidth");
    if (minWidth && cJSON_IsString(minWidth)) {
        lua_gen_add_line_fmt(gen, "minWidth = \"%s\",", cJSON_GetStringValue(minWidth));
    }

    cJSON* minHeight = cJSON_GetObjectItem(comp, "minHeight");
    if (minHeight && cJSON_IsString(minHeight)) {
        lua_gen_add_line_fmt(gen, "minHeight = \"%s\",", cJSON_GetStringValue(minHeight));
    }

    cJSON* maxWidth = cJSON_GetObjectItem(comp, "maxWidth");
    if (maxWidth && cJSON_IsString(maxWidth)) {
        lua_gen_add_line_fmt(gen, "maxWidth = \"%s\",", cJSON_GetStringValue(maxWidth));
    }

    cJSON* maxHeight = cJSON_GetObjectItem(comp, "maxHeight");
    if (maxHeight && cJSON_IsString(maxHeight)) {
        lua_gen_add_line_fmt(gen, "maxHeight = \"%s\",", cJSON_GetStringValue(maxHeight));
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

    // Handle ForEach-specific properties
    if (is_foreach) {
        cJSON* custom_data = cJSON_GetObjectItem(comp, "custom_data");
        if (custom_data && cJSON_IsObject(custom_data)) {
            cJSON* each_source = cJSON_GetObjectItem(custom_data, "each_source");
            cJSON* each_item_name = cJSON_GetObjectItem(custom_data, "each_item_name");
            cJSON* each_index_name = cJSON_GetObjectItem(custom_data, "each_index_name");

            // Output 'as' property (item name)
            if (each_item_name && cJSON_IsString(each_item_name)) {
                const char* item_name = cJSON_GetStringValue(each_item_name);
                lua_gen_add_line_fmt(gen, "as = \"%s\",", item_name);
            }

            // Output 'index' property (index name)
            if (each_index_name && cJSON_IsString(each_index_name)) {
                const char* index_name = cJSON_GetStringValue(each_index_name);
                if (strcmp(index_name, "index") != 0) {  // Only output if not default
                    lua_gen_add_line_fmt(gen, "index = \"%s\",", index_name);
                }
            }

            // Output 'each' property (data source)
            if (each_source) {
                char* source_str = cJSON_to_lua_syntax(each_source);
                if (source_str) {
                    lua_gen_add_line_fmt(gen, "each = %s,", source_str);
                    free(source_str);
                }
            }
        }
    }

    // Generate children
    cJSON* children = cJSON_GetObjectItem(comp, "children");
    if (children && cJSON_IsArray(children) && cJSON_GetArraySize(children) > 0) {
        if (is_foreach) {
            // ForEach children should be wrapped in render function
            cJSON* custom_data = cJSON_GetObjectItem(comp, "custom_data");
            const char* item_name = "item";
            if (custom_data && cJSON_IsObject(custom_data)) {
                cJSON* each_item_name = cJSON_GetObjectItem(custom_data, "each_item_name");
                if (each_item_name && cJSON_IsString(each_item_name)) {
                    item_name = cJSON_GetStringValue(each_item_name);
                }
            }
            lua_gen_add_line(gen, "");
            lua_gen_add_line_fmt(gen, "render = function(%s, %s_index)",
                                 item_name, item_name);
            gen->indent++;
            lua_gen_add_line(gen, "return");
            gen->indent++;
        } else {
            lua_gen_add_line(gen, "");
        }

        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            generate_component_tree(gen, child, NULL, handler_ctx, true);
            if (!is_foreach) {
                lua_gen_add_line(gen, ",");
            }
        }

        if (is_foreach) {
            gen->indent--;  // Dedent for children
            gen->indent--;  // Dedent for function end
            lua_gen_add_line(gen, "end,");
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

    // Footer - check for window metadata in app section
    lua_gen_add_line(gen, "");
    cJSON* app = cJSON_GetObjectItem(root, "app");
    if (app && cJSON_IsObject(app)) {
        cJSON* title = cJSON_GetObjectItem(app, "windowTitle");
        cJSON* width = cJSON_GetObjectItem(app, "windowWidth");
        cJSON* height = cJSON_GetObjectItem(app, "windowHeight");

        if (title || width || height) {
            lua_gen_add_line(gen, "return {");
            gen->indent++;
            lua_gen_add_line(gen, "root = root,");

            if (title && cJSON_IsString(title)) {
                lua_gen_add_line_fmt(gen, "window = {title = \"%s\",",
                                     cJSON_GetStringValue(title));
            } else {
                lua_gen_add_line(gen, "window = {");
            }

            gen->indent++;
            if (width && cJSON_IsNumber(width)) {
                int w = (int)cJSON_GetNumberValue(width);
                lua_gen_add_line_fmt(gen, "width = %d,", w);
            }
            if (height && cJSON_IsNumber(height)) {
                int h = (int)cJSON_GetNumberValue(height);
                lua_gen_add_line_fmt(gen, "height = %d", h);
            }
            gen->indent--;
            lua_gen_add_line(gen, "}");
            gen->indent--;
            lua_gen_add_line(gen, "}");
        } else {
            lua_gen_add_line(gen, "return root");
        }
    } else {
        lua_gen_add_line(gen, "return root");
    }

    // Clone the string builder to get the result without freeing the original
    IRStringBuilder* clone = ir_sb_clone(gen->sb);
    char* result = clone ? ir_sb_build(clone) : NULL;
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

    size_t bytes_read = fread(kir_json, 1, size, f);
    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Error: Failed to read complete KIR file\n");
        free(kir_json);
        fclose(f);
        return false;
    }
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

    size_t bytes_read = fread(kir_json, 1, size, f);
    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Error: Failed to read complete KIR file\n");
        free(kir_json);
        fclose(f);
        return false;
    }
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

/**
 * Generate multiple Lua files from multi-file KIR with exact source preservation
 */
bool lua_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: Invalid arguments to lua_codegen_generate_multi\n");
        return false;
    }

    // Read KIR file
    FILE* f = fopen(kir_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not read KIR file: %s\n", kir_path);
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read file
    char* kir_json = malloc(size + 1);
    if (!kir_json) {
        fclose(f);
        return false;
    }

    size_t read_size = fread(kir_json, 1, size, f);
    kir_json[read_size] = '\0';
    fclose(f);

    // Parse JSON
    cJSON* root = cJSON_Parse(kir_json);
    free(kir_json);

    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return false;
    }

    // Check for sources section
    cJSON* sources = cJSON_GetObjectItem(root, "sources");
    if (!sources) {
        fprintf(stderr, "Error: KIR file does not contain sources section\n");
        fprintf(stderr, "       Use lua_codegen_generate() for single-file codegen\n");
        cJSON_Delete(root);
        return false;
    }

    // Create output directory if it doesn't exist
    struct stat st = {0};
    if (stat(output_dir, &st) == -1) {
        // Directory doesn't exist, create it
        char mkdir_cmd[2048];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
        if (system(mkdir_cmd) != 0) {
            fprintf(stderr, "Error: Could not create output directory: %s\n", output_dir);
            cJSON_Delete(root);
            return false;
        }
    }

    // Write each source file
    cJSON* module __attribute__((unused)) = NULL;
    cJSON* import_item = NULL;
    int files_written = 0;

    // First, write main.lua (entry point)
    cJSON* main_source = cJSON_GetObjectItem(sources, "main");
    if (main_source) {
        cJSON* source = cJSON_GetObjectItem(main_source, "source");
        if (source && cJSON_IsString(source)) {
            char main_path[2048];
            snprintf(main_path, sizeof(main_path), "%s/main.lua", output_dir);

            FILE* out = fopen(main_path, "w");
            if (out) {
                fputs(source->valuestring, out);
                fclose(out);
                printf("✓ Generated: main.lua\n");
                files_written++;
            }
        }
    }

    // Write all other modules
    cJSON* imports = cJSON_GetObjectItem(root, "imports");
    if (imports && cJSON_IsArray(imports)) {
        cJSON_ArrayForEach(import_item, imports) {
            if (cJSON_IsString(import_item)) {
                const char* module_id = import_item->valuestring;

                // Skip "main" as it's already written
                if (strcmp(module_id, "main") == 0) {
                    continue;
                }

                // Skip Kryon internal modules
                if (strcmp(module_id, "dsl") == 0 ||
                    strcmp(module_id, "ffi") == 0 ||
                    strcmp(module_id, "runtime") == 0 ||
                    strcmp(module_id, "reactive") == 0 ||
                    strcmp(module_id, "kryon") == 0) {
                    continue;
                }

                cJSON* module_source = cJSON_GetObjectItem(sources, module_id);
                if (module_source) {
                    cJSON* source = cJSON_GetObjectItem(module_source, "source");
                    if (source && cJSON_IsString(source)) {
                        // Convert module_id to file path
                        // e.g., "components/calendar" -> "components/calendar.lua"
                        char file_path[2048];
                        snprintf(file_path, sizeof(file_path), "%s/%s.lua", output_dir, module_id);

                        // Create subdirectories if needed
                        char* last_slash = strrchr(file_path, '/');
                        if (last_slash) {
                            *last_slash = '\0';
                            struct stat st2 = {0};
                            if (stat(file_path, &st2) == -1) {
                                char mkdir_cmd[2048];
                                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", file_path);
                                int mkdir_result = system(mkdir_cmd);
                                if (mkdir_result != 0) {
                                    fprintf(stderr, "Warning: mkdir command failed with code %d\n", mkdir_result);
                                }
                            }
                            *last_slash = '/';
                        }

                        // Write file
                        FILE* out = fopen(file_path, "w");
                        if (out) {
                            fputs(source->valuestring, out);
                            fclose(out);
                            printf("✓ Generated: %s.lua\n", module_id);
                            files_written++;
                        }
                    }
                }
            }
        }
    }

    cJSON_Delete(root);

    if (files_written == 0) {
        fprintf(stderr, "Warning: No source files were written\n");
        return false;
    }

    printf("✓ Generated %d Lua files in %s\n", files_written, output_dir);
    return true;
}
