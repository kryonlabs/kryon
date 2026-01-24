/**
 * Hare Code Generator
 * Generates Hare source code from KIR JSON
 */

#define _POSIX_C_SOURCE 200809L

#include "hare_codegen.h"
#include "../codegen_common.h"
#include "../../ir/src/utils/ir_string_builder.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

// ============================================================================
// Code Generator State
// ============================================================================

typedef struct {
    IRStringBuilder* sb;
    int indent;
} HareCodeGen;

// String builder context for JSON to Hare conversion
typedef struct {
    char* result;
    size_t result_size;
    size_t result_capacity;
} HareStringBuilder;

static HareCodeGen* hare_gen_create(void) {
    HareCodeGen* gen = calloc(1, sizeof(HareCodeGen));
    if (!gen) return NULL;

    gen->sb = ir_sb_create(8192);
    if (!gen->sb) {
        free(gen);
        return NULL;
    }
    gen->indent = 0;
    return gen;
}

static void hare_gen_free(HareCodeGen* gen) {
    if (gen) {
        ir_sb_free(gen->sb);
        free(gen);
    }
}

static void hare_gen_add_line(HareCodeGen* gen, const char* line) {
    if (!gen || !gen->sb) return;
    ir_sb_indent(gen->sb, gen->indent);
    ir_sb_append_line(gen->sb, line);
}

static void hare_gen_add_line_fmt(HareCodeGen* gen, const char* fmt, ...) {
    if (!gen || !gen->sb || !fmt) return;

    va_list args;
    va_start(args, fmt);
    char temp[4096];
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    ir_sb_indent(gen->sb, gen->indent);
    ir_sb_append_line(gen->sb, temp);
}

// ============================================================================
// Hare String Builder Helpers
// ============================================================================

static HareStringBuilder* hare_sb_create(size_t initial_capacity) {
    HareStringBuilder* sb = calloc(1, sizeof(HareStringBuilder));
    if (!sb) return NULL;

    sb->result = malloc(initial_capacity);
    if (!sb->result) {
        free(sb);
        return NULL;
    }

    sb->result_size = 0;
    sb->result_capacity = initial_capacity;
    sb->result[0] = '\0';
    return sb;
}

static void hare_sb_free(HareStringBuilder* sb) {
    if (sb) {
        free(sb->result);
        free(sb);
    }
}

static int hare_sb_append_str(HareStringBuilder* sb, const char* str) {
    if (!sb || !str) return -1;

    size_t len = strlen(str);
    while (sb->result_size + len >= sb->result_capacity) {
        sb->result_capacity *= 2;
        char* new_result = realloc(sb->result, sb->result_capacity);
        if (!new_result) {
            return -1;
        }
        sb->result = new_result;
    }
    strcpy(sb->result + sb->result_size, str);
    sb->result_size += len;
    return 0;
}

static int hare_sb_append_char(HareStringBuilder* sb, char c) {
    char str[2] = {c, '\0'};
    return hare_sb_append_str(sb, str);
}

static char* hare_sb_build(HareStringBuilder* sb) {
    if (!sb) return NULL;
    hare_sb_append_char(sb, '\0');
    char* result = sb->result;
    sb->result = NULL;
    sb->result_size = 0;
    sb->result_capacity = 0;
    return result;
}

// ============================================================================
// Hare Code Generation
// ============================================================================

static char* cJSON_to_hare_syntax(const cJSON* item);

// Forward declarations
static void generate_component_tree(HareCodeGen* gen, cJSON* comp, const char* var_name, bool is_inline);

/**
 * Convert cJSON to Hare syntax
 */
