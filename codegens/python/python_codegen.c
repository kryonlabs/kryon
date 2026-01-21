/**
 * Python Code Generator
 * Generates Python DSL code from KIR JSON
 */

#define _POSIX_C_SOURCE 200809L

#include "python_codegen.h"
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
// String builder wrapper using IRStringBuilder
// ============================================================================

typedef struct {
    IRStringBuilder* sb;
    int indent;
} PythonCodeGen;

static PythonCodeGen* python_gen_create(void) {
    PythonCodeGen* gen = calloc(1, sizeof(PythonCodeGen));
    if (!gen) return NULL;

    gen->sb = ir_sb_create(8192);
    if (!gen->sb) {
        free(gen);
        return NULL;
    }
    gen->indent = 0;
    return gen;
}

static void python_gen_free(PythonCodeGen* gen) {
    if (gen) {
        ir_sb_free(gen->sb);
        free(gen);
    }
}

static void python_gen_add_line(PythonCodeGen* gen, const char* line) {
    if (!gen || !gen->sb) return;

    // Add indentation
    ir_sb_indent(gen->sb, gen->indent);
    ir_sb_append_line(gen->sb, line);
}

static void python_gen_add_line_fmt(PythonCodeGen* gen, const char* fmt, ...) {
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
// Python Code Generation
// ============================================================================

/**
 * Convert cJSON value to Python literal syntax
 * Converts JSON to Python literal syntax (very similar to JSON)
 */
static char* cJSON_to_python_syntax(const cJSON* item) {
    if (!item) return strdup("None");

    // String builder for result
    IRStringBuilder* sb = ir_sb_create(256);
    if (!sb) return NULL;

    switch (item->type) {
        case cJSON_NULL:
            ir_sb_append(sb, "None");
            break;

        case cJSON_False:
        case cJSON_True:
            ir_sb_append(sb, cJSON_IsTrue(item) ? "True" : "False");
            break;

        case cJSON_Number:
            if (cJSON_IsNumber(item)) {
                double num = cJSON_GetNumberValue(item);
                // Check if it's an integer
                if (num == (int)num) {
                    ir_sb_appendf(sb, "%d", (int)num);
                } else {
                    ir_sb_appendf(sb, "%f", num);
                }
            } else {
                ir_sb_append(sb, "0");
            }
            break;

        case cJSON_String: {
            const char* str = cJSON_GetStringValue(item);
            if (!str) {
                ir_sb_append(sb, "\"\"");
                break;
            }

            // Check if string contains newlines
            bool has_newline = (strchr(str, '\n') != NULL);

            if (has_newline) {
                // Use triple-quoted string for multi-line strings
                ir_sb_append(sb, "\"\"\"");
                ir_sb_append(sb, str);
                ir_sb_append(sb, "\"\"\"");
            } else {
                // Use regular quoted string for single-line strings
                ir_sb_append(sb, "\"");
                for (size_t i = 0; str[i]; i++) {
                    if (str[i] == '"' || str[i] == '\\') {
                        ir_sb_append(sb, "\\");
                    }
                    ir_sb_append_n(sb, &str[i], 1);
                }
                ir_sb_append(sb, "\"");
            }
            break;
        }

        case cJSON_Array: {
            ir_sb_append(sb, "[");
            const cJSON* child = item->child;
            bool first = true;
            while (child) {
                if (!first) ir_sb_append(sb, ", ");
                first = false;
                char* child_py = cJSON_to_python_syntax(child);
                if (child_py) {
                    ir_sb_append(sb, child_py);
                    free(child_py);
                }
                child = child->next;
            }
            ir_sb_append(sb, "]");
            break;
        }

        case cJSON_Object: {
            ir_sb_append(sb, "{");
            const cJSON* child = item->child;
            bool first = true;
            while (child) {
                if (!first) ir_sb_append(sb, ", ");
                first = false;

                // Key (always a string in JSON objects)
                if (child->string && child->string[0]) {
                    ir_sb_append(sb, "\"");
                    ir_sb_append(sb, child->string);
                    ir_sb_append(sb, "\": ");
                }

                // Value
                char* child_py = cJSON_to_python_syntax(child);
                if (child_py) {
                    ir_sb_append(sb, child_py);
                    free(child_py);
                }
                child = child->next;
            }
            ir_sb_append(sb, "}");
            break;
        }

        default:
            ir_sb_append(sb, "None");
            break;
    }

    char* result = ir_sb_build(sb);
    ir_sb_free(sb);
    return result ? result : strdup("None");
}

/**
 * Convert camelCase to snake_case for Python properties
 */
static char* camel_to_snake(const char* camel) {
    if (!camel) return NULL;

    size_t len = strlen(camel);
    char* snake = malloc(len * 2 + 1);  // Worst case: all caps
    if (!snake) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && camel[i] >= 'A' && camel[i] <= 'Z') {
            snake[j++] = '_';
            snake[j++] = camel[i] + 32;  // Convert to lowercase
        } else {
            snake[j++] = camel[i];
        }
    }
    snake[j] = '\0';

    return snake;
}

