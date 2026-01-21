/**
 * Kotlin Code Generator - KIR → Kotlin MainActivity
 *
 * Generates idiomatic Kotlin code with Kryon DSL from KIR JSON files
 */

#define _POSIX_C_SOURCE 200809L

#include "kotlin_codegen.h"
#include "../codegen_common.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// ============================================================================
// Code Generation Context
// ============================================================================

typedef struct {
    FILE* output;
    int indent_level;
    cJSON* root_json;           // Full KIR JSON
    cJSON* component_tree;      // Root component
    cJSON* logic_block;         // Logic block (events, state, etc.)
    const char* package_name;   // Package name for generated code
    const char* class_name;     // Class name (default: MainActivity)
} KotlinCodegenContext;

// ============================================================================
// Utility Functions
// ============================================================================

// Get package name from KIR metadata or environment variable
// Returns a string that must NOT be freed (static buffer or pointer to JSON string)
static const char* get_package_name(cJSON* root_json) {
    // Try to get from KIR metadata
    cJSON* metadata = cJSON_GetObjectItem(root_json, "metadata");
    if (metadata) {
        cJSON* package = cJSON_GetObjectItem(metadata, "package");
        if (package && cJSON_IsString(package)) {
            return package->valuestring;
        }
    }

    // Try environment variable
    const char* env_package = getenv("KRYON_KOTLIN_PACKAGE");
    if (env_package && env_package[0] != '\0') {
        return env_package;
    }

    // Default fallback
    return "com.kryon.generated";
}

// Get class name from KIR metadata or environment variable
// Returns a string that must NOT be freed (static buffer or pointer to JSON string)
static const char* get_class_name(cJSON* root_json) {
    // Try to get from KIR metadata
    cJSON* metadata = cJSON_GetObjectItem(root_json, "metadata");
    if (metadata) {
        cJSON* class_name = cJSON_GetObjectItem(metadata, "class");
        if (class_name && cJSON_IsString(class_name)) {
            return class_name->valuestring;
        }
    }

    // Try environment variable
    const char* env_class = getenv("KRYON_KOTLIN_CLASS");
    if (env_class && env_class[0] != '\0') {
        return env_class;
    }

    // Default fallback
    return "MainActivity";
}

static void write_indent(KotlinCodegenContext* ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        fprintf(ctx->output, "    ");
    }
}

static void writeln(KotlinCodegenContext* ctx, const char* str) {
    write_indent(ctx);
    fprintf(ctx->output, "%s\n", str);
}

// Use codegen_codegen_parse_px_value() and codegen_codegen_is_transparent_color() from codegen_common.h

// ============================================================================
// Component Property Generation
// ============================================================================