static char* cJSON_to_hare_syntax(const cJSON* item) {
    if (!item) return strdup("void");

    HareStringBuilder* sb = hare_sb_create(256);
    if (!sb) return NULL;

    switch (cJSON_IsInvalid(item) ? 100 : item->type) {
        case cJSON_NULL:
            hare_sb_append_str(sb, "void");
            break;

        case cJSON_False:
        case cJSON_True:
            hare_sb_append_str(sb, cJSON_IsTrue(item) ? "true" : "false");
            break;

        case cJSON_Number:
            if (item->valuedouble == (double)(int)item->valuedouble) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%d", (int)item->valuedouble);
                hare_sb_append_str(sb, buf);
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", item->valuedouble);
                hare_sb_append_str(sb, buf);
            }
            break;

        case cJSON_String: {
            const char* str = cJSON_GetStringValue(item);
            if (!str) {
                hare_sb_append_str(sb, "\"\"");
                break;
            }

            hare_sb_append_char(sb, '"');
            for (size_t i = 0; str[i]; i++) {
                if (str[i] == '"' || str[i] == '\\') {
                    hare_sb_append_char(sb, '\\');
                }
                hare_sb_append_char(sb, str[i]);
            }
            hare_sb_append_char(sb, '"');
            break;
        }

        case cJSON_Array: {
            hare_sb_append_char(sb, '[');
            const cJSON* child = item->child;
            bool first = true;
            while (child) {
                if (!first) hare_sb_append_str(sb, ", ");
                first = false;
                char* child_hare = cJSON_to_hare_syntax(child);
                if (child_hare) {
                    hare_sb_append_str(sb, child_hare);
                    free(child_hare);
                }
                child = child->next;
            }
            hare_sb_append_char(sb, ']');
            break;
        }

        case cJSON_Object: {
            hare_sb_append_char(sb, '{');
            const cJSON* child = item->child;
            bool first = true;
            while (child) {
                if (!first) hare_sb_append_str(sb, ", ");
                first = false;

                if (child->string && child->string[0]) {
                    hare_sb_append_str(sb, child->string);
                }

                hare_sb_append_str(sb, " = ");

                char* child_hare = cJSON_to_hare_syntax(child);
                if (child_hare) {
                    hare_sb_append_str(sb, child_hare);
                    free(child_hare);
                }
                child = child->next;
            }
            hare_sb_append_char(sb, '}');
            break;
        }

        default:
            hare_sb_append_str(sb, "void");
            break;
    }

    char* result = hare_sb_build(sb);
    hare_sb_free(sb);
    return result;
}