/**
 * Generate component tree recursively
 */
static void generate_component_tree(PythonCodeGen* gen, cJSON* comp, const char* var_name, bool is_inline) {
    if (!comp || !cJSON_IsObject(comp)) return;

    const char* comp_type = "Container";
    cJSON* type_node = cJSON_GetObjectItem(comp, "type");
    if (type_node && cJSON_IsString(type_node)) {
        comp_type = cJSON_GetStringValue(type_node);
    }

    // Get properties
    cJSON* props = cJSON_GetObjectItem(comp, "properties");
    cJSON* style = cJSON_GetObjectItem(comp, "style");
    cJSON* layout = cJSON_GetObjectItem(comp, "layout");
    cJSON* children = cJSON_GetObjectItem(comp, "children");

    // Build constructor arguments
    IRStringBuilder* args = ir_sb_create(256);
    bool has_args = false;

    // Add properties as keyword arguments
    if (props && cJSON_IsObject(props)) {
        cJSON* prop = props->child;
        while (prop) {
            if (prop->string) {
                if (has_args) ir_sb_append(args, ", ");

                // Convert camelCase to snake_case
                char* snake_key = camel_to_snake(prop->string);
                ir_sb_appendf(args, "%s=", snake_key);
                free(snake_key);

                // Convert value to Python syntax
                char* py_val = cJSON_to_python_syntax(prop);
                if (py_val) {
                    ir_sb_append(args, py_val);
                    free(py_val);
                }
                has_args = true;
            }
            prop = prop->next;
        }
    }

    // Add style if present
    if (style && cJSON_IsObject(style)) {
        if (has_args) ir_sb_append(args, ", ");
        ir_sb_append(args, "style=Style(");

        cJSON* style_prop = style->child;
        bool first_style = true;
        while (style_prop) {
            if (style_prop->string) {
                if (!first_style) ir_sb_append(args, ", ");

                char* snake_key = camel_to_snake(style_prop->string);
                ir_sb_appendf(args, "%s=", snake_key);
                free(snake_key);

                char* py_val = cJSON_to_python_syntax(style_prop);
                if (py_val) {
                    ir_sb_append(args, py_val);
                    free(py_val);
                }
                first_style = false;
            }
            style_prop = style_prop->next;
        }
        ir_sb_append(args, ")");
        has_args = true;
    }

    // Add layout if present
    if (layout && cJSON_IsObject(layout)) {
        if (has_args) ir_sb_append(args, ", ");
        ir_sb_append(args, "layout=Layout(");

        cJSON* layout_prop = layout->child;
        bool first_layout = true;
        while (layout_prop) {
            if (layout_prop->string) {
                if (!first_layout) ir_sb_append(args, ", ");

                char* snake_key = camel_to_snake(layout_prop->string);
                ir_sb_appendf(args, "%s=", snake_key);
                free(snake_key);

                char* py_val = cJSON_to_python_syntax(layout_prop);
                if (py_val) {
                    ir_sb_append(args, py_val);
                    free(py_val);
                }
                first_layout = false;
            }
            layout_prop = layout_prop->next;
        }
        ir_sb_append(args, ")");
        has_args = true;
    }

    // Generate component creation
    char* args_str = ir_sb_build(args);
    if (has_args) {
        python_gen_add_line_fmt(gen, "%s = %s(%s)", var_name, comp_type, args_str);
    } else {
        python_gen_add_line_fmt(gen, "%s = %s()", var_name, comp_type);
    }
    free(args_str);
    ir_sb_free(args);

    // Add children
    if (children && cJSON_IsArray(children)) {
        gen->indent++;

        cJSON* child = NULL;
        int i = 0;
        cJSON_ArrayForEach(child, children) {
            char child_var_name[256];
            snprintf(child_var_name, sizeof(child_var_name), "%s_child_%d", var_name, i);

            generate_component_tree(gen, child, child_var_name, true);
            python_gen_add_line_fmt(gen, "%s.add_child(%s)", var_name, child_var_name);

            i++;
        }

        gen->indent--;
    }
}

