/**
 * Kotlin Code Generator - KIR → Kotlin MainActivity
 *
 * Generates idiomatic Kotlin code with Kryon DSL from KIR JSON files
 */

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
