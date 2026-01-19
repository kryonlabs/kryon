/**
 * Lua Code Generator
 * Generates Lua DSL code from KIR JSON
 */

#define _POSIX_C_SOURCE 200809L

#include "lua_codegen.h"
#include "../codegen_common.h"
#include "../../ir/src/utils/ir_string_builder.h"
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
            const char* str = cJSON_GetStringValue(item);
            if (!str) {
                append_str("\"\"");
                break;
            }

            // Check if string contains newlines or other special chars that need bracket syntax
            bool has_newline = false;
            bool has_bracket = false;
            for (size_t i = 0; str[i]; i++) {
                if (str[i] == '\n' || str[i] == '\r') {
                    has_newline = true;
                }
                if (str[i] == ']' && str[i+1] == ']') {
                    has_bracket = true;
                }
            }

            if (has_newline) {
                // Use Lua bracket string syntax for multi-line strings
                if (has_bracket) {
                    // String contains ]], use [=[...]=] syntax
                    append_str("[=[");
                    append_str(str);
                    append_str("]=]");
                } else {
                    append_str("[[");
                    append_str(str);
                    append_str("]]");
                }
            } else {
                // Use regular quoted string for single-line strings
                append_char('"');
                for (size_t i = 0; str[i]; i++) {
                    if (str[i] == '"' || str[i] == '\\') {
                        append_char('\\');
                    }
                    append_char(str[i]);
                }
                append_char('"');
            }
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

    // Check for source_declarations (Phase 1: Source Preservation)
    cJSON* source_decls = cJSON_GetObjectItem(root, "source_declarations");

    // Check early if this is a component module (has component_definitions)
    // Component modules handle function output differently to avoid duplication
    cJSON* component_defs_early = cJSON_GetObjectItem(root, "component_definitions");
    bool is_component_module = (component_defs_early && cJSON_IsArray(component_defs_early) &&
                                cJSON_GetArraySize(component_defs_early) > 0);

    if (source_decls && cJSON_IsObject(source_decls)) {
        // Use preserved requires from original source
        // NOTE: For component modules, requires are output later in the component_defs block
        // to avoid duplicate output. Only output here for main modules.
        if (!is_component_module) {
            cJSON* requires = cJSON_GetObjectItem(source_decls, "requires");
            if (requires && cJSON_IsArray(requires)) {
                cJSON* req = NULL;
                cJSON_ArrayForEach(req, requires) {
                    cJSON* var = cJSON_GetObjectItem(req, "variable");
                    cJSON* mod = cJSON_GetObjectItem(req, "module");
                    if (var && mod && cJSON_IsString(var) && cJSON_IsString(mod)) {
                        lua_gen_add_line_fmt(gen, "local %s = require(\"%s\")",
                            cJSON_GetStringValue(var), cJSON_GetStringValue(mod));
                    }
                }
                lua_gen_add_line(gen, "");
            }
            // Output helper functions from source_declarations
            // NOTE: For component modules, functions are output in the component_defs block
            cJSON* functions = cJSON_GetObjectItem(source_decls, "functions");
            if (functions && cJSON_IsArray(functions) && cJSON_GetArraySize(functions) > 0) {
                lua_gen_add_line(gen, "-- Helper Functions (preserved from source)");
                cJSON* fn = NULL;
                cJSON_ArrayForEach(fn, functions) {
                    cJSON* source = cJSON_GetObjectItem(fn, "source");
                    if (source && cJSON_IsString(source)) {
                        // Split by newlines and add each line
                        const char* fn_source = cJSON_GetStringValue(source);
                        char* fn_copy = strdup(fn_source);
                        if (fn_copy) {
                            char* line = strtok(fn_copy, "\n");
                            while (line) {
                                lua_gen_add_line(gen, line);
                                line = strtok(NULL, "\n");
                            }
                            free(fn_copy);
                        }
                        lua_gen_add_line(gen, "");
                    }
                }
            }
        }
    } else {
        // Fallback: hardcoded requires for backward compatibility
        lua_gen_add_line(gen, "local Reactive = require(\"kryon.reactive\")");
        lua_gen_add_line(gen, "local UI = require(\"kryon.dsl\")");
    }

    // Generate reactive state and initialization sections
    // Only for main modules (non-component modules)
    bool used_source_state_init = false;
    if (source_decls && cJSON_IsObject(source_decls) && !is_component_module) {
        // Helper lambda to output multiline strings
        #define OUTPUT_MULTILINE(src_str) do { \
            char* copy = strdup(src_str); \
            if (copy) { \
                char* saveptr = NULL; \
                char* line = strtok_r(copy, "\n", &saveptr); \
                while (line) { \
                    lua_gen_add_line(gen, line); \
                    line = strtok_r(NULL, "\n", &saveptr); \
                } \
                free(copy); \
            } \
        } while(0)

        // 1. Output module_init (math.randomseed, Storage.init, etc.)
        cJSON* module_init = cJSON_GetObjectItem(source_decls, "module_init");
        if (module_init && cJSON_IsString(module_init)) {
            lua_gen_add_line(gen, "");
            lua_gen_add_line(gen, "-- Module Initialization (preserved from source)");
            OUTPUT_MULTILINE(cJSON_GetStringValue(module_init));
            lua_gen_add_line(gen, "");
        }

        // 2. Output initialization (local variables between functions and conditional blocks)
        cJSON* initialization = cJSON_GetObjectItem(source_decls, "initialization");
        if (initialization && cJSON_IsString(initialization)) {
            lua_gen_add_line(gen, "");
            lua_gen_add_line(gen, "-- Initialization (preserved from source)");
            OUTPUT_MULTILINE(cJSON_GetStringValue(initialization));
            lua_gen_add_line(gen, "");
        }

        // 3. Output conditional_blocks (if Storage.get then ... end)
        cJSON* conditional_blocks = cJSON_GetObjectItem(source_decls, "conditional_blocks");
        if (conditional_blocks && cJSON_IsString(conditional_blocks)) {
            lua_gen_add_line(gen, "");
            lua_gen_add_line(gen, "-- Conditional Blocks (preserved from source)");
            OUTPUT_MULTILINE(cJSON_GetStringValue(conditional_blocks));
            lua_gen_add_line(gen, "");
        }

        // 4. Output state initialization
        cJSON* state_init = cJSON_GetObjectItem(source_decls, "state_init");
        if (state_init && cJSON_IsObject(state_init)) {
            cJSON* expression = cJSON_GetObjectItem(state_init, "expression");
            if (expression && cJSON_IsString(expression)) {
                const char* expr_str = cJSON_GetStringValue(expression);
                lua_gen_add_line(gen, "");
                lua_gen_add_line(gen, "-- Reactive State (preserved from source)");
                // Use OUTPUT_MULTILINE for multi-line expressions
                OUTPUT_MULTILINE(expr_str);
                lua_gen_add_line(gen, "");
                used_source_state_init = true;
            }
        }

        // 5. Output non_reactive_state (local editingState = {}, etc.)
        cJSON* non_reactive_state = cJSON_GetObjectItem(source_decls, "non_reactive_state");
        if (non_reactive_state && cJSON_IsString(non_reactive_state)) {
            lua_gen_add_line(gen, "");
            lua_gen_add_line(gen, "-- Non-reactive State (preserved from source)");
            OUTPUT_MULTILINE(cJSON_GetStringValue(non_reactive_state));
            lua_gen_add_line(gen, "");
        }

        #undef OUTPUT_MULTILINE
    }

    if (!used_source_state_init) {
        cJSON* manifest = cJSON_GetObjectItem(root, "reactive_manifest");
        if (manifest) {
            // Check if there are any reactive variables (excluding non-reactive ones)
            cJSON* variables = cJSON_GetObjectItem(manifest, "variables");
            bool has_reactive_vars = false;
            if (variables && cJSON_IsArray(variables)) {
                cJSON* var = NULL;
                cJSON_ArrayForEach(var, variables) {
                    cJSON* name_node = cJSON_GetObjectItem(var, "name");
                    if (name_node && cJSON_IsString(name_node)) {
                        const char* name = cJSON_GetStringValue(name_node);
                        // Skip non-reactive variables
                        if (strcmp(name, "app_export") == 0 ||
                            strcmp(name, "initialization") == 0 ||
                            strcmp(name, "module_init") == 0 ||
                            strcmp(name, "module_constants") == 0 ||
                            strcmp(name, "conditional_blocks") == 0 ||
                            strcmp(name, "non_reactive_state") == 0 ||
                            strcmp(name, "functions") == 0) {
                            continue;
                        }
                        has_reactive_vars = true;
                        break;
                    }
                }
            }
            if (has_reactive_vars) {
                // Add Reactive require if not already present
                lua_gen_add_line(gen, "local Reactive = require(\"kryon.reactive\")");
                lua_gen_add_line(gen, "");
            }
        }
    }

    // Extract logic_block for event handlers
    EventHandlerContext handler_ctx = {0};
    cJSON* logic_block = cJSON_GetObjectItem(root, "logic_block");
    if (logic_block && cJSON_IsObject(logic_block)) {
        handler_ctx.functions = cJSON_GetObjectItem(logic_block, "functions");
        handler_ctx.event_bindings = cJSON_GetObjectItem(logic_block, "event_bindings");
    }

    // Check what type of module this is
    cJSON* component = cJSON_GetObjectItem(root, "root");
    if (!component) component = cJSON_GetObjectItem(root, "component");

    cJSON* component_defs = cJSON_GetObjectItem(root, "component_definitions");

    // Check if we have a buildUI function - if so, use it directly instead of expanded tree
    bool has_build_ui = false;
    if (source_decls && cJSON_IsObject(source_decls)) {
        cJSON* funcs = cJSON_GetObjectItem(source_decls, "functions");
        if (funcs && cJSON_IsArray(funcs)) {
            cJSON* fn = NULL;
            cJSON_ArrayForEach(fn, funcs) {
                cJSON* name = cJSON_GetObjectItem(fn, "name");
                if (name && cJSON_IsString(name) && strcmp(cJSON_GetStringValue(name), "buildUI") == 0) {
                    has_build_ui = true;
                    break;
                }
            }
        }
    }

    bool already_returned = false;
    if (has_build_ui) {
        // Use preserved buildUI function - check for app_export first
        cJSON* app_export = cJSON_GetObjectItem(source_decls, "app_export");
        if (app_export && cJSON_IsString(app_export)) {
            // Use preserved app export from source
            lua_gen_add_line(gen, "");
            lua_gen_add_line(gen, "-- App Export (preserved from source)");
            const char* export_code = cJSON_GetStringValue(app_export);
            char* export_copy = strdup(export_code);
            if (export_copy) {
                char* line = strtok(export_copy, "\n");
                while (line) {
                    lua_gen_add_line(gen, line);
                    line = strtok(NULL, "\n");
                }
                free(export_copy);
            }
            already_returned = true;
        } else {
            // Fallback - minimal export using buildUI
            lua_gen_add_line(gen, "");
            lua_gen_add_line(gen, "-- App Export");
            lua_gen_add_line(gen, "return { root = buildUI }");
            already_returned = true;
        }
    } else if (component && !cJSON_IsNull(component)) {
        // Main module with root component tree (fallback when no buildUI)
        lua_gen_add_line(gen, "-- UI Component Tree");
        generate_component_tree(gen, component, "root", &handler_ctx, false);
    } else if (component_defs && cJSON_IsArray(component_defs) && cJSON_GetArraySize(component_defs) > 0) {
        // Component module - check for preserved source
        // Priority: 1) source_declarations.functions, 2) component_definitions[].source, 3) template fallback
        bool has_preserved_source = false;

        // Check source_declarations.functions first
        if (source_decls) {
            cJSON* sd_funcs = cJSON_GetObjectItem(source_decls, "functions");
            if (sd_funcs && cJSON_IsArray(sd_funcs) && cJSON_GetArraySize(sd_funcs) > 0) {
                has_preserved_source = true;
            }
        }

        // Also check if component_definitions have source directly (from static extraction)
        if (!has_preserved_source) {
            cJSON* first_def = cJSON_GetArrayItem(component_defs, 0);
            if (first_def && cJSON_GetObjectItem(first_def, "source")) {
                has_preserved_source = true;
            }
        }

        if (has_preserved_source) {
            // Use preserved source - output requires first
            cJSON* sd_requires = source_decls ? cJSON_GetObjectItem(source_decls, "requires") : NULL;
            if (sd_requires && cJSON_IsArray(sd_requires)) {
                cJSON* req = NULL;
                cJSON_ArrayForEach(req, sd_requires) {
                    cJSON* var = cJSON_GetObjectItem(req, "variable");
                    cJSON* mod = cJSON_GetObjectItem(req, "module");
                    if (var && mod && cJSON_IsString(var) && cJSON_IsString(mod)) {
                        lua_gen_add_line_fmt(gen, "local %s = require(\"%s\")",
                                             cJSON_GetStringValue(var),
                                             cJSON_GetStringValue(mod));
                    }
                }
                lua_gen_add_line(gen, "");
            }

            // Output module_constants (COLORS table, DEFAULT_COLOR, etc.)
            cJSON* module_constants = source_decls ? cJSON_GetObjectItem(source_decls, "module_constants") : NULL;
            if (module_constants && cJSON_IsString(module_constants)) {
                const char* const_str = cJSON_GetStringValue(module_constants);
                if (const_str && strlen(const_str) > 0) {
                    char* const_copy = strdup(const_str);
                    if (const_copy) {
                        char* line = strtok(const_copy, "\n");
                        while (line) {
                            lua_gen_add_line(gen, line);
                            line = strtok(NULL, "\n");
                        }
                        free(const_copy);
                    }
                    lua_gen_add_line(gen, "");
                }
            }

            // Output all preserved functions from source_declarations OR component_definitions
            cJSON* sd_funcs = source_decls ? cJSON_GetObjectItem(source_decls, "functions") : NULL;
            if (sd_funcs && cJSON_IsArray(sd_funcs) && cJSON_GetArraySize(sd_funcs) > 0) {
                // Use source_declarations.functions (preferred - has ALL functions)
                cJSON* fn = NULL;
                cJSON_ArrayForEach(fn, sd_funcs) {
                    cJSON* fn_source = cJSON_GetObjectItem(fn, "source");
                    if (fn_source && cJSON_IsString(fn_source)) {
                        const char* src = cJSON_GetStringValue(fn_source);
                        if (src && strlen(src) > 0) {
                            // Output the function source line by line
                            char* src_copy = strdup(src);
                            if (src_copy) {
                                char* line = strtok(src_copy, "\n");
                                while (line) {
                                    lua_gen_add_line(gen, line);
                                    line = strtok(NULL, "\n");
                                }
                                free(src_copy);
                            }
                            lua_gen_add_line(gen, "");
                        }
                    }
                }
            } else {
                // Fallback: use source from component_definitions (only exported functions)
                cJSON* comp_def = NULL;
                cJSON_ArrayForEach(comp_def, component_defs) {
                    cJSON* def_source = cJSON_GetObjectItem(comp_def, "source");
                    if (def_source && cJSON_IsString(def_source)) {
                        const char* src = cJSON_GetStringValue(def_source);
                        if (src && strlen(src) > 0) {
                            // Output the function source line by line
                            char* src_copy = strdup(src);
                            if (src_copy) {
                                char* line = strtok(src_copy, "\n");
                                while (line) {
                                    lua_gen_add_line(gen, line);
                                    line = strtok(NULL, "\n");
                                }
                                free(src_copy);
                            }
                            lua_gen_add_line(gen, "");
                        }
                    }
                }
            }

            // Generate return statement from exports
            lua_gen_add_line(gen, "return {");
            gen->indent++;
            cJSON* comp_def = NULL;
            int def_count = cJSON_GetArraySize(component_defs);
            int i = 0;
            cJSON_ArrayForEach(comp_def, component_defs) {
                cJSON* name = cJSON_GetObjectItem(comp_def, "name");
                if (name && cJSON_IsString(name)) {
                    const char* func_name = cJSON_GetStringValue(name);
                    if (i < def_count - 1) {
                        lua_gen_add_line_fmt(gen, "%s = %s,", func_name, func_name);
                    } else {
                        lua_gen_add_line_fmt(gen, "%s = %s", func_name, func_name);
                    }
                }
                i++;
            }
            gen->indent--;
            lua_gen_add_line(gen, "}");
            already_returned = true;
        } else {
            // Fallback: generate from template (expanded UI tree)
            lua_gen_add_line(gen, "-- Component Definitions");
            lua_gen_add_line(gen, "local M = {}");
            lua_gen_add_line(gen, "");

            cJSON* comp_def = NULL;
            cJSON_ArrayForEach(comp_def, component_defs) {
                cJSON* name = cJSON_GetObjectItem(comp_def, "name");
                cJSON* template = cJSON_GetObjectItem(comp_def, "template");

                if (name && cJSON_IsString(name) && template) {
                    const char* func_name = cJSON_GetStringValue(name);
                    lua_gen_add_line_fmt(gen, "function M.%s(props)", func_name);
                    gen->indent++;
                    lua_gen_add_line(gen, "props = props or {}");
                    lua_gen_add_line(gen, "return");
                    gen->indent++;
                    generate_component_tree(gen, template, NULL, &handler_ctx, true);
                    gen->indent--;
                    gen->indent--;
                    lua_gen_add_line(gen, "end");
                    lua_gen_add_line(gen, "");
                }
            }
        }
    } else {
        // Library module with exports - use source_declarations for preservation
        lua_gen_add_line(gen, "-- Library Module (no UI components)");

        // Helper macro for outputting multiline strings
        #define LIB_OUTPUT_MULTILINE(src_str) do { \
            char* copy = strdup(src_str); \
            if (copy) { \
                char* saveptr = NULL; \
                char* line = strtok_r(copy, "\n", &saveptr); \
                while (line) { \
                    lua_gen_add_line(gen, line); \
                    line = strtok_r(NULL, "\n", &saveptr); \
                } \
                free(copy); \
            } \
        } while(0)

        // 1. Output requires from source_declarations
        cJSON* sd_requires = source_decls ? cJSON_GetObjectItem(source_decls, "requires") : NULL;
        if (sd_requires && cJSON_IsArray(sd_requires) && cJSON_GetArraySize(sd_requires) > 0) {
            cJSON* req = NULL;
            cJSON_ArrayForEach(req, sd_requires) {
                cJSON* var = cJSON_GetObjectItem(req, "variable");
                cJSON* mod = cJSON_GetObjectItem(req, "module");
                if (var && cJSON_IsString(var) && mod && cJSON_IsString(mod)) {
                    lua_gen_add_line_fmt(gen, "local %s = require(\"%s\")",
                        cJSON_GetStringValue(var), cJSON_GetStringValue(mod));
                }
            }
            lua_gen_add_line(gen, "");
        }

        // 2. Output module constants (COLORS table, DEFAULT_COLOR, etc.)
        cJSON* module_constants = source_decls ? cJSON_GetObjectItem(source_decls, "module_constants") : NULL;
        if (module_constants && cJSON_IsString(module_constants)) {
            const char* const_str = cJSON_GetStringValue(module_constants);
            if (const_str && strlen(const_str) > 0) {
                LIB_OUTPUT_MULTILINE(const_str);
                lua_gen_add_line(gen, "");
            }
        }

        // 3. Output functions from source_declarations
        cJSON* sd_functions = source_decls ? cJSON_GetObjectItem(source_decls, "functions") : NULL;
        if (sd_functions && cJSON_IsArray(sd_functions) && cJSON_GetArraySize(sd_functions) > 0) {
            cJSON* fn = NULL;
            cJSON_ArrayForEach(fn, sd_functions) {
                cJSON* fn_source = cJSON_GetObjectItem(fn, "source");
                if (fn_source && cJSON_IsString(fn_source)) {
                    const char* src = cJSON_GetStringValue(fn_source);
                    if (src && strlen(src) > 0) {
                        LIB_OUTPUT_MULTILINE(src);
                        lua_gen_add_line(gen, "");
                    }
                }
            }
        }

        // 4. Generate return statement based on exports
        cJSON* exports = cJSON_GetObjectItem(root, "exports");
        if (exports && cJSON_IsArray(exports) && cJSON_GetArraySize(exports) > 0) {
            lua_gen_add_line(gen, "return {");
            gen->indent++;
            int exp_count = cJSON_GetArraySize(exports);
            int exp_i = 0;
            cJSON* exp = NULL;
            cJSON_ArrayForEach(exp, exports) {
                cJSON* exp_name = cJSON_GetObjectItem(exp, "name");
                if (exp_name && cJSON_IsString(exp_name)) {
                    const char* name = cJSON_GetStringValue(exp_name);
                    if (exp_i < exp_count - 1) {
                        lua_gen_add_line_fmt(gen, "%s = %s,", name, name);
                    } else {
                        lua_gen_add_line_fmt(gen, "%s = %s", name, name);
                    }
                }
                exp_i++;
            }
            gen->indent--;
            lua_gen_add_line(gen, "}");
            already_returned = true;
        }

        #undef LIB_OUTPUT_MULTILINE
    }

    // Footer - determine what to return based on module type
    // Skip if we've already generated a return statement (e.g., from app_export)
    if (!already_returned) {
        lua_gen_add_line(gen, "");

        // If this is a component or library module, return M
        bool is_component_module = (component_defs && cJSON_IsArray(component_defs) &&
                                    cJSON_GetArraySize(component_defs) > 0) ||
                                   (!component || cJSON_IsNull(component));

        if (is_component_module) {
            lua_gen_add_line(gen, "return M");
        } else {
            // Main module - check for window metadata in app section
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
        }
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

    // Set error prefix for this codegen
    codegen_set_error_prefix("Lua");

    // Read KIR file using shared utility
    char* kir_json = codegen_read_kir_file(kir_path, NULL);
    if (!kir_json) {
        return false;
    }

    // Generate Lua code
    char* lua_code = lua_codegen_from_json(kir_json);
    free(kir_json);

    if (!lua_code) {
        return false;
    }

    // Write output using shared utility
    bool success = codegen_write_output_file(output_path, lua_code);
    free(lua_code);

    if (success) {
        printf("✓ Generated Lua code: %s\n", output_path);
    }
    return success;
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

    // Set error prefix for this codegen
    codegen_set_error_prefix("Lua");

    // Read KIR file using shared utility
    char* kir_json = codegen_read_kir_file(kir_path, NULL);
    if (!kir_json) {
        return false;
    }

    // Generate Lua code
    char* lua_code = lua_codegen_from_json(kir_json);
    free(kir_json);

    if (!lua_code) {
        return false;
    }

    // Write output using shared utility
    bool success = codegen_write_output_file(output_path, lua_code);
    free(lua_code);

    if (!success) {
        return false;
    }

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
 * Helper to check if a module is a Kryon internal module
 */
static bool is_kryon_internal(const char* module_id) {
    if (!module_id) return false;

    // Check for kryon/ prefix
    if (strncmp(module_id, "kryon/", 6) == 0) return true;

    // Check for known internal module names
    if (strcmp(module_id, "dsl") == 0) return true;
    if (strcmp(module_id, "ffi") == 0) return true;
    if (strcmp(module_id, "runtime") == 0) return true;
    if (strcmp(module_id, "reactive") == 0) return true;
    if (strcmp(module_id, "runtime_web") == 0) return true;
    if (strcmp(module_id, "kryon") == 0) return true;

    return false;
}

/**
 * Helper to check if a module is an external plugin
 */
static bool is_external_plugin(const char* module_id) {
    if (!module_id) return false;

    // Known external plugins (runtime dependencies, not source modules)
    if (strcmp(module_id, "datetime") == 0) return true;
    if (strcmp(module_id, "storage") == 0) return true;

    return false;
}

/**
 * Helper to get the parent directory of a path
 */
static void get_parent_dir(const char* path, char* parent, size_t parent_size) {
    if (!path || !parent) return;

    strncpy(parent, path, parent_size - 1);
    parent[parent_size - 1] = '\0';

    char* last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        parent[0] = '.';
        parent[1] = '\0';
    }
}

