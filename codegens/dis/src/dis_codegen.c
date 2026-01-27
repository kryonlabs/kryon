/**
 * DIS Codegen - Main Implementation
 *
 * Translates KIR (Kryon Intermediate Representation) to DIS bytecode.
 */

#define _POSIX_C_SOURCE 200809L
#include "dis_codegen.h"
#include "module_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// Error handling
static char* error_message = NULL;

void dis_codegen_set_error(const char* fmt, ...) {
    if (error_message) {
        free(error_message);
        error_message = NULL;
    }

    va_list args;
    va_start(args, fmt);

    // Calculate required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size > 0) {
        error_message = (char*)malloc(size + 1);
        if (error_message) {
            vsnprintf(error_message, size + 1, fmt, args);
        }
    }

    va_end(args);
}

const char* dis_codegen_get_error(void) {
    return error_message ? error_message : "Unknown error";
}

void dis_codegen_clear_error(void) {
    if (error_message) {
        free(error_message);
        error_message = NULL;
    }
}

// Forward declaration
static bool translate_kir_to_dis(DISModuleBuilder* builder, const char* kir_json);

bool dis_codegen_generate(const char* kir_path, const char* output_path) {
    return dis_codegen_generate_with_options(kir_path, output_path, NULL);
}

bool dis_codegen_generate_with_options(const char* kir_path,
                                       const char* output_path,
                                       DISCodegenOptions* options) {
    if (!kir_path || !output_path) {
        dis_codegen_set_error("Invalid input/output path");
        return false;
    }

    // Read KIR file
    FILE* f = fopen(kir_path, "r");
    if (!f) {
        dis_codegen_set_error("Failed to open KIR file: %s", kir_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        dis_codegen_set_error("Empty KIR file: %s", kir_path);
        return false;
    }

    char* kir_json = (char*)malloc(file_size + 1);
    if (!kir_json) {
        fclose(f);
        dis_codegen_set_error("Memory allocation failed");
        return false;
    }

    size_t read_size = fread(kir_json, 1, file_size, f);
    kir_json[read_size] = '\0';
    fclose(f);

    // Determine module name from input path
    const char* module_name = options && options->module_name ? options->module_name : "main";

    // Create module builder
    DISModuleBuilder* builder = module_builder_create(module_name, options);
    if (!builder) {
        free(kir_json);
        dis_codegen_set_error("Failed to create module builder");
        return false;
    }

    // Translate KIR to DIS
    bool success = translate_kir_to_dis(builder, kir_json);
    free(kir_json);

    if (!success) {
        module_builder_destroy(builder);
        return false;
    }

    // Write .dis file
    success = module_builder_write_file(builder, output_path);
    module_builder_destroy(builder);

    return success;
}

char* dis_codegen_from_json(const char* kir_json) {
    if (!kir_json) {
        dis_codegen_set_error("NULL KIR JSON");
        return NULL;
    }

    // Create module builder with default options
    DISModuleBuilder* builder = module_builder_create("module", NULL);
    if (!builder) {
        return NULL;
    }

    // Translate to generate DIS bytecode
    if (!translate_kir_to_dis(builder, kir_json)) {
        module_builder_destroy(builder);
        return NULL;
    }

    // For now, return a simple status string
    // TODO: Implement actual DIS assembly output
    char* result = strdup("// DIS bytecode generated successfully\n");
    module_builder_destroy(builder);

    return result;
}

bool dis_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    // For now, delegate to single-file generation
    // TODO: Implement multi-file project support
    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/main.dis", output_dir);
    return dis_codegen_generate(kir_path, output_path);
}

// ============================================================================
// KIR to DIS Translation
// ============================================================================

/**
 * Simple KIR JSON parser for basic structure extraction
 * This is a placeholder - a proper implementation would use a JSON library
 */

typedef struct {
    const char* json;
    size_t pos;
    size_t len;
} KIRJSONParser;

static char skip_whitespace(KIRJSONParser* parser) {
    while (parser->pos < parser->len) {
        char c = parser->json[parser->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return c;
        }
        parser->pos++;
    }
    return '\0';
}

static bool expect_string(KIRJSONParser* parser, const char* str) {
    size_t str_len = strlen(str);
    if (parser->pos + str_len > parser->len) {
        return false;
    }
    if (strncmp(parser->json + parser->pos, str, str_len) == 0) {
        parser->pos += str_len;
        return true;
    }
    return false;
}

/**
 * Parse KIR JSON and translate to DIS bytecode
 */
bool translate_kir_to_dis(DISModuleBuilder* builder, const char* kir_json) {
    if (!kir_json || strlen(kir_json) == 0) {
        dis_codegen_set_error("Empty KIR JSON");
        return false;
    }

    // Basic validation: check if it looks like JSON
    if (kir_json[0] != '{' && kir_json[0] != '[') {
        dis_codegen_set_error("Invalid KIR JSON format");
        return false;
    }

    // TODO: Implement proper KIR parsing and DIS code generation
    // For now, just verify the format is acceptable

    // Set entry point to PC 0
    module_builder_set_entry(builder, 0, 0);

    // Add a main function symbol
    module_builder_add_function(builder, "main", 0);

    return true;
}