// Convert camelCase to snake_case for Hare
static void to_snake_case(const char* input, char* output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 1; i++) {
        if (input[i] >= 'A' && input[i] <= 'Z') {
            if (i > 0 && j < output_size - 1) {
                output[j++] = '_';
            }
            if (j < output_size - 1) {
                output[j++] = input[i] + 32;
            }
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

// Generate component tree recursively
static void generate_component_tree(HareCodeGen* gen, cJSON* comp, const char* var_name, bool is_inline) {
    if (!comp || !cJSON_IsObject(comp)) return;

    const char* comp_type = "Container";
    cJSON* type_node = cJSON_GetObjectItem(comp, "type");
    if (type_node && cJSON_IsString(type_node)) {
        comp_type = cJSON_GetStringValue(type_node);
    }

    // Map IR type to Hare DSL function
    const char* dsl_func = "container";
    bool is_foreach = false;
    if (strcmp(comp_type, "Row") == 0) dsl_func = "row";
    else if (strcmp(comp_type, "Column") == 0) dsl_func = "column";
    else if (strcmp(comp_type, "Text") == 0) dsl_func = "text";
    else if (strcmp(comp_type, "Button") == 0) dsl_func = "button";
    else if (strcmp(comp_type, "Checkbox") == 0) dsl_func = "checkbox";
    else if (strcmp(comp_type, "Input") == 0) dsl_func = "input";
    else if (strcmp(comp_type, "TabGroup") == 0) dsl_func = "tab_group";
    else if (strcmp(comp_type, "TabBar") == 0) dsl_func = "tab_bar";
    else if (strcmp(comp_type, "TabContent") == 0) dsl_func = "tab_content";
    else if (strcmp(comp_type, "TabPanel") == 0) dsl_func = "tab_panel";
    else if (strcmp(comp_type, "ForEach") == 0) {
        dsl_func = "for_each";
        is_foreach = true;
    }

    // Convert camelCase to snake_case for Hare
    char func_name[256];
    to_snake_case(dsl_func, func_name, sizeof(func_name));

    hare_gen_add_line_fmt(gen, "%s {", func_name);
    gen->indent++;

    // Generate properties
    cJSON* width = cJSON_GetObjectItem(comp, "width");
    if (width && cJSON_IsString(width)) {
        const char* w = cJSON_GetStringValue(width);
        if (strcmp(w, "0.0px") != 0 && strcmp(w, "0px") != 0) {
            hare_gen_add_line_fmt(gen, "width = \"%s\",", w);
        }
    }

    cJSON* height = cJSON_GetObjectItem(comp, "height");
    if (height && cJSON_IsString(height)) {
        const char* h = cJSON_GetStringValue(height);
        if (strcmp(h, "0.0px") != 0 && strcmp(h, "0px") != 0) {
            hare_gen_add_line_fmt(gen, "height = \"%s\",", h);
        }
    }

    cJSON* padding = cJSON_GetObjectItem(comp, "padding");
    if (padding && cJSON_IsNumber(padding) && cJSON_GetNumberValue(padding) > 0) {
        hare_gen_add_line_fmt(gen, "padding = %u,", (unsigned int)cJSON_GetNumberValue(padding));
    }

    cJSON* fontSize = cJSON_GetObjectItem(comp, "fontSize");
    if (fontSize && cJSON_IsNumber(fontSize) && cJSON_GetNumberValue(fontSize) > 0) {
        hare_gen_add_line_fmt(gen, "font_size = %u,", (unsigned int)cJSON_GetNumberValue(fontSize));
    }

    cJSON* fontWeight = cJSON_GetObjectItem(comp, "fontWeight");
    if (fontWeight && cJSON_IsNumber(fontWeight) && cJSON_GetNumberValue(fontWeight) != 400) {
        hare_gen_add_line_fmt(gen, "font_weight = %u,", (unsigned int)cJSON_GetNumberValue(fontWeight));
    }

    cJSON* text = cJSON_GetObjectItem(comp, "text");
    if (text && cJSON_IsString(text)) {
        hare_gen_add_line_fmt(gen, "text = \"%s\",", cJSON_GetStringValue(text));
    }

    cJSON* backgroundColor = cJSON_GetObjectItem(comp, "backgroundColor");
    if (backgroundColor && cJSON_IsString(backgroundColor)) {
        hare_gen_add_line_fmt(gen, "background_color = \"%s\",", cJSON_GetStringValue(backgroundColor));
    }

    cJSON* background = cJSON_GetObjectItem(comp, "background");
    if (background && cJSON_IsString(background)) {
        hare_gen_add_line_fmt(gen, "background = \"%s\",", cJSON_GetStringValue(background));
    }

    cJSON* color = cJSON_GetObjectItem(comp, "color");
    if (color && cJSON_IsString(color)) {
        hare_gen_add_line_fmt(gen, "color = \"%s\",", cJSON_GetStringValue(color));
    }

    cJSON* borderRadius = cJSON_GetObjectItem(comp, "borderRadius");
    if (borderRadius && cJSON_IsNumber(borderRadius) && cJSON_GetNumberValue(borderRadius) > 0) {
        hare_gen_add_line_fmt(gen, "border_radius = %u,", (unsigned int)cJSON_GetNumberValue(borderRadius));
    }

    cJSON* borderWidth = cJSON_GetObjectItem(comp, "borderWidth");
    if (borderWidth && cJSON_IsNumber(borderWidth) && cJSON_GetNumberValue(borderWidth) > 0) {
        hare_gen_add_line_fmt(gen, "border_width = %u,", (unsigned int)cJSON_GetNumberValue(borderWidth));
    }

    cJSON* borderColor = cJSON_GetObjectItem(comp, "borderColor");
    if (borderColor && cJSON_IsString(borderColor)) {
        hare_gen_add_line_fmt(gen, "border_color = \"%s\",", cJSON_GetStringValue(borderColor));
    }

    cJSON* gap = cJSON_GetObjectItem(comp, "gap");
    if (gap && cJSON_IsNumber(gap) && cJSON_GetNumberValue(gap) > 0) {
        hare_gen_add_line_fmt(gen, "gap = %u,", (unsigned int)cJSON_GetNumberValue(gap));
    }

    cJSON* margin = cJSON_GetObjectItem(comp, "margin");
    if (margin && cJSON_IsNumber(margin) && cJSON_GetNumberValue(margin) > 0) {
        hare_gen_add_line_fmt(gen, "margin = %u,", (unsigned int)cJSON_GetNumberValue(margin));
    }

    cJSON* opacity = cJSON_GetObjectItem(comp, "opacity");
    if (opacity && cJSON_IsNumber(opacity)) {
        double op = cJSON_GetNumberValue(opacity);
        if (op < 1.0) {
            hare_gen_add_line_fmt(gen, "opacity = %.2f,", op);
        }
    }

    cJSON* alignItems = cJSON_GetObjectItem(comp, "alignItems");
    if (alignItems && cJSON_IsString(alignItems)) {
        hare_gen_add_line_fmt(gen, "align_items = \"%s\",", cJSON_GetStringValue(alignItems));
    }

    cJSON* justifyContent = cJSON_GetObjectItem(comp, "justifyContent");
    if (justifyContent && cJSON_IsString(justifyContent)) {
        hare_gen_add_line_fmt(gen, "justify_content = \"%s\",", cJSON_GetStringValue(justifyContent));
    }

    cJSON* textAlign = cJSON_GetObjectItem(comp, "textAlign");
    if (textAlign && cJSON_IsString(textAlign)) {
        hare_gen_add_line_fmt(gen, "text_align = \"%s\",", cJSON_GetStringValue(textAlign));
    }

    cJSON* placeholder = cJSON_GetObjectItem(comp, "placeholder");
    if (placeholder && cJSON_IsString(placeholder)) {
        hare_gen_add_line_fmt(gen, "placeholder = \"%s\",", cJSON_GetStringValue(placeholder));
    }

    cJSON* value = cJSON_GetObjectItem(comp, "value");
    if (value && cJSON_IsString(value)) {
        hare_gen_add_line_fmt(gen, "value = \"%s\",", cJSON_GetStringValue(value));
    }

    cJSON* checked = cJSON_GetObjectItem(comp, "checked");
    if (checked && cJSON_IsBool(checked)) {
        hare_gen_add_line_fmt(gen, "checked = %s,", cJSON_IsTrue(checked) ? "true" : "false");
    }

    cJSON* disabled = cJSON_GetObjectItem(comp, "disabled");
    if (disabled && cJSON_IsBool(disabled)) {
        hare_gen_add_line_fmt(gen, "disabled = %s,", cJSON_IsTrue(disabled) ? "true" : "false");
    }

    cJSON* visible = cJSON_GetObjectItem(comp, "visible");
    if (visible && cJSON_IsBool(visible) && !cJSON_IsTrue(visible)) {
        hare_gen_add_line(gen, "visible = false,");
    }

    // Generate children
    cJSON* children = cJSON_GetObjectItem(comp, "children");
    if (children && cJSON_IsArray(children) && cJSON_GetArraySize(children) > 0) {
        if (is_foreach) {
            hare_gen_add_line(gen, "");
            hare_gen_add_line(gen, "render = fn(item: *void, index: size) void = {");
            gen->indent++;
            hare_gen_add_line(gen, "return;");
            gen->indent++;
        } else {
            hare_gen_add_line(gen, "");
        }

        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            generate_component_tree(gen, child, NULL, true);
            if (!is_foreach) {
                hare_gen_add_line(gen, ",");
            }
        }

        if (is_foreach) {
            gen->indent--;
            gen->indent--;
            hare_gen_add_line(gen, "},");
        }
    }

    gen->indent--;
    hare_gen_add_line(gen, "}");
}

char* hare_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    // Parse JSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return NULL;
    }

    HareCodeGen* gen = hare_gen_create();
    if (!gen) {
        cJSON_Delete(root);
        return NULL;
    }

    // Header
    hare_gen_add_line(gen, "// Generated from .kir by Kryon Hare Code Generator");
    hare_gen_add_line(gen, "// Do not edit manually - regenerate from source");
    hare_gen_add_line(gen, "");
    hare_gen_add_line(gen, "use kryon::*;");
    hare_gen_add_line(gen, "");

    // Check for source_declarations
    cJSON* source_decls = cJSON_GetObjectItem(root, "source_declarations");

    if (source_decls && cJSON_IsObject(source_decls)) {
        cJSON* requires = cJSON_GetObjectItem(source_decls, "requires");
        if (requires && cJSON_IsArray(requires)) {
            cJSON* req = NULL;
            cJSON_ArrayForEach(req, requires) {
                cJSON* mod = cJSON_GetObjectItem(req, "module");
                if (mod && cJSON_IsString(mod)) {
                    hare_gen_add_line_fmt(gen, "use %s;", cJSON_GetStringValue(mod));
                }
            }
            hare_gen_add_line(gen, "");
        }
    }

    // Check what type of module this is
    cJSON* component = cJSON_GetObjectItem(root, "root");
    if (!component) component = cJSON_GetObjectItem(root, "component");

    cJSON* component_defs = cJSON_GetObjectItem(root, "component_definitions");

    if (component && !cJSON_IsNull(component)) {
        // Main module with root component tree
        hare_gen_add_line(gen, "// UI Component Tree");
        hare_gen_add_line(gen, "export fn main() void = {");
        gen->indent++;
        generate_component_tree(gen, component, "root", false);
        gen->indent--;
        hare_gen_add_line(gen, "};");
    } else if (component_defs && cJSON_IsArray(component_defs) && cJSON_GetArraySize(component_defs) > 0) {
        // Component module
        hare_gen_add_line(gen, "// Component Definitions");

        cJSON* comp_def = NULL;
        cJSON_ArrayForEach(comp_def, component_defs) {
            cJSON* name = cJSON_GetObjectItem(comp_def, "name");
            cJSON* template = cJSON_GetObjectItem(comp_def, "template");

            if (name && cJSON_IsString(name) && template) {
                const char* func_name = cJSON_GetStringValue(name);
                hare_gen_add_line_fmt(gen, "export fn %s(props: *ComponentProps) *Component = {", func_name);
                gen->indent++;
                hare_gen_add_line(gen, "return;");
                gen->indent++;
                generate_component_tree(gen, template, NULL, true);
                gen->indent--;
                gen->indent--;
                hare_gen_add_line(gen, "};");
                hare_gen_add_line(gen, "");
            }
        }
    }

    // Get result
    IRStringBuilder* clone = ir_sb_clone(gen->sb);
    char* result = clone ? ir_sb_build(clone) : NULL;
    hare_gen_free(gen);
    cJSON_Delete(root);
    return result;
}

