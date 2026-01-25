/**
 * C Code Generator - Component Tree Generation
 *
 * Functions for generating C code from KIR component trees.
 */

#include "ir_c_components.h"
#include "ir_c_output.h"
#include "ir_c_types.h"
#include "ir_c_reactive.h"
#include "ir_c_expression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Internal aliases for shorter function names
#define write_indent(ctx) c_write_indent(ctx)
#define writeln(ctx, str) c_writeln(ctx, str)
#define write_raw(ctx, str) c_write_raw(ctx, str)
#define is_reactive_variable(ctx, name) c_is_reactive_variable(ctx, name)
#define get_scoped_var_name(ctx, name) c_get_scoped_var_name(ctx, name)
#define generate_scoped_var_name(name, scope) c_generate_scoped_var_name(name, scope)
#define expr_to_c(expr) c_expr_to_c(expr)
// Note: get_component_macro and kir_type_to_c are from ir_c_types.h (no c_ prefix)

// Get variable name for a component ID (internal utility)
static const char* get_variable_for_component_id(CCodegenContext* ctx, int component_id) {
    if (!ctx->variables) return NULL;

    int var_count = cJSON_GetArraySize(ctx->variables);
    for (int i = 0; i < var_count; i++) {
        cJSON* var = cJSON_GetArrayItem(ctx->variables, i);
        cJSON* id_item = cJSON_GetObjectItem(var, "component_id");
        if (id_item && id_item->valueint == component_id) {
            cJSON* name = cJSON_GetObjectItem(var, "name");
            if (name && name->valuestring) {
                return name->valuestring;
            }
        }
    }
    return NULL;
}

// Forward declaration
static bool generate_property_macro(CCodegenContext* ctx, const char* key, cJSON* value, bool* first_prop);