/**
 * Helper to write file with automatic directory creation
 */
static bool write_file_with_mkdir(const char* path, const char* content) {
    if (!path || !content) return false;

    // Create a mutable copy for directory extraction
    char dir_path[2048];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    // Find and create parent directory
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        struct stat st = {0};
        if (stat(dir_path, &st) == -1) {
            char mkdir_cmd[2048];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", dir_path);
            if (system(mkdir_cmd) != 0) {
                fprintf(stderr, "Warning: Could not create directory: %s\n", dir_path);
            }
        }
    }

    // Write the file
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not write file: %s\n", path);
        return false;
    }
    fputs(content, f);
    fclose(f);
    return true;
}

/**
 * Processed modules tracking for recursive import processing
 */
#define MAX_PROCESSED_MODULES 256

typedef struct {
    char* modules[MAX_PROCESSED_MODULES];
    int count;
} ProcessedModules;

static bool is_module_processed(ProcessedModules* pm, const char* module_id) {
    for (int i = 0; i < pm->count; i++) {
        if (strcmp(pm->modules[i], module_id) == 0) return true;
    }
    return false;
}

static void mark_module_processed(ProcessedModules* pm, const char* module_id) {
    if (pm->count < MAX_PROCESSED_MODULES) {
        pm->modules[pm->count++] = strdup(module_id);
    }
}

