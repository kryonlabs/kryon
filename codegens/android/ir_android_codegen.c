/**
 * Android/Kotlin Code Generator - KIR → Kotlin MainActivity
 *
 * Generates idiomatic Kotlin code with Kryon DSL from KIR JSON files.
 * Pipeline: kry -> kir -> kotlin -> android
 *
 * This codegen reads KIR (JSON intermediate representation) and generates:
 * - MainActivity.kt with Kryon DSL calls
 * - AndroidManifest.xml
 * - build.gradle.kts files
 */

#include "ir_android_codegen.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

static void write_indent(KotlinCodegenContext* ctx) {
    for (int i = 0; i < ctx->indent_level; i++) {
        fprintf(ctx->output, "    ");
    }
}

static void writeln(KotlinCodegenContext* ctx, const char* str) {
    write_indent(ctx);
    fprintf(ctx->output, "%s\n", str);
}

// Parse px value from string like "200.0px"
static float parse_px_value(const char* str) {
    if (!str) return 0.0f;
    float value = 0.0f;
    sscanf(str, "%f", &value);
    return value;
}

// Check if color is transparent (skip rendering)
static bool is_transparent_color(const char* color) {
    return color && (strcmp(color, "#00000000") == 0 || strcmp(color, "transparent") == 0);
}

// Sanitize identifier name (replace hyphens, dots with underscores)
static char* sanitize_identifier(const char* name) {
    if (!name) return NULL;

    char* sanitized = strdup(name);
    for (size_t i = 0; i < strlen(sanitized); i++) {
        if (sanitized[i] == '-' || sanitized[i] == '.') {
            sanitized[i] = '_';
        }
    }
    return sanitized;
}

// ============================================================================
// Component Property Generation
// ============================================================================

static void generate_property(KotlinCodegenContext* ctx, const char* key, cJSON* value) {
    if (!key || !value) return;

    // Width
    if (strcmp(key, "width") == 0 && value->valuestring) {
        float width = parse_px_value(value->valuestring);
        write_indent(ctx);
        fprintf(ctx->output, "width(%.1ff)\n", width);
        return;
    }

    // Height
    if (strcmp(key, "height") == 0 && value->valuestring) {
        float height = parse_px_value(value->valuestring);
        write_indent(ctx);
        fprintf(ctx->output, "height(%.1ff)\n", height);
        return;
    }

    // Background color
    if (strcmp(key, "background") == 0 && value->valuestring) {
        if (!is_transparent_color(value->valuestring)) {
            write_indent(ctx);
            fprintf(ctx->output, "background(\"%s\")\n", value->valuestring);
        }
        return;
    }

    // Text color
    if (strcmp(key, "color") == 0 && value->valuestring) {
        if (!is_transparent_color(value->valuestring)) {
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
            fprintf(ctx->output, "border(%.1ff, \"%s\")\n",
                    (float)width->valuedouble, color->valuestring);
        }

        if (radius) {
            write_indent(ctx);
            fprintf(ctx->output, "borderRadius(%.1ff)\n", (float)radius->valuedouble);
        }
        return;
    }

    // Text alignment
    if (strcmp(key, "textAlign") == 0 && value->valuestring) {
        write_indent(ctx);
        fprintf(ctx->output, "textAlign(\"%s\")\n", value->valuestring);
        return;
    }

    // Corner radius
    if (strcmp(key, "cornerRadius") == 0 && cJSON_IsNumber(value)) {
        write_indent(ctx);
        fprintf(ctx->output, "cornerRadius(%.1ff)\n", (float)value->valuedouble);
        return;
    }
}

// ============================================================================
// Component Generation
// ============================================================================

static void generate_component(KotlinCodegenContext* ctx, cJSON* component);

static void generate_container(KotlinCodegenContext* ctx, cJSON* component) {
    writeln(ctx, "container {");

    ctx->indent_level++;

    // Generate properties
    cJSON* props = cJSON_GetObjectItem(component, "properties");
    if (props) {
        cJSON* prop;
        cJSON_ArrayForEach(prop, props) {
            generate_property(ctx, prop->string, prop);
        }
    }

    // Generate children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child;
        cJSON_ArrayForEach(child, children) {
            generate_component(ctx, child);
        }
    }

    ctx->indent_level--;
    writeln(ctx, "}");
}