static void generate_property(KotlinCodegenContext* ctx, const char* key, cJSON* value) {
    if (!key || !value) return;

    // Width
    if (strcmp(key, "width") == 0 && value->valuestring) {
        float width = codegen_parse_px_value(value->valuestring);
        write_indent(ctx);
        fprintf(ctx->output, "width(%.1ff)\n", width);
        return;
    }

    // Height
    if (strcmp(key, "height") == 0 && value->valuestring) {
        float height = codegen_parse_px_value(value->valuestring);
        write_indent(ctx);
        fprintf(ctx->output, "height(%.1ff)\n", height);
        return;
    }

    // Background color
    if (strcmp(key, "background") == 0 && value->valuestring) {
        if (!codegen_is_transparent_color(value->valuestring)) {
            write_indent(ctx);
            fprintf(ctx->output, "background(\"%s\")\n", value->valuestring);
        }
        return;
    }

    // Text color
    if (strcmp(key, "color") == 0 && value->valuestring) {
        if (!codegen_is_transparent_color(value->valuestring)) {
            write_indent(ctx);
            fprintf(ctx->output, "color(\"%s\")\n", value->valuestring);
        }
        return;
    }

    // Font size
    if (strcmp(key, "fontSize") == 0 && cJSON_IsNumber(value)) {
        write_indent(ctx);
        fprintf(ctx->output, "fontSize(%.1ff)\n", (float)value->valuedouble);
        return;
    }

    // Font weight
    if (strcmp(key, "fontWeight") == 0 && cJSON_IsNumber(value)) {
        write_indent(ctx);
        fprintf(ctx->output, "fontWeight(%d)\n", value->valueint);
        return;
    }

    // Padding (all sides)
    if (strcmp(key, "padding") == 0) {
        if (cJSON_IsObject(value)) {
            cJSON* top = cJSON_GetObjectItem(value, "top");
            cJSON* right = cJSON_GetObjectItem(value, "right");
            cJSON* bottom = cJSON_GetObjectItem(value, "bottom");
            cJSON* left = cJSON_GetObjectItem(value, "left");

            if (top && right && bottom && left) {
                write_indent(ctx);
                fprintf(ctx->output, "padding(%.1ff, %.1ff, %.1ff, %.1ff)\n",
                        (float)top->valuedouble, (float)right->valuedouble,
                        (float)bottom->valuedouble, (float)left->valuedouble);
            }
        } else if (cJSON_IsNumber(value)) {
            write_indent(ctx);
            fprintf(ctx->output, "padding(%.1ff)\n", (float)value->valuedouble);
        }
        return;
    }

    // Margin (all sides)
    if (strcmp(key, "margin") == 0) {
        if (cJSON_IsObject(value)) {
            cJSON* top = cJSON_GetObjectItem(value, "top");
            cJSON* right = cJSON_GetObjectItem(value, "right");
            cJSON* bottom = cJSON_GetObjectItem(value, "bottom");
            cJSON* left = cJSON_GetObjectItem(value, "left");

            if (top && right && bottom && left) {
                write_indent(ctx);
                fprintf(ctx->output, "margin(%.1ff, %.1ff, %.1ff, %.1ff)\n",
                        (float)top->valuedouble, (float)right->valuedouble,
                        (float)bottom->valuedouble, (float)left->valuedouble);
            }
        } else if (cJSON_IsNumber(value)) {
            write_indent(ctx);
            fprintf(ctx->output, "margin(%.1ff)\n", (float)value->valuedouble);
        }
        return;
    }

    // Gap
    if (strcmp(key, "gap") == 0 && cJSON_IsNumber(value)) {
        write_indent(ctx);
        fprintf(ctx->output, "gap(%.1ff)\n", (float)value->valuedouble);
        return;
    }

    // Alignment
    if (strcmp(key, "alignItems") == 0 && value->valuestring) {
        write_indent(ctx);
        fprintf(ctx->output, "alignItems(\"%s\")\n", value->valuestring);
        return;
    }

    if (strcmp(key, "justifyContent") == 0 && value->valuestring) {
        write_indent(ctx);
        fprintf(ctx->output, "justifyContent(\"%s\")\n", value->valuestring);
        return;
    }

    // Position
    if (strcmp(key, "position") == 0 && value->valuestring) {
        write_indent(ctx);
        fprintf(ctx->output, "position(\"%s\")\n", value->valuestring);
        return;
    }

    if (strcmp(key, "left") == 0 && cJSON_IsNumber(value)) {
        write_indent(ctx);
        fprintf(ctx->output, "left(%.1ff)\n", (float)value->valuedouble);
        return;
    }

    if (strcmp(key, "top") == 0 && cJSON_IsNumber(value)) {
        write_indent(ctx);
        fprintf(ctx->output, "top(%.1ff)\n", (float)value->valuedouble);
        return;
    }

    // Border
    if (strcmp(key, "border") == 0 && cJSON_IsObject(value)) {
        cJSON* width = cJSON_GetObjectItem(value, "width");
        cJSON* color = cJSON_GetObjectItem(value, "color");
        cJSON* radius = cJSON_GetObjectItem(value, "radius");

        if (width && color && color->valuestring) {
            write_indent(ctx);
            fprintf(ctx->output, "border(%.1ff, \"%s\"",
                    (float)width->valuedouble, color->valuestring);
            if (radius && radius->valuedouble > 0) {
                fprintf(ctx->output, ", %.1ff", (float)radius->valuedouble);
            }
            fprintf(ctx->output, ")\n");
        }
        return;
    }
}

// ============================================================================
// Component Tree Generation
// ============================================================================

static void generate_component_recursive(KotlinCodegenContext* ctx, cJSON* component);