static void free_processed_modules(ProcessedModules* pm) {
    for (int i = 0; i < pm->count; i++) {
        free(pm->modules[i]);
    }
    pm->count = 0;
}

/**
 * Recursively process a module and its transitive imports
 */
static int process_module_recursive(const char* module_id, const char* kir_dir,
                                    const char* output_dir, ProcessedModules* processed) {
    // Skip if already processed
    if (is_module_processed(processed, module_id)) return 0;
    mark_module_processed(processed, module_id);

    // Skip internal modules and external plugins
    if (is_kryon_internal(module_id)) return 0;
    if (is_external_plugin(module_id)) return 0;

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

    // Generate Lua from KIR
    char* component_lua = lua_codegen_from_json(component_kir_json);
    free(component_kir_json);

    if (component_lua) {
        // Write to output directory maintaining hierarchy
        char output_path[2048];
        snprintf(output_path, sizeof(output_path),
                 "%s/%s.lua", output_dir, module_id);

        if (write_file_with_mkdir(output_path, component_lua)) {
            printf("✓ Generated: %s.lua\n", module_id);
            files_written++;
        }
        free(component_lua);
    } else {
        fprintf(stderr, "Warning: Failed to generate Lua for '%s'\n", module_id);
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
                    files_written += process_module_recursive(sub_module_id, kir_dir,
                                                              output_dir, processed);
                }
            }
        }
        cJSON_Delete(component_root);
    }

    return files_written;
}