static void generate_text(KotlinCodegenContext* ctx, cJSON* component) {
    cJSON* props = cJSON_GetObjectItem(component, "properties");
    cJSON* text_content = props ? cJSON_GetObjectItem(props, "text") : NULL;

    if (text_content && text_content->valuestring) {
        write_indent(ctx);
        fprintf(ctx->output, "text(\"%s\")", text_content->valuestring);
    } else {
        write_indent(ctx);
        fprintf(ctx->output, "text(\"\")");
    }

    writeln(ctx, " {");

    ctx->indent_level++;

    // Generate properties
    if (props) {
        cJSON* prop;
        cJSON_ArrayForEach(prop, props) {
            if (strcmp(prop->string, "text") != 0) {  // Skip text itself
                generate_property(ctx, prop->string, prop);
            }
        }
    }

    ctx->indent_level--;
    writeln(ctx, "}");
}

static void generate_button(KotlinCodegenContext* ctx, cJSON* component) {
    cJSON* props = cJSON_GetObjectItem(component, "properties");
    cJSON* text_content = props ? cJSON_GetObjectItem(props, "text") : NULL;

    if (text_content && text_content->valuestring) {
        write_indent(ctx);
        fprintf(ctx->output, "button(\"%s\")", text_content->valuestring);
    } else {
        write_indent(ctx);
        fprintf(ctx->output, "button(\"\")");
    }

    writeln(ctx, " {");

    ctx->indent_level++;

    // Generate properties
    if (props) {
        cJSON* prop;
        cJSON_ArrayForEach(prop, props) {
            if (strcmp(prop->string, "text") != 0) {  // Skip text itself
                generate_property(ctx, prop->string, prop);
            }
        }
    }

    ctx->indent_level--;
    writeln(ctx, "}");
}

static void generate_row(KotlinCodegenContext* ctx, cJSON* component) {
    writeln(ctx, "row {");

    ctx->indent_level++;

    // Generate properties
    cJSON* props = cJSON_GetObjectItem(component, "properties");
    if (props) {
        cJSON* prop;
        cJSON_ArrayForEach(prop, props) {
            generate_property(ctx, prop->string, prop);
        }
    }

    // Generate children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child;
        cJSON_ArrayForEach(child, children) {
            generate_component(ctx, child);
        }
    }

    ctx->indent_level--;
    writeln(ctx, "}");
}

static void generate_column(KotlinCodegenContext* ctx, cJSON* component) {
    writeln(ctx, "column {");

    ctx->indent_level++;

    // Generate properties
    cJSON* props = cJSON_GetObjectItem(component, "properties");
    if (props) {
        cJSON* prop;
        cJSON_ArrayForEach(prop, props) {
            generate_property(ctx, prop->string, prop);
        }
    }

    // Generate children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child;
        cJSON_ArrayForEach(child, children) {
            generate_component(ctx, child);
        }
    }

    ctx->indent_level--;
    writeln(ctx, "}");
}

static void generate_image(KotlinCodegenContext* ctx, cJSON* component) {
    cJSON* props = cJSON_GetObjectItem(component, "properties");
    cJSON* src = props ? cJSON_GetObjectItem(props, "src") : NULL;

    if (src && src->valuestring) {
        write_indent(ctx);
        fprintf(ctx->output, "image(\"%s\")", src->valuestring);
    } else {
        write_indent(ctx);
        fprintf(ctx->output, "image(\"\")");
    }

    writeln(ctx, " {");

    ctx->indent_level++;

    // Generate properties
    if (props) {
        cJSON* prop;
        cJSON_ArrayForEach(prop, props) {
            if (strcmp(prop->string, "src") != 0) {  // Skip src itself
                generate_property(ctx, prop->string, prop);
            }
        }
    }

    ctx->indent_level--;
    writeln(ctx, "}");
}