void c_generate_component_recursive(CCodegenContext* ctx, cJSON* component, bool is_root) {
    cJSON* type_obj = cJSON_GetObjectItem(component, "type");
    cJSON* id_obj = cJSON_GetObjectItem(component, "id");
    cJSON* children_obj = cJSON_GetObjectItem(component, "children");
    cJSON* text_obj = cJSON_GetObjectItem(component, "text");
    cJSON* scope_obj = cJSON_GetObjectItem(component, "scope");

    if (!type_obj || !type_obj->valuestring) return;

    const char* type = type_obj->valuestring;

    // Save and set current scope for scoped components
    const char* prev_scope = ctx->current_scope;
    if (scope_obj && scope_obj->valuestring) {
        ctx->current_scope = scope_obj->valuestring;
    }

    // Special handling for For loops - emit FOR_EACH macro
    if (strcmp(type, "For") == 0) {
        cJSON* for_def = cJSON_GetObjectItem(component, "for_def");
        if (for_def) {
            cJSON* item_name = cJSON_GetObjectItem(for_def, "item_name");
            cJSON* source = cJSON_GetObjectItem(for_def, "source");
            cJSON* source_expr = source ? cJSON_GetObjectItem(source, "expression") : NULL;

            const char* item_var = item_name ? item_name->valuestring : "item";
            const char* source_var = source_expr ? source_expr->valuestring : "items";

            // Generate FOR_EACH macro call
            write_indent(ctx);
            fprintf(ctx->output, "FOR_EACH(%s, %s, %s_count,\n",
                    item_var, source_var, source_var);

            ctx->indent_level++;

            // Generate template children as the body
            if (children_obj && cJSON_IsArray(children_obj)) {
                int child_count = cJSON_GetArraySize(children_obj);
                for (int i = 0; i < child_count; i++) {
                    cJSON* child = cJSON_GetArrayItem(children_obj, i);
                    c_generate_component_recursive(ctx, child, false);
                    if (i < child_count - 1) {
                        fprintf(ctx->output, ",\n");
                    }
                }
            }

            ctx->indent_level--;
            fprintf(ctx->output, "\n");
            write_indent(ctx);
            fprintf(ctx->output, ")");
            return;
        }
    }

    // Special handling for Custom components (component references)
    cJSON* component_ref = cJSON_GetObjectItem(component, "component_ref");
    if (component_ref && component_ref->valuestring) {
        write_indent(ctx);
        cJSON* arg_obj = cJSON_GetObjectItem(component, "arg");
        if (arg_obj && arg_obj->valuestring) {
            fprintf(ctx->output, "%s(%s)", component_ref->valuestring, arg_obj->valuestring);
        } else {
            fprintf(ctx->output, "%s()", component_ref->valuestring);
        }
        return;
    }

    // Check if this is a custom component (type is PascalCase and not a builtin)
    // Custom components have their type set to the component name (e.g., "HabitPanel")
    const char* macro_check = get_component_macro(type);
    bool is_custom_component = (strcmp(macro_check, "CONTAINER") == 0 &&
                                type[0] >= 'A' && type[0] <= 'Z' &&
                                strcmp(type, "Container") != 0);
    if (is_custom_component) {
        write_indent(ctx);
        cJSON* arg_obj = cJSON_GetObjectItem(component, "arg");
        if (arg_obj && arg_obj->valuestring) {
            fprintf(ctx->output, "%s(%s)", type, arg_obj->valuestring);
        } else {
            fprintf(ctx->output, "%s()", type);
        }
        return;
    }

    const char* macro = get_component_macro(type);

    // Check if this component has a variable assignment
    const char* var_name = NULL;
    if (id_obj) {
        var_name = get_variable_for_component_id(ctx, id_obj->valueint);
    }

    // Write indentation
    write_indent(ctx);

    // Variable assignment if needed
    if (var_name && !is_root) {
        fprintf(ctx->output, "%s = ", var_name);
    }

    // Component macro
    fprintf(ctx->output, "%s(", macro);

    // Note: BIND_TEXT for reactive text should be handled specially
    // It should be output immediately after the TEXT component, not as a separate child

    // Output text parameter
    cJSON* text_expr_obj = cJSON_GetObjectItem(component, "text_expression");

    // Check for reactive text binding via property_bindings
    cJSON* property_bindings = cJSON_GetObjectItem(component, "property_bindings");
    bool has_property_binding_bind_text = false;

    if (property_bindings) {
        cJSON* text_binding = cJSON_GetObjectItem(property_bindings, "text");
        if (text_binding) {
            cJSON* binding_type = cJSON_GetObjectItem(text_binding, "binding_type");
            if (binding_type && binding_type->valuestring &&
                strcmp(binding_type->valuestring, "static_template") == 0) {
                has_property_binding_bind_text = true;
            }
        }
    }

    // Fallback: check if text_expression references a reactive variable
    // (property_bindings may not be present in expanded component instances)
    bool has_fallback_bind_text = !has_property_binding_bind_text &&
                                     text_expr_obj && text_expr_obj->valuestring &&
                                     is_reactive_variable(ctx, text_expr_obj->valuestring);

    // Output text parameter
    bool has_text = (text_obj && text_obj->valuestring) || (text_expr_obj && text_expr_obj->valuestring);

    if (has_property_binding_bind_text || has_fallback_bind_text) {
        // Reactive binding - use empty string, actual value comes from signal
        fprintf(ctx->output, "\"\"");
    } else if (text_obj && text_obj->valuestring) {
        fprintf(ctx->output, "\"%s\"", text_obj->valuestring);
    } else if (text_expr_obj && text_expr_obj->valuestring) {
        fprintf(ctx->output, "\"\"");
    }

    // Special handling for Checkbox: output 'checked' parameter after label
    if (strcmp(type, "Checkbox") == 0) {
        cJSON* checked_obj = cJSON_GetObjectItem(component, "checked");
        bool checked_val = checked_obj && cJSON_IsTrue(checked_obj);
        fprintf(ctx->output, ", %s", checked_val ? "true" : "false");
    }

    // Special handling for Dropdown/Input: output 'placeholder' parameter
    if (strcmp(type, "Dropdown") == 0 || strcmp(type, "Input") == 0) {
        // For Dropdown/Input with no text, output placeholder as the first argument
        if (!has_text) {
            cJSON* placeholder_obj = cJSON_GetObjectItem(component, "placeholder");
            const char* placeholder_val = placeholder_obj && placeholder_obj->valuestring ?
                                          placeholder_obj->valuestring : "";
            fprintf(ctx->output, "\"%s\"", placeholder_val);
        }
    }

    // Check for FULL_SIZE pattern (width=100.0px AND height=100.0px)
    cJSON* width_prop = cJSON_GetObjectItem(component, "width");
    cJSON* height_prop = cJSON_GetObjectItem(component, "height");
    bool is_full_size = (width_prop && width_prop->valuestring &&
                         height_prop && height_prop->valuestring &&
                         (strcmp(width_prop->valuestring, "100.0px") == 0 ||
                          strcmp(width_prop->valuestring, "100.0%") == 0) &&
                         (strcmp(height_prop->valuestring, "100.0px") == 0 ||
                          strcmp(height_prop->valuestring, "100.0%") == 0));

    // Check if we have properties or children
    bool has_properties = false;
    cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, component) {
        const char* key = prop->string;
        if (!key) continue;
        if (strcmp(key, "id") != 0 && strcmp(key, "type") != 0 &&
            strcmp(key, "text") != 0 && strcmp(key, "children") != 0 &&
            strcmp(key, "TEST_MARKER") != 0 && strcmp(key, "direction") != 0 &&
            strcmp(key, "background") != 0 && strcmp(key, "color") != 0 &&
            strcmp(key, "placeholder") != 0 &&  // Skip placeholder for Dropdown/Input
            strcmp(key, "checked") != 0) {     // Skip checked for Checkbox
            has_properties = true;
            break;
        }
        // Check if background/color are non-default
        if (strcmp(key, "background") == 0 && prop->valuestring && strcmp(prop->valuestring, "#00000000") != 0) {
            has_properties = true;
            break;
        }
        if (strcmp(key, "color") == 0 && prop->valuestring && strcmp(prop->valuestring, "#00000000") != 0) {
            has_properties = true;
            break;
        }
    }

    bool has_children = (children_obj && cJSON_GetArraySize(children_obj) > 0);

    // Check if this component has a placeholder argument (Dropdown/Input)
    // For Dropdown/Input, we always output a placeholder (even if empty), so check based on type and !has_text
    bool has_placeholder = (strcmp(type, "Dropdown") == 0 || strcmp(type, "Input") == 0) &&
                          !has_text;

    // Add comma after text/placeholder if there are properties or children
    if ((has_text || has_placeholder) && (has_properties || has_children)) {
        fprintf(ctx->output, ",\n");
    } else if (!has_text && !has_placeholder) {
        fprintf(ctx->output, "\n");
    } else if (!has_properties && !has_children) {
        fprintf(ctx->output, "\n");
    }

    ctx->indent_level++;

    // Generate FULL_SIZE if applicable
    bool first_prop = true;
    if (is_full_size) {
        if (!first_prop) {
            fprintf(ctx->output, ",\n");
        }
        write_indent(ctx);
        fprintf(ctx->output, "FULL_SIZE");
        first_prop = false;
    }

    // Special handling for dropdown_state (Dropdown components)
    cJSON* dropdown_state = cJSON_GetObjectItem(component, "dropdown_state");
    if (dropdown_state && cJSON_IsObject(dropdown_state) && strcmp(type, "Dropdown") == 0) {
        // Handle options array
        cJSON* options_array = cJSON_GetObjectItem(dropdown_state, "options");
        if (options_array && cJSON_IsArray(options_array)) {
            int count = cJSON_GetArraySize(options_array);
            if (count > 0) {
                if (!first_prop) fprintf(ctx->output, ",\n");
                write_indent(ctx);

                // Generate OPTIONS macro call with array elements
                fprintf(ctx->output, "OPTIONS(%d", count);
                for (int i = 0; i < count; i++) {
                    cJSON* item = cJSON_GetArrayItem(options_array, i);
                    if (item && cJSON_IsString(item)) {
                        fprintf(ctx->output, ", \"%s\"", item->valuestring);
                    }
                }
                fprintf(ctx->output, ")");
                first_prop = false;
            }
        }

        // Handle selectedIndex
        cJSON* selected_index = cJSON_GetObjectItem(dropdown_state, "selectedIndex");
        if (selected_index && cJSON_IsNumber(selected_index)) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "SELECTED_INDEX(%d)", selected_index->valueint);
            first_prop = false;
        }
    }

    // Special handling for TEXT components with reactive text_expression
    // Generate BIND_TEXT_EXPR as a property for fallback case (no property_bindings)
    if (strcmp(type, "Text") == 0) {
        cJSON* text_expr_obj = cJSON_GetObjectItem(component, "text_expression");
        if (text_expr_obj && text_expr_obj->valuestring && is_reactive_variable(ctx, text_expr_obj->valuestring)) {
            // Check that there's no property_bindings (which would be handled elsewhere)
            cJSON* property_bindings = cJSON_GetObjectItem(component, "property_bindings");
            bool has_property_binding = false;
            if (property_bindings) {
                cJSON* text_binding = cJSON_GetObjectItem(property_bindings, "text");
                if (text_binding) {
                    cJSON* binding_type = cJSON_GetObjectItem(text_binding, "binding_type");
                    if (binding_type && binding_type->valuestring &&
                        strcmp(binding_type->valuestring, "static_template") == 0) {
                        has_property_binding = true;
                    }
                }
            }

            // Only generate BIND_TEXT_EXPR for fallback case (no property_bindings)
            if (!has_property_binding) {
                if (!first_prop) fprintf(ctx->output, ",\n");
                write_indent(ctx);
                char* scoped_name = get_scoped_var_name(ctx, text_expr_obj->valuestring);
                char signal_name[256];
                snprintf(signal_name, sizeof(signal_name), "%s_signal", scoped_name);
                fprintf(ctx->output, "BIND_TEXT_EXPR(%s)", signal_name);
                free(scoped_name);
                first_prop = false;
            }
        }
    }

    // Generate properties
    cJSON* prop2 = NULL;
    cJSON_ArrayForEach(prop2, component) {
        const char* key = prop2->string;
        if (!key) continue;

        // Skip internal fields
        if (strcmp(key, "id") == 0 || strcmp(key, "type") == 0 ||
            strcmp(key, "text") == 0 || strcmp(key, "children") == 0 ||
            strcmp(key, "TEST_MARKER") == 0 || strcmp(key, "direction") == 0 ||
            strcmp(key, "placeholder") == 0 ||  // Skip placeholder for Dropdown/Input (handled as argument)
            strcmp(key, "checked") == 0 ||      // Skip checked for Checkbox (handled as argument)
            strcmp(key, "dropdown_state") == 0) { // Skip dropdown_state (handled separately below)
            continue;
        }

        // Skip width/height if we already generated FULL_SIZE
        if (is_full_size && (strcmp(key, "width") == 0 || strcmp(key, "height") == 0)) {
            continue;
        }

        // Skip transparent/default colors
        if ((strcmp(key, "background") == 0 || strcmp(key, "color") == 0) &&
            prop2->valuestring && strcmp(prop2->valuestring, "#00000000") == 0) {
            continue;
        }

        // Skip events property if there are no event handlers in metadata OR logic_block
        // KRY handlers are in logic_block.functions, C handlers are in c_metadata.event_handlers
        if (strcmp(key, "events") == 0) {
            bool has_kry_handlers = false;
            // Check if we have logic_block with functions (KRY handlers)
            cJSON* logic_block = cJSON_GetObjectItem(ctx->root_json, "logic_block");
            if (logic_block) {
                cJSON* functions = cJSON_GetObjectItem(logic_block, "functions");
                if (functions && cJSON_IsArray(functions) && cJSON_GetArraySize(functions) > 0) {
                    has_kry_handlers = true;
                }
            }
            bool has_c_handlers = (ctx->event_handlers && cJSON_GetArraySize(ctx->event_handlers) > 0);
            if (!has_kry_handlers && !has_c_handlers) {
                continue;
            }
        }

        // Generate the property (comma handling is internal now)
        c_generate_property_macro(ctx, key, prop2, &first_prop);
    }

    // Generate children
    if (children_obj && cJSON_IsArray(children_obj)) {
        int child_count = cJSON_GetArraySize(children_obj);
        if (child_count > 0) {
            // Print comma before children if we had properties
            if (!first_prop) {
                fprintf(ctx->output, ",\n");
                fprintf(ctx->output, "\n");  // Blank line for readability
            }

            for (int i = 0; i < child_count; i++) {
                cJSON* child = cJSON_GetArrayItem(children_obj, i);
                c_generate_component_recursive(ctx, child, false);
                if (i < child_count - 1) {
                    fprintf(ctx->output, ",\n");
                } else {
                    fprintf(ctx->output, "\n");
                }
            }
        }
    }

    ctx->indent_level--;
    write_indent(ctx);
    fprintf(ctx->output, ")");

    // Restore previous scope
    ctx->current_scope = prev_scope;
}
bool c_generate_property_macro(CCodegenContext* ctx, const char* key, cJSON* value, bool* first_prop) {
    // NOTE: Each case that generates output must:
    // 1. Print comma if not first_prop
    // 2. Set *first_prop = false after printing

    // Width/Height
    if (strcmp(key, "width") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (strcmp(value->valuestring, "100.0px") == 0 || strcmp(value->valuestring, "100.0%") == 0) {
            fprintf(ctx->output, "FULL_WIDTH");
        } else {
            // Parse numeric value (e.g., "200.0px" → 200)
            int width_val = 0;
            if (sscanf(value->valuestring, "%d", &width_val) == 1 && width_val > 0) {
                fprintf(ctx->output, "WIDTH(%d)", width_val);
            } else {
                fprintf(ctx->output, "WIDTH(\"%s\")", value->valuestring);
            }
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "height") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (strcmp(value->valuestring, "100.0px") == 0 || strcmp(value->valuestring, "100.0%") == 0) {
            fprintf(ctx->output, "FULL_HEIGHT");
        } else {
            // Parse numeric value (e.g., "60.0px" → 60)
            int height_val = 0;
            if (sscanf(value->valuestring, "%d", &height_val) == 1 && height_val > 0) {
                fprintf(ctx->output, "HEIGHT(%d)", height_val);
            } else {
                fprintf(ctx->output, "HEIGHT(\"%s\")", value->valuestring);
            }
        }
        *first_prop = false;
        return true;
    }

    // Min/Max Width
    if (strcmp(key, "minWidth") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MIN_WIDTH(%d)", val);
        } else {
            fprintf(ctx->output, "MIN_WIDTH(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "maxWidth") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MAX_WIDTH(%d)", val);
        } else {
            fprintf(ctx->output, "MAX_WIDTH(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }
    // Min/Max Height
    if (strcmp(key, "minHeight") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MIN_HEIGHT(%d)", val);
        } else {
            fprintf(ctx->output, "MIN_HEIGHT(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "maxHeight") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MAX_HEIGHT(%d)", val);
        } else {
            fprintf(ctx->output, "MAX_HEIGHT(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }

    // Colors
    if (strcmp(key, "background") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        // Convert color string "#RRGGBB" to hex 0xRRGGBB
        const char* color_str = value->valuestring;
        if (color_str[0] == '#' && strlen(color_str) >= 7) {
            fprintf(ctx->output, "BG_COLOR(0x%s)", color_str + 1);  // Skip '#'
        } else {
            // Fallback for non-hex colors
            fprintf(ctx->output, "BG_COLOR(0x%s)", color_str);
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "color") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        // Check for named colors
        if (strcmp(value->valuestring, "#ffffff") == 0) {
            fprintf(ctx->output, "COLOR_WHITE");
        } else if (strcmp(value->valuestring, "#000000") == 0) {
            fprintf(ctx->output, "COLOR_BLACK");
        } else if (strcmp(value->valuestring, "#ff0000") == 0) {
            fprintf(ctx->output, "COLOR_RED");
        } else if (strcmp(value->valuestring, "#00ff00") == 0) {
            fprintf(ctx->output, "COLOR_GREEN");
        } else if (strcmp(value->valuestring, "#0000ff") == 0) {
            fprintf(ctx->output, "COLOR_BLUE");
        } else if (strcmp(value->valuestring, "#ffff00") == 0) {
            fprintf(ctx->output, "COLOR_YELLOW");
        } else if (strcmp(value->valuestring, "#00ffff") == 0) {
            fprintf(ctx->output, "COLOR_CYAN");
        } else if (strcmp(value->valuestring, "#ff00ff") == 0) {
            fprintf(ctx->output, "COLOR_MAGENTA");
        } else if (strcmp(value->valuestring, "#808080") == 0) {
            fprintf(ctx->output, "COLOR_GRAY");
        } else if (strcmp(value->valuestring, "#ffa500") == 0) {
            fprintf(ctx->output, "COLOR_ORANGE");
        } else if (strcmp(value->valuestring, "#800080") == 0) {
            fprintf(ctx->output, "COLOR_PURPLE");
        } else {
            // Convert color string "#RRGGBB" to hex 0xRRGGBB
            const char* color_str = value->valuestring;
            if (color_str[0] == '#' && strlen(color_str) >= 7) {
                fprintf(ctx->output, "TEXT_COLOR(0x%s)", color_str + 1);  // Skip '#'
            } else {
                fprintf(ctx->output, "TEXT_COLOR(0x%s)", color_str);
            }
        }
        *first_prop = false;
        return true;
    }

    // Padding/Margin
    if (strcmp(key, "padding") == 0) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (cJSON_IsNumber(value)) {
            fprintf(ctx->output, "PADDING(%.0f)", value->valuedouble);
        } else if (cJSON_IsArray(value)) {
            int size = cJSON_GetArraySize(value);
            if (size == 4) {
                fprintf(ctx->output, "PADDING_SIDES(%.0f, %.0f, %.0f, %.0f)",
                        cJSON_GetArrayItem(value, 0)->valuedouble,
                        cJSON_GetArrayItem(value, 1)->valuedouble,
                        cJSON_GetArrayItem(value, 2)->valuedouble,
                        cJSON_GetArrayItem(value, 3)->valuedouble);
            }
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "gap") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "GAP(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }

    // Typography
    if (strcmp(key, "fontSize") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "FONT_SIZE(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "fontBold") == 0 && cJSON_IsBool(value)) {
        if (cJSON_IsTrue(value)) {
            if (!*first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "FONT_BOLD");
            *first_prop = false;
            return true;
        }
        return false;  // fontBold=false generates no output, so no comma
    }

    // Layout
    if (strcmp(key, "justifyContent") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (strcmp(value->valuestring, "center") == 0) {
            fprintf(ctx->output, "JUSTIFY_CENTER");
        } else if (strcmp(value->valuestring, "flex-start") == 0 || strcmp(value->valuestring, "start") == 0) {
            fprintf(ctx->output, "JUSTIFY_START");
        } else if (strcmp(value->valuestring, "flex-end") == 0 || strcmp(value->valuestring, "end") == 0) {
            fprintf(ctx->output, "JUSTIFY_END");
        } else if (strcmp(value->valuestring, "space-between") == 0) {
            fprintf(ctx->output, "JUSTIFY_SPACE_BETWEEN");
        } else if (strcmp(value->valuestring, "space-around") == 0) {
            fprintf(ctx->output, "JUSTIFY_SPACE_AROUND");
        } else {
            fprintf(ctx->output, "JUSTIFY_CENTER");
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "alignItems") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (strcmp(value->valuestring, "center") == 0) {
            fprintf(ctx->output, "ALIGN_CENTER");
        } else if (strcmp(value->valuestring, "flex-start") == 0 || strcmp(value->valuestring, "start") == 0) {
            fprintf(ctx->output, "ALIGN_START");
        } else if (strcmp(value->valuestring, "flex-end") == 0 || strcmp(value->valuestring, "end") == 0) {
            fprintf(ctx->output, "ALIGN_END");
        } else if (strcmp(value->valuestring, "stretch") == 0) {
            fprintf(ctx->output, "ALIGN_STRETCH");
        } else {
            fprintf(ctx->output, "ALIGN_CENTER");
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "flexShrink") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "FLEX_SHRINK(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }

    // Visibility
    if (strcmp(key, "visible") == 0 && cJSON_IsBool(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "VISIBLE(%s)", cJSON_IsTrue(value) ? "true" : "false");
        *first_prop = false;
        return true;
    }

    // Events
    if (strcmp(key, "events") == 0 && cJSON_IsArray(value)) {
        int event_count = cJSON_GetArraySize(value);
        bool printed = false;
        for (int i = 0; i < event_count; i++) {
            cJSON* event = cJSON_GetArrayItem(value, i);
            cJSON* event_type = cJSON_GetObjectItem(event, "type");
            cJSON* logic_id = cJSON_GetObjectItem(event, "logic_id");

            if (event_type && event_type->valuestring && logic_id && logic_id->valuestring) {
                // For KRY handlers, logic_id is the function name directly (e.g., "handler_1_click")
                // For C metadata handlers, we need to look up the function name
                const char* func_name = logic_id->valuestring;  // Default: use logic_id as function name
                bool is_c_handler = false;  // Track if this is a C metadata handler

                // Try to find function name in c_metadata.event_handlers (legacy C handlers)
                if (ctx->event_handlers) {
                    int handler_count = cJSON_GetArraySize(ctx->event_handlers);
                    for (int j = 0; j < handler_count; j++) {
                        cJSON* handler = cJSON_GetArrayItem(ctx->event_handlers, j);
                        cJSON* h_logic_id = cJSON_GetObjectItem(handler, "logic_id");
                        cJSON* h_func_name = cJSON_GetObjectItem(handler, "function_name");

                        if (h_logic_id && h_logic_id->valuestring &&
                            h_func_name && h_func_name->valuestring &&
                            strcmp(h_logic_id->valuestring, logic_id->valuestring) == 0) {
                            func_name = h_func_name->valuestring;
                            is_c_handler = true;  // This is a C metadata handler
                            break;
                        }
                    }
                }

                // For KRY handlers, scope the function name based on current_scope
                // C handlers are already defined by the user, so don't scope them
                char* scoped_func_name = NULL;
                if (!is_c_handler && ctx->current_scope && strcmp(ctx->current_scope, "component") != 0) {
                    // Generate scoped handler name (e.g., "handler_1_click_Counter_0")
                    scoped_func_name = generate_scoped_var_name(func_name, ctx->current_scope);
                    func_name = scoped_func_name;
                }

                if (strcmp(event_type->valuestring, "click") == 0) {
                    if (!*first_prop) fprintf(ctx->output, ",\n");
                    write_indent(ctx);
                    fprintf(ctx->output, "ON_CLICK(%s)", func_name);
                    *first_prop = false;
                    printed = true;
                }

                if (scoped_func_name) {
                    free(scoped_func_name);
                }
            }
        }
        return printed;
    }

    // Dropdown options array
    if (strcmp(key, "options") == 0 && cJSON_IsArray(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);

        // Generate OPTIONS macro call with array elements
        int count = cJSON_GetArraySize(value);
        fprintf(ctx->output, "OPTIONS(%d", count);

        // Output each option string
        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(value, i);
            if (item && cJSON_IsString(item)) {
                fprintf(ctx->output, ", \"%s\"", item->valuestring);
            }
        }
        fprintf(ctx->output, ")");

        *first_prop = false;
        return true;
    }

    // Handle reactive property bindings
    if (strcmp(key, "property_bindings") == 0 && cJSON_IsObject(value)) {
        bool printed = false;
        cJSON* binding = NULL;

        cJSON_ArrayForEach(binding, value) {
            const char* prop_name = binding->string;
            if (!prop_name) continue;

            cJSON* source_expr = cJSON_GetObjectItem(binding, "source_expr");
            cJSON* binding_type = cJSON_GetObjectItem(binding, "binding_type");

            if (source_expr && source_expr->valuestring &&
                binding_type && binding_type->valuestring &&
                strcmp(binding_type->valuestring, "static_template") == 0) {

                if (!*first_prop) fprintf(ctx->output, ",\n");
                write_indent(ctx);

                // Get scoped variable name for the current scope
                char* scoped_name = get_scoped_var_name(ctx, source_expr->valuestring);
                char signal_name[256];
                snprintf(signal_name, sizeof(signal_name), "%s_signal", scoped_name);

                // Generate binding macro call
                if (strcmp(prop_name, "text") == 0) {
                    fprintf(ctx->output, "BIND_TEXT(%s)", signal_name);
                } else if (strcmp(prop_name, "visible") == 0) {
                    fprintf(ctx->output, "BIND_VISIBLE(%s)", signal_name);
                } else if (strcmp(prop_name, "background") == 0) {
                    fprintf(ctx->output, "BIND_BACKGROUND(%s)", signal_name);
                } else if (strcmp(prop_name, "color") == 0) {
                    fprintf(ctx->output, "BIND_COLOR(%s)", signal_name);
                } else if (strcmp(prop_name, "selectedIndex") == 0) {
                    fprintf(ctx->output, "SELECTED_INDEX(%s)", source_expr->valuestring);
                } else {
                    fprintf(ctx->output, "BIND(%s, %s)", prop_name, scoped_name);
                }

                free(scoped_name);
                *first_prop = false;
                printed = true;
            }
        }
        return printed;
    }

    // Fallback: skip unknown properties (no comma printed)
    return false;
}
void c_generate_component_definitions(CCodegenContext* ctx, cJSON* component_defs) {
    if (!component_defs || !cJSON_IsArray(component_defs)) return;

    cJSON* def;
    cJSON_ArrayForEach(def, component_defs) {
        cJSON* name = cJSON_GetObjectItem(def, "name");
        cJSON* props = cJSON_GetObjectItem(def, "props");
        cJSON* template = cJSON_GetObjectItem(def, "template");

        if (!name || !cJSON_IsString(name)) continue;

        const char* comp_name = cJSON_GetStringValue(name);

        // Generate component function signature
        fprintf(ctx->output, "/**\n");
        fprintf(ctx->output, " * Component: %s\n", comp_name);

        // Document props
        if (props && cJSON_IsArray(props)) {
            cJSON* prop;
            cJSON_ArrayForEach(prop, props) {
                cJSON* prop_name = cJSON_GetObjectItem(prop, "name");
                cJSON* prop_type = cJSON_GetObjectItem(prop, "type");
                if (prop_name && cJSON_IsString(prop_name)) {
                    fprintf(ctx->output, " * @param %s %s\n",
                            cJSON_GetStringValue(prop_name),
                            prop_type && cJSON_IsString(prop_type) ?
                            cJSON_GetStringValue(prop_type) : "any");
                }
            }
        }
        fprintf(ctx->output, " */\n");

        // Generate KRYON_COMPONENT macro call
        fprintf(ctx->output, "KRYON_COMPONENT(%s", comp_name);

        // Add props as macro parameters
        if (props && cJSON_IsArray(props)) {
            cJSON* prop;
            cJSON_ArrayForEach(prop, props) {
                cJSON* prop_name = cJSON_GetObjectItem(prop, "name");
                if (prop_name && cJSON_IsString(prop_name)) {
                    fprintf(ctx->output, ", %s", cJSON_GetStringValue(prop_name));
                }
            }
        }
        fprintf(ctx->output, ",\n");

        // Generate template content
        ctx->indent_level++;
        if (template) {
            c_generate_component_recursive(ctx, template, true);
        }
        ctx->indent_level--;

        fprintf(ctx->output, "\n);\n\n");
    }
}

/**
 * Generate C struct definitions from source_structures.struct_types
 */

// Internal wrapper for forward declaration
static bool generate_property_macro(CCodegenContext* ctx, const char* key, cJSON* value, bool* first_prop) {
    return c_generate_property_macro(ctx, key, value, first_prop);
}