/**
 * Generate Python code from KIR JSON
 */
static char* python_codegen_from_kir(cJSON* root) {
    if (!root) return NULL;

    PythonCodeGen* gen = python_gen_create();
    if (!gen) return NULL;

    // Add imports
    python_gen_add_line(gen, "import kryon");
    python_gen_add_line(gen, "from kryon.dsl import (");
    gen->indent++;
    python_gen_add_line(gen, "Container, Row, Column, Center,");
    python_gen_add_line(gen, "Text, Button, Input, Checkbox, Dropdown, Textarea,");
    python_gen_add_line(gen, "Image, Canvas, Markdown,");
    python_gen_add_line(gen, "TabGroup, TabBar, Tab, TabContent, TabPanel,");
    python_gen_add_line(gen, "Modal,");
    python_gen_add_line(gen, "Table, TableHead, TableBody, TableFoot,");
    python_gen_add_line(gen, "TableRow, TableCell, TableHeaderCell,");
    python_gen_add_line(gen, "Heading, Paragraph, Blockquote, CodeBlock,");
    python_gen_add_line(gen, "HorizontalRule, List, ListItem, Link,");
    python_gen_add_line(gen, "Span, Strong, Em, CodeInline, Small, Mark,");
    gen->indent--;
    python_gen_add_line(gen, ")");
    python_gen_add_line(gen, "from kryon.dsl import Style, Layout, Color");
    python_gen_add_line(gen, "");

    // Get root component
    cJSON* root_comp = cJSON_GetObjectItem(root, "root");
    if (!root_comp) {
        // Try direct component (no wrapper)
        root_comp = root;
    }

    // Generate component tree
    generate_component_tree(gen, root_comp, "app", false);

    char* result = ir_sb_build(gen->sb);
    python_gen_free(gen);

    return result;
}

// ============================================================================
// Public API
// ============================================================================

bool python_codegen_generate(const char* kir_path, const char* output_path) {
    return python_codegen_generate_with_options(kir_path, output_path, NULL);
}

char* python_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    // Parse KIR JSON
    cJSON* root = cJSON_ParseWithLengthOpts(kir_json, strlen(kir_json), NULL, 1);
    if (!root) {
        fprintf(stderr, "Failed to parse KIR JSON\n");
        return NULL;
    }

    // Generate Python code
    char* python_code = python_codegen_from_kir(root);

    cJSON_Delete(root);
    return python_code;
}

bool python_codegen_generate_with_options(const char* kir_path,
                                          const char* output_path,
                                          PythonCodegenOptions* options) {
    // Read KIR file
    size_t size;
    char* kir_json = codegen_read_kir_file(kir_path, &size);
    if (!kir_json) {
        fprintf(stderr, "Failed to read KIR file: %s\n", kir_path);
        return false;
    }

    // Generate Python code
    char* python_code = python_codegen_from_json(kir_json);
    free(kir_json);

    if (!python_code) {
        fprintf(stderr, "Failed to generate Python code\n");
        return false;
    }

    // Write output file
    if (!codegen_write_output_file(output_path, python_code)) {
        fprintf(stderr, "Failed to write output file: %s\n", output_path);
        free(python_code);
        return false;
    }

    free(python_code);
    return true;
}