static void generate_component(KotlinCodegenContext* ctx, cJSON* component) {
    if (!component) return;

    cJSON* type = cJSON_GetObjectItem(component, "type");
    if (!type || !type->valuestring) return;

    const char* component_type = type->valuestring;

    if (strcmp(component_type, "Container") == 0 ||
        strcmp(component_type, "View") == 0) {
        generate_container(ctx, component);
    } else if (strcmp(component_type, "Text") == 0) {
        generate_text(ctx, component);
    } else if (strcmp(component_type, "Button") == 0) {
        generate_button(ctx, component);
    } else if (strcmp(component_type, "Row") == 0) {
        generate_row(ctx, component);
    } else if (strcmp(component_type, "Column") == 0) {
        generate_column(ctx, component);
    } else if (strcmp(component_type, "Image") == 0) {
        generate_image(ctx, component);
    } else {
        // Unknown component type - generate as container with comment
        char* sanitized = sanitize_identifier(component_type);
        write_indent(ctx);
        fprintf(ctx->output, "// Unknown component type: %s\n", component_type);
        writeln(ctx, "container {");
        ctx->indent_level++;

        // Still try to generate children
        cJSON* children = cJSON_GetObjectItem(component, "children");
        if (children && cJSON_IsArray(children)) {
            cJSON* child;
            cJSON_ArrayForEach(child, children) {
                generate_component(ctx, child);
            }
        }

        ctx->indent_level--;
        writeln(ctx, "}");
        free(sanitized);
    }
}

// ============================================================================
// Main Code Generation
// ============================================================================

static bool generate_kotlin_from_json(cJSON* root_json, FILE* output,
                                      const char* package_name, const char* class_name) {
    if (!root_json || !output) return false;

    KotlinCodegenContext ctx = {
        .output = output,
        .indent_level = 0,
        .root_json = root_json,
        .component_tree = cJSON_GetObjectItem(root_json, "component"),
        .logic_block = cJSON_GetObjectItem(root_json, "logic"),
        .package_name = package_name,
        .class_name = class_name ? class_name : "MainActivity"
    };

    // Write file header
    fprintf(output, "package %s\n\n", package_name);
    fprintf(output, "import com.kryon.KryonActivity\n");
    fprintf(output, "import com.kryon.dsl.*\n\n");
    fprintf(output, "/**\n");
    fprintf(output, " * Auto-generated Kotlin code from KIR\n");
    fprintf(output, " * DO NOT EDIT - This file is generated by Kryon\n");
    fprintf(output, " */\n");
    fprintf(output, "class %s : KryonActivity() {\n\n", ctx.class_name);

    ctx.indent_level = 1;

    // Override buildUI function
    writeln(&ctx, "override fun buildUI() {");
    ctx.indent_level++;

    // Generate component tree
    if (ctx.component_tree) {
        generate_component(&ctx, ctx.component_tree);
    } else {
        writeln(&ctx, "// No component tree found in KIR");
        writeln(&ctx, "container { }");
    }

    ctx.indent_level--;
    writeln(&ctx, "}");

    ctx.indent_level = 0;
    fprintf(output, "}\n");

    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool ir_generate_kotlin_code(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: kir_path and output_path are required\n");
        return false;
    }

    // Read KIR file
    FILE* f = fopen(kir_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open KIR file: %s\n", kir_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* json_str = (char*)malloc(length + 1);
    if (!json_str) {
        fclose(f);
        return false;
    }

    fread(json_str, 1, length, f);
    json_str[length] = '\0';
    fclose(f);

    // Parse JSON
    cJSON* root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return false;
    }

    // Open output file
    FILE* output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", output_path);
        cJSON_Delete(root);
        return false;
    }

    // Generate Kotlin code
    bool success = generate_kotlin_from_json(root, output, "com.kryon.app", "MainActivity");

    fclose(output);
    cJSON_Delete(root);

    return success;
}

bool ir_generate_kotlin_code_from_string(const char* kir_json, const char* output_path) {
    if (!kir_json || !output_path) {
        fprintf(stderr, "Error: kir_json and output_path are required\n");
        return false;
    }

    // Parse JSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return false;
    }

    // Open output file
    FILE* output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", output_path);
        cJSON_Delete(root);
        return false;
    }

    // Generate Kotlin code
    bool success = generate_kotlin_from_json(root, output, "com.kryon.app", "MainActivity");

    fclose(output);
    cJSON_Delete(root);

    return success;
}