/**
 * Generate multiple Lua files from multi-file KIR by reading linked KIR files
 *
 * This function reads the main.kir file, generates Lua code from its KIR representation,
 * then follows the imports array to find and process linked component KIR files.
 * Transitive imports are processed recursively.
 * Each KIR file is transformed to Lua using lua_codegen_from_json().
 *
 * KIR Architecture:
 * - main.kir contains the main module's KIR representation
 * - Each import in main.kir references a component: "components/calendar" -> "components/calendar.kir"
 * - All codegen output is generated FROM the KIR representation, never copied from source
 */
bool lua_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: Invalid arguments to lua_codegen_generate_multi\n");
        return false;
    }

    // Set error prefix for this codegen
    codegen_set_error_prefix("Lua");

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
    struct stat st = {0};
    if (stat(output_dir, &st) == -1) {
        char mkdir_cmd[2048];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
        if (system(mkdir_cmd) != 0) {
            fprintf(stderr, "Error: Could not create output directory: %s\n", output_dir);
            cJSON_Delete(main_root);
            free(main_kir_json);
            return false;
        }
    }

    int files_written = 0;

    // 1. Generate main.lua from main.kir
    char* main_lua = lua_codegen_from_json(main_kir_json);
    free(main_kir_json);

    if (main_lua) {
        char main_output_path[2048];
        snprintf(main_output_path, sizeof(main_output_path), "%s/main.lua", output_dir);

        if (write_file_with_mkdir(main_output_path, main_lua)) {
            printf("✓ Generated: main.lua\n");
            files_written++;
        }
        free(main_lua);
    } else {
        fprintf(stderr, "Warning: Failed to generate main.lua from KIR\n");
    }

    // 2. Get the KIR directory (parent of kir_path)
    char kir_dir[2048];
    get_parent_dir(kir_path, kir_dir, sizeof(kir_dir));

    // 3. Track processed modules to avoid duplicates
    ProcessedModules processed = {0};
    mark_module_processed(&processed, "main");  // Mark main as processed

    // 4. Process each import recursively (including transitive imports)
    cJSON* imports = cJSON_GetObjectItem(main_root, "imports");
    if (imports && cJSON_IsArray(imports)) {
        cJSON* import_item = NULL;
        cJSON_ArrayForEach(import_item, imports) {
            if (!cJSON_IsString(import_item)) continue;

            const char* module_id = cJSON_GetStringValue(import_item);
            if (module_id) {
                files_written += process_module_recursive(module_id, kir_dir,
                                                          output_dir, &processed);
            }
        }
    }

    free_processed_modules(&processed);
    cJSON_Delete(main_root);

    if (files_written == 0) {
        fprintf(stderr, "Warning: No Lua files were generated\n");
        return false;
    }

    printf("✓ Generated %d Lua files in %s\n", files_written, output_dir);
    return true;
}