/**
 * Recursively process a module and its transitive imports for Python
 */
static int python_process_module_recursive(const char* module_id, const char* kir_dir,
                                           const char* output_dir, CodegenProcessedModules* processed) {
    // Skip if already processed
    if (codegen_processed_modules_contains(processed, module_id)) return 0;
    codegen_processed_modules_add(processed, module_id);

    // Skip internal modules
    if (codegen_is_internal_module(module_id)) return 0;

    // Skip external plugins (they're runtime dependencies, not source)
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

    // Generate Python from KIR
    char* component_py = python_codegen_from_json(component_kir_json);
    free(component_kir_json);

    if (component_py) {
        // Write to output directory maintaining hierarchy
        char output_path[2048];
        snprintf(output_path, sizeof(output_path),
                 "%s/%s.py", output_dir, module_id);

        if (codegen_write_file_with_mkdir(output_path, component_py)) {
            printf("✓ Generated: %s.py\n", module_id);
            files_written++;

            // Create __init__.py in component's directory if it's nested
            char* slash = strrchr(output_path, '/');
            if (slash) {
                char init_path[2048];
                size_t dir_len = (size_t)(slash - output_path);
                strncpy(init_path, output_path, dir_len);
                init_path[dir_len] = '\0';
                strncat(init_path, "/__init__.py", sizeof(init_path) - dir_len - 1);

                // Only create if doesn't exist
                if (!codegen_file_exists(init_path)) {
                    codegen_write_file_with_mkdir(init_path, "# Auto-generated by Kryon\n");
                }
            }
        }
        free(component_py);
    } else {
        fprintf(stderr, "Warning: Failed to generate Python for '%s'\n", module_id);
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
                    files_written += python_process_module_recursive(sub_module_id, kir_dir,
                                                                     output_dir, processed);
                }
            }
        }
        cJSON_Delete(component_root);
    }

    return files_written;
}

bool python_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: Invalid arguments to python_codegen_generate_multi\n");
        return false;
    }

    // Set error prefix for this codegen
    codegen_set_error_prefix("Python");

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

    // 1. Generate main.py from main.kir
    char* main_py = python_codegen_from_json(main_kir_json);
    free(main_kir_json);

    if (main_py) {
        char main_output_path[2048];
        snprintf(main_output_path, sizeof(main_output_path), "%s/main.py", output_dir);

        if (codegen_write_file_with_mkdir(main_output_path, main_py)) {
            printf("✓ Generated: main.py\n");
            files_written++;
        }
        free(main_py);
    } else {
        fprintf(stderr, "Warning: Failed to generate main.py from KIR\n");
    }

    // 2. Create root __init__.py
    char init_path[2048];
    snprintf(init_path, sizeof(init_path), "%s/__init__.py", output_dir);
    codegen_write_file_with_mkdir(init_path, "# Auto-generated by Kryon\n");

    // 3. Get the KIR directory (parent of kir_path)
    char kir_dir[2048];
    codegen_get_parent_dir(kir_path, kir_dir, sizeof(kir_dir));

    // 4. Track processed modules to avoid duplicates
    CodegenProcessedModules processed = {0};
    codegen_processed_modules_add(&processed, "main");  // Mark main as processed

    // 5. Process each import recursively (including transitive imports)
    cJSON* imports = cJSON_GetObjectItem(main_root, "imports");
    if (imports && cJSON_IsArray(imports)) {
        cJSON* import_item = NULL;
        cJSON_ArrayForEach(import_item, imports) {
            if (!cJSON_IsString(import_item)) continue;

            const char* module_id = cJSON_GetStringValue(import_item);
            if (module_id) {
                files_written += python_process_module_recursive(module_id, kir_dir,
                                                                 output_dir, &processed);
            }
        }
    }

    codegen_processed_modules_free(&processed);
    cJSON_Delete(main_root);

    if (files_written == 0) {
        fprintf(stderr, "Warning: No Python files were generated\n");
        return false;
    }

    printf("✓ Generated %d Python files in %s\n", files_written, output_dir);
    return true;
}