bool ir_generate_android_manifest(const char* kir_path, const char* output_path,
                                   const char* app_name) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: kir_path and output_path are required\n");
        return false;
    }

    FILE* output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: Cannot create manifest file: %s\n", output_path);
        return false;
    }

    fprintf(output, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    fprintf(output, "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\">\n");

    if (app_name) {
        fprintf(output, "    <application\n");
        fprintf(output, "        android:allowBackup=\"true\"\n");
        fprintf(output, "        android:label=\"%s\"\n", app_name);
        fprintf(output, "        android:theme=\"@style/Theme.Kryon\">\n");
        fprintf(output, "        <activity\n");
        fprintf(output, "            android:name=\".MainActivity\"\n");
        fprintf(output, "            android:exported=\"true\">\n");
        fprintf(output, "            <intent-filter>\n");
        fprintf(output, "                <action android:name=\"android.intent.action.MAIN\" />\n");
        fprintf(output, "                <category android:name=\"android.intent.category.LAUNCHER\" />\n");
        fprintf(output, "            </intent-filter>\n");
        fprintf(output, "        </activity>\n");
        fprintf(output, "    </application>\n");
    }

    fprintf(output, "</manifest>\n");

    fclose(output);
    return true;
}

bool ir_generate_gradle_build(const char* kir_path, const char* output_path,
                               const char* namespace) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: kir_path and output_path are required\n");
        return false;
    }

    FILE* output = fopen(output_path, "w");
    if (!output) {
        fprintf(stderr, "Error: Cannot create Gradle file: %s\n", output_path);
        return false;
    }

    fprintf(output, "plugins {\n");
    fprintf(output, "    id(\"com.android.application\")\n");
    fprintf(output, "    id(\"org.jetbrains.kotlin.android\")\n");
    fprintf(output, "}\n\n");

    fprintf(output, "android {\n");
    if (namespace) {
        fprintf(output, "    namespace = \"%s\"\n", namespace);
    }
    fprintf(output, "    compileSdk = 33\n\n");
    fprintf(output, "    defaultConfig {\n");
    fprintf(output, "        applicationId = \"%s\"\n", namespace ? namespace : "com.kryon.app");
    fprintf(output, "        minSdk = 24\n");
    fprintf(output, "        targetSdk = 33\n");
    fprintf(output, "    }\n\n");
    fprintf(output, "    compileOptions {\n");
    fprintf(output, "        sourceCompatibility = JavaVersion.VERSION_17\n");
    fprintf(output, "        targetCompatibility = JavaVersion.VERSION_17\n");
    fprintf(output, "    }\n\n");
    fprintf(output, "    kotlinOptions {\n");
    fprintf(output, "        jvmTarget = \"17\"\n");
    fprintf(output, "    }\n");
    fprintf(output, "}\n\n");

    fprintf(output, "dependencies {\n");
    fprintf(output, "    implementation(project(\":bindings:kotlin\"))\n");
    fprintf(output, "    implementation(\"androidx.core:core-ktx:1.12.0\")\n");
    fprintf(output, "    implementation(\"androidx.appcompat:appcompat:1.6.1\")\n");
    fprintf(output, "}\n");

    fclose(output);
    return true;
}

bool ir_generate_android_project(const char* kir_path, const char* output_dir,
                                  const char* package_name, const char* app_name) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: kir_path and output_dir are required\n");
        return false;
    }

    // Create output directory
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
    system(mkdir_cmd);

    // Generate MainActivity.kt
    char main_activity_path[1024];
    snprintf(main_activity_path, sizeof(main_activity_path), "%s/MainActivity.kt", output_dir);
    if (!ir_generate_kotlin_code(kir_path, main_activity_path)) {
        fprintf(stderr, "Error: Failed to generate MainActivity.kt\n");
        return false;
    }

    // Generate AndroidManifest.xml
    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s/AndroidManifest.xml", output_dir);
    if (!ir_generate_android_manifest(kir_path, manifest_path, app_name)) {
        fprintf(stderr, "Error: Failed to generate AndroidManifest.xml\n");
        return false;
    }

    // Generate build.gradle.kts
    char gradle_path[1024];
    snprintf(gradle_path, sizeof(gradle_path), "%s/build.gradle.kts", output_dir);
    if (!ir_generate_gradle_build(kir_path, gradle_path, package_name)) {
        fprintf(stderr, "Error: Failed to generate build.gradle.kts\n");
        return false;
    }

    printf("✓ Generated Android project in %s/\n", output_dir);
    printf("  - MainActivity.kt\n");
    printf("  - AndroidManifest.xml\n");
    printf("  - build.gradle.kts\n");

    return true;
}
