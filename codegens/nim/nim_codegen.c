/**
 * Nim Code Generator
 * Generates Nim DSL code from KIR v3.0 JSON
 */

#define _POSIX_C_SOURCE 200809L

#include "nim_codegen.h"
#include "../../ir/ir_serialization.h"
#include "../../third_party/cJSON/cJSON.h"
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
                        // Look for Nim source first
                        cJSON* source = NULL;
                        cJSON_ArrayForEach(source, sources) {
                            cJSON* lang = cJSON_GetObjectItem(source, "language");
                            if (lang && cJSON_IsString(lang) &&
                                strcmp(cJSON_GetStringValue(lang), "nim") == 0) {
                                cJSON* code = cJSON_GetObjectItem(source, "source");
                                if (code && cJSON_IsString(code)) {
                                    return strdup(cJSON_GetStringValue(code));
                                }
                            }
                        }

                        // Fall back to translating from kry
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

                                // Translate: print(...) -> echo ...
                                if (strstr(cleaned, "print(") != NULL) {
                                    const char* start = strstr(cleaned, "print(");
                                    const char* args_start = start + 6;
                                    const char* args_end = strchr(args_start, ')');
                                    if (args_end) {
                                        size_t args_len = args_end - args_start;
                                        char args[512];
                                        strncpy(args, args_start, args_len);
                                        args[args_len] = '\0';

                                        char buffer[1024];
                                        snprintf(buffer, sizeof(buffer), "echo %s", args);
                                        return strdup(buffer);
                                    }
                                }

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

// Generate Nim component code recursively
static bool generate_nim_component(cJSON* node, char** buffer, size_t* size, size_t* capacity, int indent, EventHandlerContext* handler_ctx) {
    if (!node || !cJSON_IsObject(node)) return true;

    // Get component type
    cJSON* type_item = cJSON_GetObjectItem(node, "type");
    if (!type_item) return true;
    const char* type = type_item->valuestring;

    // Get component ID for event handler lookup
    int component_id = -1;
    cJSON* id_item = cJSON_GetObjectItem(node, "id");
    if (id_item && cJSON_IsNumber(id_item)) {
        component_id = (int)cJSON_GetNumberValue(id_item);
    }

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
            strcmp(key, "text") == 0 ||
            strcmp(key, "events") == 0) {
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

    // Add event handlers if available
    if (component_id >= 0 && handler_ctx) {
        char* click_handler = find_event_handler(handler_ctx, component_id, "click");
        if (click_handler) {
            for (int i = 0; i < indent + 1; i++) {
                append_string(buffer, size, capacity, "  ");
            }
            append_fmt(buffer, size, capacity, "onClick = proc() =\n");
            for (int i = 0; i < indent + 2; i++) {
                append_string(buffer, size, capacity, "  ");
            }
            append_fmt(buffer, size, capacity, "%s\n", click_handler);
            free(click_handler);
        }
    }

    // Process children
    cJSON* children = cJSON_GetObjectItem(node, "children");
    if (children && cJSON_IsArray(children)) {
        // Add blank line before children for readability
        append_string(buffer, size, capacity, "\n");

        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            if (!generate_nim_component(child, buffer, size, capacity, indent + 1, handler_ctx)) {
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

    // Extract logic_block for event handlers
    EventHandlerContext handler_ctx = {0};
    cJSON* logic_block = cJSON_GetObjectItem(root_json, "logic_block");
    if (logic_block && cJSON_IsObject(logic_block)) {
        handler_ctx.functions = cJSON_GetObjectItem(logic_block, "functions");
        handler_ctx.event_bindings = cJSON_GetObjectItem(logic_block, "event_bindings");
    }

    // Check if this is KIR v3.0 format (has root field)
    cJSON* root_comp = cJSON_GetObjectItem(root_json, "root");
    if (root_comp) {
        // KIR v3.0 format - use root component
        if (!generate_nim_component(root_comp, &buffer, &size, &capacity, 1, &handler_ctx)) {
            free(buffer);
            cJSON_Delete(root_json);
            return NULL;
        }
    } else {
        // Legacy format or direct component - treat whole JSON as component
        if (!generate_nim_component(root_json, &buffer, &size, &capacity, 1, &handler_ctx)) {
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

    size_t bytes_read = fread(kir_json, 1, file_size, kir_file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read complete KIR file\n");
        free(kir_json);
        fclose(kir_file);
        return false;
    }
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
