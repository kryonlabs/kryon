/**
 * @file codegen_io.c
 * @brief Codegen Input/Output Abstraction Layer Implementation
 *
 * Eliminates repetitive file I/O patterns across 15+ codegen files.
 */

#define _POSIX_C_SOURCE 200809L

#include "codegen_io.h"
#include "codegen_common.h"
#include "../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Thread-local error prefix for I/O operations
static __thread char g_io_error_prefix[32] = "IO";
static __thread char g_last_error[256] = {0};

/* ============================================================================
 * Input Loading Functions
 * ============================================================================ */

CodegenInput* codegen_load_input(const char* kir_path) {
    if (!kir_path) {
        snprintf(g_last_error, sizeof(g_last_error),
                "NULL KIR path provided");
        fprintf(stderr, "[%s] Error: %s\n", g_io_error_prefix, g_last_error);
        return NULL;
    }

    // Read KIR file
    size_t size;
    char* kir_json = codegen_read_kir_file(kir_path, &size);
    if (!kir_json) {
        // Error already reported by codegen_read_kir_file
        snprintf(g_last_error, sizeof(g_last_error),
                "Failed to read KIR file: %s", kir_path);
        return NULL;
    }

    // Parse KIR JSON
    cJSON* root = codegen_parse_kir_json(kir_json);
    if (!root) {
        snprintf(g_last_error, sizeof(g_last_error),
                "Failed to parse KIR JSON from: %s", kir_path);
        codegen_error("%s", g_last_error);
        free(kir_json);
        return NULL;
    }

    // Allocate input structure
    CodegenInput* input = calloc(1, sizeof(CodegenInput));
    if (!input) {
        snprintf(g_last_error, sizeof(g_last_error),
                "Memory allocation failed for input structure");
        codegen_error("%s", g_last_error);
        cJSON_Delete(root);
        free(kir_json);
        return NULL;
    }

    // Initialize input structure
    input->root = root;
    input->json_string = kir_json;
    input->size = size;
    input->path = kir_path;

    return input;
}

CodegenInput* codegen_load_from_string(const char* json_string) {
    if (!json_string) {
        snprintf(g_last_error, sizeof(g_last_error),
                "NULL JSON string provided");
        codegen_error("%s", g_last_error);
        return NULL;
    }

    // Parse JSON string
    cJSON* root = codegen_parse_kir_json(json_string);
    if (!root) {
        snprintf(g_last_error, sizeof(g_last_error),
                "Failed to parse JSON string");
        codegen_error("%s", g_last_error);
        return NULL;
    }

    // Allocate input structure
    CodegenInput* input = calloc(1, sizeof(CodegenInput));
    if (!input) {
        snprintf(g_last_error, sizeof(g_last_error),
                "Memory allocation failed for input structure");
        codegen_error("%s", g_last_error);
        cJSON_Delete(root);
        return NULL;
    }

    // Initialize input structure
    // Note: We duplicate the string so it can be freed consistently
    input->root = root;
    input->json_string = strdup(json_string);
    input->size = strlen(json_string);
    input->path = "<string>";

    if (!input->json_string) {
        snprintf(g_last_error, sizeof(g_last_error),
                "Memory allocation failed for JSON string");
        codegen_error("%s", g_last_error);
        cJSON_Delete(root);
        free(input);
        return NULL;
    }

    return input;
}

void codegen_free_input(CodegenInput* input) {
    if (!input) {
        return;
    }

    // Free cJSON root
    if (input->root) {
        cJSON_Delete(input->root);
    }

    // Free JSON string
    if (input->json_string) {
        free(input->json_string);
    }

    // Free the structure itself
    free(input);
}

/* ============================================================================
 * Input Validation Helpers
 * ============================================================================ */

bool codegen_input_is_valid(const CodegenInput* input) {
    return input && input->root && input->json_string;
}

cJSON* codegen_input_get_root_component(CodegenInput* input) {
    if (!codegen_input_is_valid(input)) {
        return NULL;
    }

    return codegen_extract_root_component(input->root);
}

cJSON* codegen_input_get_logic_block(CodegenInput* input) {
    if (!codegen_input_is_valid(input)) {
        return NULL;
    }

    return codegen_extract_logic_block(input->root);
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

void codegen_io_set_error_prefix(const char* prefix) {
    if (prefix) {
        snprintf(g_io_error_prefix, sizeof(g_io_error_prefix), "%s", prefix);
    }
}

const char* codegen_io_get_last_error(void) {
    return g_last_error[0] ? g_last_error : NULL;
}
