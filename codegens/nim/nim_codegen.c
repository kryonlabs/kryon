/**
 * Nim Code Generator
 * Generates Nim DSL code from KIR v3.0 JSON
 */

#include "nim_codegen.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// Helper to append string to buffer with reallocation
static bool append_string(char** buffer, size_t* size, size_t* capacity, const char* str) {
    size_t len = strlen(str);
    if (*size + len >= *capacity) {
        *capacity *= 2;
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

// Format a JSON value as Nim literal
static void format_nim_value(char* output, size_t size, cJSON* value) {
    if (cJSON_IsString(value)) {
        snprintf(output, size, "\"%s\"", value->valuestring);
    } else if (cJSON_IsNumber(value)) {
        if (value->valuedouble == (int)value->valuedouble) {
            snprintf(output, size, "%d", (int)value->valuedouble);
        } else {
            snprintf(output, size, "%.2f", value->valuedouble);
        }
    } else if (cJSON_IsBool(value)) {
        snprintf(output, size, "%s", cJSON_IsTrue(value) ? "true" : "false");
    } else if (cJSON_IsNull(value)) {
        snprintf(output, size, "nil");
    } else {
        snprintf(output, size, "\"???\"");
    }
}

// Generate Nim component code recursively
static bool generate_nim_component(cJSON* node, char** buffer, size_t* size, size_t* capacity, int indent) {
    if (!node || !cJSON_IsObject(node)) return true;

    // Get component type
    cJSON* type_item = cJSON_GetObjectItem(node, "type");
    if (!type_item) return true;
    const char* type = type_item->valuestring;

    // Generate indentation
    for (int i = 0; i < indent; i++) {
        append_string(buffer, size, capacity, "  ");
    }

    // Write component type
    append_fmt(buffer, size, capacity, "%s:\n", type);

    // Get text content if present (for Text components)
    cJSON* text_item = cJSON_GetObjectItem(node, "text");

    // Iterate through all properties (KIR v3.0 has flattened props)
    cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, node) {
        const char* key = prop->string;

        // Skip special keys
        if (strcmp(key, "type") == 0 ||
            strcmp(key, "id") == 0 ||
            strcmp(key, "children") == 0 ||
            strcmp(key, "text") == 0) {
            continue;
        }

        // Write property
        for (int i = 0; i < indent + 1; i++) {
            append_string(buffer, size, capacity, "  ");
        }

        char value_str[512];
        format_nim_value(value_str, sizeof(value_str), prop);
        append_fmt(buffer, size, capacity, "%s = %s\n", key, value_str);
    }

    // Add text property if present
    if (text_item && cJSON_IsString(text_item)) {
        for (int i = 0; i < indent + 1; i++) {
            append_string(buffer, size, capacity, "  ");
        }
        append_fmt(buffer, size, capacity, "text = \"%s\"\n", text_item->valuestring);
    }

    // Process children
    cJSON* children = cJSON_GetObjectItem(node, "children");
    if (children && cJSON_IsArray(children)) {
        // Add blank line before children for readability
        append_string(buffer, size, capacity, "\n");

        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            if (!generate_nim_component(child, buffer, size, capacity, indent + 1)) {
                return false;
            }
        }
    }

    return true;
}

// Generate Nim DSL code from KIR JSON
char* nim_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    // Parse KIR JSON
    cJSON* root_json = cJSON_Parse(kir_json);
    if (!root_json) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return NULL;
    }

    // Allocate output buffer
    size_t capacity = 8192;
    size_t size = 0;
    char* buffer = malloc(capacity);
    if (!buffer) {
        cJSON_Delete(root_json);
        return NULL;
    }
    buffer[0] = '\0';

    // Write header
    append_string(&buffer, &size, &capacity, "## Generated from KIR\n");
    append_string(&buffer, &size, &capacity, "## Kryon Nim DSL Code Generator\n\n");
    append_string(&buffer, &size, &capacity, "import kryon_dsl\n\n");
    append_string(&buffer, &size, &capacity, "# =============================================================================\n");
    append_string(&buffer, &size, &capacity, "# Application\n");
    append_string(&buffer, &size, &capacity, "# =============================================================================\n\n");
    append_string(&buffer, &size, &capacity, "let app = kryonApp:\n");

    // Check if this is KIR v3.0 format (has root field)
    cJSON* root_comp = cJSON_GetObjectItem(root_json, "root");
    if (root_comp) {
        // KIR v3.0 format - use root component
        if (!generate_nim_component(root_comp, &buffer, &size, &capacity, 1)) {
            free(buffer);
            cJSON_Delete(root_json);
            return NULL;
        }
    } else {
        // Legacy format or direct component - treat whole JSON as component
        if (!generate_nim_component(root_json, &buffer, &size, &capacity, 1)) {
            free(buffer);
            cJSON_Delete(root_json);
            return NULL;
        }
    }

    // Add footer
    append_string(&buffer, &size, &capacity, "\n");
    append_string(&buffer, &size, &capacity, "# Run the application\n");
    append_string(&buffer, &size, &capacity, "when isMainModule:\n");
    append_string(&buffer, &size, &capacity, "  app.run()\n");

    cJSON_Delete(root_json);
    return buffer;
}

// Generate Nim code from KIR file
bool nim_codegen_generate(const char* kir_path, const char* output_path) {
    // Read KIR file
    FILE* kir_file = fopen(kir_path, "r");
    if (!kir_file) {
        fprintf(stderr, "Error: Could not open KIR file: %s\n", kir_path);
        return false;
    }

    fseek(kir_file, 0, SEEK_END);
    long file_size = ftell(kir_file);
    fseek(kir_file, 0, SEEK_SET);

    char* kir_json = malloc(file_size + 1);
    if (!kir_json) {
        fclose(kir_file);
        return false;
    }

    fread(kir_json, 1, file_size, kir_file);
    kir_json[file_size] = '\0';
    fclose(kir_file);

    // Generate Nim code
    char* nim_code = nim_codegen_from_json(kir_json);
    free(kir_json);

    if (!nim_code) {
        return false;
    }

    // Write output file
    FILE* output_file = fopen(output_path, "w");
    if (!output_file) {
        fprintf(stderr, "Error: Could not write output file: %s\n", output_path);
        free(nim_code);
        return false;
    }

    fprintf(output_file, "%s", nim_code);
    fclose(output_file);
    free(nim_code);

    return true;
}

// Generate Nim code with options
bool nim_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        NimCodegenOptions* options) {
    // Generate base Nim code
    bool success = nim_codegen_generate(kir_path, output_path);
    if (!success) return false;

    // Apply post-processing options
    if (options && options->format) {
        // Try to format with nimpretty
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "nimpretty \"%s\" 2>/dev/null", output_path);
        int result = system(cmd);
        if (result == 0) {
            printf("  Formatted with nimpretty\n");
        }
    }

    return true;
}
