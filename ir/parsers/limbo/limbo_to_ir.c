/**
 * Limbo to KIR Converter Implementation
 *
 * Converts Limbo AST to Kryon IR (KIR JSON format)
 * Generates KIR v3.0 format JSON for round-trip conversion
 */

#define _POSIX_C_SOURCE 200809L

#include "limbo_to_ir.h"
#include "limbo_parser.h"
#include "../include/ir_serialization.h"
#include "../common/parser_io_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Convert Limbo module to KIR JSON
 */
cJSON* limbo_ast_to_kir(LimboModule* module) {
    if (!module) return NULL;

    // Create root KIR object
    cJSON* kir = cJSON_CreateObject();
    if (!kir) return NULL;

    // Add KIR version
    cJSON_AddStringToObject(kir, "version", "3.0");

    // Add module metadata
    cJSON* metadata = cJSON_CreateObject();
    if (metadata) {
        if (module->name) {
            cJSON_AddStringToObject(metadata, "source_language", "limbo");
            cJSON_AddStringToObject(metadata, "module_name", module->name);
        }
        cJSON_AddItemToObject(kir, "metadata", metadata);
    }

    // Create root component (window/container)
    cJSON* components = cJSON_CreateArray();
    if (components) {
        // Create a default window component
        cJSON* window = cJSON_CreateObject();
        cJSON_AddStringToObject(window, "type", "window");
        cJSON_AddNumberToObject(window, "id", 1);

        cJSON* props = cJSON_CreateObject();
        if (module->name) {
            cJSON_AddStringToObject(props, "title", module->name);
        }
        cJSON_AddNumberToObject(props, "width", 800);
        cJSON_AddNumberToObject(props, "height", 600);
        cJSON_AddItemToObject(window, "properties", props);

        cJSON_AddItemToArray(components, window);
        cJSON_AddItemToObject(kir, "components", components);
    }

    // Add logic (empty for now - would need to parse function bodies)
    cJSON* logic = cJSON_CreateArray();
    cJSON_AddItemToObject(kir, "logic_blocks", logic);

    return kir;
}

/**
 * Parse Limbo source directly to KIR JSON
 */
cJSON* limbo_parse_to_kir(const char* source, size_t source_len) {
    if (!source) return NULL;

    LimboModule* module = limbo_parse(source, source_len);
    if (!module) return NULL;

    cJSON* kir = limbo_ast_to_kir(module);

    limbo_ast_module_free(module);

    return kir;
}

/**
 * Parse Limbo file to KIR JSON
 */
cJSON* limbo_file_to_kir(const char* filepath) {
    if (!filepath) return NULL;

    // Read file
    ParserError error = {0};
    size_t size = 0;
    char* source = parser_read_file(filepath, &size, &error);
    if (!source) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    cJSON* kir = limbo_parse_to_kir(source, size);

    free(source);

    return kir;
}

/**
 * Convert Limbo source to KIR JSON string
 */
char* limbo_to_kir(const char* source, size_t source_len) {
    cJSON* kir = limbo_parse_to_kir(source, source_len);
    if (!kir) return NULL;

    char* json = cJSON_Print(kir);
    cJSON_Delete(kir);

    return json;
}

/**
 * Convert Limbo file to KIR JSON string
 */
char* limbo_file_to_kir_string(const char* filepath) {
    cJSON* kir = limbo_file_to_kir(filepath);
    if (!kir) return NULL;

    char* json = cJSON_Print(kir);
    cJSON_Delete(kir);

    return json;
}