// ============================================================================
// Multi-File Support
// ============================================================================

/**
 * Recursively process a module and its transitive imports
 */
static int hare_process_module_recursive(const char* module_id, const char* kir_dir,
                                         const char* output_dir, CodegenProcessedModules* processed) {
    // Skip if already processed
    if (codegen_processed_modules_contains(processed, module_id)) return 0;
    codegen_processed_modules_add(processed, module_id);

    // Skip internal modules
    if (codegen_is_internal_module(module_id)) return 0;

    // Check if this is an external plugin
    if (codegen_is_external_plugin(module_id)) {
        return 0;
    }

    // Build path to component's KIR file
    char component_kir_path[2048];
    snprintf(component_kir_path, sizeof(component_kir_path),
             "%s/%s.kir", kir_dir, module_id);

    // Read component's KIR
    char* component_kir_json = codegen_read_kir_file(component_kir_path, NULL);
    if (!component_kir_json) {
        return 0;
    }

    // Parse to get transitive imports before generating
    cJSON* component_root = cJSON_Parse(component_kir_json);
    int files_written = 0;

    // Generate Hare from KIR
    char* component_hare = hare_codegen_from_json(component_kir_json);
    free(component_kir_json);

    if (component_hare) {
        // Write to output directory maintaining hierarchy
        char output_path[2048];
        snprintf(output_path, sizeof(output_path),
                 "%s/%s.ha", output_dir, module_id);

        if (codegen_write_file_with_mkdir(output_path, component_hare)) {
            printf("Generated: %s.ha\n", module_id);
            files_written++;
        }
        free(component_hare);
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
                    files_written += hare_process_module_recursive(sub_module_id, kir_dir,
                                                                   output_dir, processed);
                }
            }
        }
        cJSON_Delete(component_root);
    }

    return files_written;
}