static void generate_component_recursive(KotlinCodegenContext* ctx, cJSON* component) {
    if (!component) return;

    cJSON* type_obj = cJSON_GetObjectItem(component, "type");
    cJSON* text_obj __attribute__((unused)) = cJSON_GetObjectItem(component, "text");
    cJSON* children_obj = cJSON_GetObjectItem(component, "children");

    if (!type_obj || !type_obj->valuestring) return;

    const char* type = type_obj->valuestring;

    // Start component
    write_indent(ctx);
    fprintf(ctx->output, "%s {\n", type);

    ctx->indent_level++;

    // Generate properties
    cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, component) {
        const char* key = prop->string;
        if (!key) continue;

        // Skip metadata fields
        if (strcmp(key, "id") == 0 || strcmp(key, "type") == 0 ||
            strcmp(key, "children") == 0 || strcmp(key, "direction") == 0) {
            continue;
        }

        // Handle text content
        if (strcmp(key, "text") == 0 && prop->valuestring) {
            write_indent(ctx);
            fprintf(ctx->output, "text(\"%s\")\n", prop->valuestring);
            continue;
        }

        // Generate other properties
        generate_property(ctx, key, prop);
    }

    // Generate children
    if (children_obj && cJSON_IsArray(children_obj)) {
        int child_count = cJSON_GetArraySize(children_obj);
        if (child_count > 0) {
            fprintf(ctx->output, "\n");  // Blank line before children
        }

        for (int i = 0; i < child_count; i++) {
            cJSON* child = cJSON_GetArrayItem(children_obj, i);
            generate_component_recursive(ctx, child);
        }
    }

    ctx->indent_level--;
    write_indent(ctx);
    fprintf(ctx->output, "}\n");
}

// ============================================================================
// Class Generation
// ============================================================================

static void generate_header(KotlinCodegenContext* ctx) {
    fprintf(ctx->output, "package %s\n\n", ctx->package_name);
    fprintf(ctx->output, "import android.util.Log\n");
    fprintf(ctx->output, "import com.kryon.KryonActivity\n\n");
}

static void generate_class(KotlinCodegenContext* ctx) {
    fprintf(ctx->output, "class %s : KryonActivity() {\n", ctx->class_name);
    ctx->indent_level++;

    // onKryonCreate method
    writeln(ctx, "override fun onKryonCreate() {");
    ctx->indent_level++;

    writeln(ctx, "Log.d(\"Kryon\", \"MainActivity.onKryonCreate()\")");
    fprintf(ctx->output, "\n");

    // setContent block
    writeln(ctx, "setContent {");
    ctx->indent_level++;

    // Generate component tree
    if (ctx->component_tree) {
        generate_component_recursive(ctx, ctx->component_tree);
    }

    ctx->indent_level--;
    writeln(ctx, "}");

    ctx->indent_level--;
    writeln(ctx, "}");

    ctx->indent_level--;
    fprintf(ctx->output, "}\n");
}

// ============================================================================
// Main Entry Point
// ============================================================================

bool ir_generate_kotlin_code(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) return false;

    // Set error prefix for this codegen
    codegen_set_error_prefix("Kotlin");

    // Read KIR file using shared utility
    char* kir_json = codegen_read_kir_file(kir_path, NULL);
    if (!kir_json) {
        return false;
    }

    // Parse KIR JSON
    cJSON* root = cJSON_Parse(kir_json);
    free(kir_json);

    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return false;
    }

    // Open output file
    FILE* output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: Failed to open output file: %s\n", output_path);
        cJSON_Delete(root);
        return false;
    }

    // Setup context
    KotlinCodegenContext ctx = {0};
    ctx.output = output;
    ctx.indent_level = 0;
    ctx.root_json = root;
    ctx.component_tree = cJSON_GetObjectItem(root, "root");
    ctx.logic_block = cJSON_GetObjectItem(root, "logic_block");
    ctx.package_name = get_package_name(root);
    ctx.class_name = get_class_name(root);

    // Generate code
    generate_header(&ctx);
    generate_class(&ctx);

    // Cleanup
    fclose(output);
    cJSON_Delete(root);

    return true;
}

bool ir_generate_kotlin_code_from_string(const char* kir_json, const char* output_path) {
    if (!kir_json || !output_path) return false;

    // Parse KIR JSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return false;
    }

    // Open output file
    FILE* output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: Failed to open output file: %s\n", output_path);
        cJSON_Delete(root);
        return false;
    }

    // Setup context
    KotlinCodegenContext ctx = {0};
    ctx.output = output;
    ctx.indent_level = 0;
    ctx.root_json = root;
    ctx.component_tree = cJSON_GetObjectItem(root, "root");
    ctx.logic_block = cJSON_GetObjectItem(root, "logic_block");
    ctx.package_name = get_package_name(root);
    ctx.class_name = get_class_name(root);

    // Generate code
    generate_header(&ctx);
    generate_class(&ctx);

    // Cleanup
    fclose(output);
    cJSON_Delete(root);

    return true;
}

/**
 * Convert module_id like "components/calendar" to PascalCase "Calendar"
 */