bool hare_codegen_generate(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: Invalid arguments to hare_codegen_generate_multi\n");
        return false;
    }

    // Set error prefix for this codegen
    codegen_set_error_prefix("Hare");

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

    // 1. Generate main.ha from main.kir
    char* main_hare = hare_codegen_from_json(main_kir_json);
    free(main_kir_json);

    if (main_hare) {
        char main_output_path[2048];
        snprintf(main_output_path, sizeof(main_output_path), "%s/main.ha", output_dir);

        if (codegen_write_file_with_mkdir(main_output_path, main_hare)) {
            printf("Generated: main.ha\n");
            files_written++;
        }
        free(main_hare);
    }

    // 2. Get the KIR directory (parent of kir_path)
    char kir_dir[2048];
    codegen_get_parent_dir(kir_path, kir_dir, sizeof(kir_dir));

    // 3. Track processed modules to avoid duplicates
    CodegenProcessedModules processed = {0};
    codegen_processed_modules_add(&processed, "main");

    // 4. Process each import recursively (including transitive imports)
    cJSON* imports = cJSON_GetObjectItem(main_root, "imports");
    if (imports && cJSON_IsArray(imports)) {
        cJSON* import_item = NULL;
        cJSON_ArrayForEach(import_item, imports) {
            if (!cJSON_IsString(import_item)) continue;

            const char* module_id = cJSON_GetStringValue(import_item);
            if (module_id) {
                files_written += hare_process_module_recursive(module_id, kir_dir,
                                                               output_dir, &processed);
            }
        }
    }

    codegen_processed_modules_free(&processed);
    cJSON_Delete(main_root);

    if (files_written == 0) {
        fprintf(stderr, "Warning: No Hare files were generated\n");
        return false;
    }

    printf("Generated %d Hare files in %s\n", files_written, output_dir);
    return true;
}

// Legacy alias for compatibility - delegates to hare_codegen_generate
bool hare_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    return hare_codegen_generate(kir_path, output_dir);
}