static char* kotlin_module_to_class_name(const char* module_id) {
    if (!module_id) return strdup("Component");

    // Find the last part after the final slash
    const char* name_start = strrchr(module_id, '/');
    if (name_start) {
        name_start++;  // Skip the slash
    } else {
        name_start = module_id;
    }

    // Allocate and copy
    char* result = strdup(name_start);
    if (!result) return strdup("Component");

    // Capitalize first letter (simple PascalCase)
    if (result[0] >= 'a' && result[0] <= 'z') {
        result[0] = result[0] - 'a' + 'A';
    }

    return result;
}

/**
 * Convert module_id to Kotlin package path (dots instead of slashes)
 */
static char* kotlin_module_to_package(const char* base_package, const char* module_id) {
    if (!module_id) return strdup(base_package);

    // Find the directory portion (without the file name)
    const char* last_slash = strrchr(module_id, '/');
    if (!last_slash) {
        // No subdirectory, use base package
        return strdup(base_package);
    }

    // Build package: base_package.subdir1.subdir2
    size_t dir_len = (size_t)(last_slash - module_id);
    size_t base_len = strlen(base_package);
    char* result = malloc(base_len + 1 + dir_len + 1);
    if (!result) return strdup(base_package);

    strcpy(result, base_package);
    strcat(result, ".");

    // Copy directory part, converting / to .
    size_t pos = base_len + 1;
    for (size_t i = 0; i < dir_len; i++) {
        if (module_id[i] == '/') {
            result[pos++] = '.';
        } else {
            result[pos++] = module_id[i];
        }
    }
    result[pos] = '\0';

    return result;
}

/**
 * Recursively process a module and its transitive imports for Kotlin
 */
static int kotlin_process_module_recursive(const char* module_id, const char* kir_dir,
                                           const char* output_dir, const char* base_package,
                                           CodegenProcessedModules* processed) {
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

    // Get class name and package for this component
    char* class_name = kotlin_module_to_class_name(module_id);
    char* package_name = kotlin_module_to_package(base_package, module_id);

    // Write Kotlin file
    char output_path[2048];
    snprintf(output_path, sizeof(output_path), "%s/%s.kt", output_dir, module_id);

    if (ir_generate_kotlin_code_from_string(component_kir_json, output_path)) {
        printf("✓ Generated: %s.kt\n", module_id);
        files_written++;
    } else {
        fprintf(stderr, "Warning: Failed to generate Kotlin code for '%s'\n", module_id);
    }

    free(class_name);
    free(package_name);
    free(component_kir_json);

    // Process transitive imports from this component
    if (component_root) {
        cJSON* imports = cJSON_GetObjectItem(component_root, "imports");
        if (imports && cJSON_IsArray(imports)) {
            cJSON* import_item = NULL;
            cJSON_ArrayForEach(import_item, imports) {
                if (!cJSON_IsString(import_item)) continue;
                const char* sub_module_id = cJSON_GetStringValue(import_item);
                if (sub_module_id) {
                    files_written += kotlin_process_module_recursive(sub_module_id, kir_dir,
                                                                     output_dir, base_package,
                                                                     processed);
                }
            }
        }
        cJSON_Delete(component_root);
    }

    return files_written;
}

bool ir_generate_kotlin_code_multi(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: Invalid arguments to ir_generate_kotlin_code_multi\n");
        return false;
    }

    // Set error prefix for this codegen
    codegen_set_error_prefix("Kotlin");

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

    // Get base package from main KIR metadata or use default
    const char* base_package = get_package_name(main_root);

    int files_written = 0;

    // 1. Generate MainActivity.kt from main.kir
    char main_output_path[2048];
    snprintf(main_output_path, sizeof(main_output_path), "%s/MainActivity.kt", output_dir);

    if (ir_generate_kotlin_code_from_string(main_kir_json, main_output_path)) {
        printf("✓ Generated: MainActivity.kt\n");
        files_written++;
    } else {
        fprintf(stderr, "Warning: Failed to generate MainActivity.kt from KIR\n");
    }

    free(main_kir_json);

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
                files_written += kotlin_process_module_recursive(module_id, kir_dir,
                                                                 output_dir, base_package,
                                                                 &processed);
            }
        }
    }

    codegen_processed_modules_free(&processed);
    cJSON_Delete(main_root);

    if (files_written == 0) {
        fprintf(stderr, "Warning: No Kotlin files were generated\n");
        return false;
    }

    printf("✓ Generated %d Kotlin files in %s\n", files_written, output_dir);
    return true;
}
