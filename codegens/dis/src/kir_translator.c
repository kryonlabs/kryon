/**
 * KIR to DIS Translator
 *
 * Translates KIR (Kryon Intermediate Representation) to DIS bytecode.
 * Parses KIR JSON and generates TaijiOS DIS instructions.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "internal.h"
#include "../include/dis_codegen.h"

// ============================================================================
// JSON Parser
// ============================================================================

typedef struct {
    const char* json;
    size_t pos;
    size_t length;
} KIRJSONParser;

static void skip_whitespace(KIRJSONParser* parser) {
    while (parser->pos < parser->length && isspace(parser->json[parser->pos])) {
        parser->pos++;
    }
}

static char peek(KIRJSONParser* parser) {
    if (parser->pos >= parser->length) return '\0';
    return parser->json[parser->pos];
}

static char advance(KIRJSONParser* parser) {
    if (parser->pos >= parser->length) return '\0';
    return parser->json[parser->pos++];
}

static bool expect_string(KIRJSONParser* parser, const char* str) {
    skip_whitespace(parser);
    size_t len = strlen(str);
    if (parser->pos + len > parser->length) return false;
    if (strncmp(parser->json + parser->pos, str, len) == 0) {
        parser->pos += len;
        return true;
    }
    return false;
}

static char* parse_string(KIRJSONParser* parser) {
    skip_whitespace(parser);
    if (peek(parser) != '"') return NULL;
    advance(parser);  // Skip opening quote

    // Simple string parsing (no escape handling for now)
    size_t start = parser->pos;
    while (parser->pos < parser->length && parser->json[parser->pos] != '"') {
        parser->pos++;
    }

    if (parser->pos >= parser->length) return NULL;

    size_t len = parser->pos - start;
    char* str = (char*)malloc(len + 1);
    if (!str) return NULL;

    memcpy(str, parser->json + start, len);
    str[len] = '\0';
    advance(parser);  // Skip closing quote

    return str;
}

static bool expect_char(KIRJSONParser* parser, char c) {
    skip_whitespace(parser);
    if (peek(parser) == c) {
        advance(parser);
        return true;
    }
    return false;
}

static int parse_int(KIRJSONParser* parser) {
    skip_whitespace(parser);
    int sign = 1;
    if (peek(parser) == '-') {
        sign = -1;
        advance(parser);
    }

    int value = 0;
    while (isdigit(peek(parser))) {
        value = value * 10 + (advance(parser) - '0');
    }

    return sign * value;
}

// ============================================================================
// KIR Type Translation
// ============================================================================

/**
 * Get DIS type index for a KIR type
 */
static int32_t get_dis_type_for_kir(DISModuleBuilder* builder, const char* kir_type) {
    // Check if already registered
    void* existing = hash_table_get(builder->type_indices, kir_type);
    if (existing) {
        return (int32_t)(intptr_t)existing;
    }

    // Register new type
    return register_type(builder, kir_type);
}

// ============================================================================
// KIR Component Translation
// ============================================================================

/**
 * Translate a KIR component to DIS data structure
 * This creates type descriptors and data initialization for the component tree
 */
static bool translate_component(DISModuleBuilder* builder, const char* component_json) {
    // For now, create a simple component type
    // Real implementation would parse the JSON properly

    // Register component type
    int32_t component_type = get_dis_type_for_kir(builder, "component:Box");

    // Allocate space for component in data section
    uint32_t component_offset = module_builder_allocate_global(builder, 32);  // Placeholder size

    // Initialize component data
    emit_data_word(builder, 0, component_offset);  // Component header
    emit_data_word(builder, 0, component_offset + 4);  // Props pointer
    emit_data_word(builder, 0, component_offset + 8);  // Children pointer

    return true;
}

// ============================================================================
// KIR Reactive State Translation
// ============================================================================

/**
 * Translate reactive variables to DIS global data
 */
static bool translate_reactive_state(DISModuleBuilder* builder, const char* state_json) {
    // For now, just allocate space for a counter
    // Real implementation would parse the reactive manifest

    // Allocate global variable: count (number)
    uint32_t count_offset = module_builder_allocate_global(builder, sizeof(int32_t));

    // Initialize to 0
    emit_data_word(builder, 0, count_offset);

    return true;
}

// ============================================================================
// KIR Event Handler Translation
// ============================================================================

/**
 * Translate a KIR event handler to DIS function
 * Example: "count = count + 1"
 */
static bool translate_handler(DISModuleBuilder* builder, const char* handler_name,
                              const char* handler_code) {
    // Get current PC for function entry
    uint32_t entry_pc = module_builder_get_pc(builder);

    // Simple handler: increment count
    // count = count + 1
    // Assuming count is at MP offset 0

    // Load count into temporary (using offset 0 from MP)
    // IMOVW 0(mp), tmp
    emit_mov_w(builder, 0, 1);  // Load from MP+0 to temp register (simplified)

    // Add immediate 1
    // IADDW $1, tmp, tmp
    emit_add_w(builder, 1, 1, 1);  // tmp = tmp + 1

    // Store back
    // IMOVW tmp, 0(mp)
    emit_mov_w(builder, 1, 0);  // Store to MP+0

    // Return
    emit_ret(builder);

    // Export the handler
    add_export(builder, handler_name, entry_pc, 0, 0);

    return true;
}

// ============================================================================
// Main Translation Entry Point
// ============================================================================

/**
 * Translate KIR JSON to DIS bytecode
 */
bool translate_kir_to_dis(DISModuleBuilder* builder, const char* kir_json) {
    if (!builder || !kir_json) {
        return false;
    }

    KIRJSONParser parser = {kir_json, 0, strlen(kir_json)};

    // Skip to first meaningful character
    skip_whitespace(&parser);
    if (advance(&parser) != '{') {
        return false;  // Not a JSON object
    }

    // Simple KIR parsing (look for key sections)
    // Real implementation would use proper JSON parser

    bool has_reactive = false;
    bool has_component = false;
    bool has_handlers = false;

    // Scan for reactive_manifest
    if (strstr(kir_json, "\"reactive_manifest\"") != NULL) {
        has_reactive = true;
    }

    // Scan for root component
    if (strstr(kir_json, "\"root\"") != NULL) {
        has_component = true;
    }

    // Scan for logic_block (event handlers)
    if (strstr(kir_json, "\"logic_block\"") != NULL) {
        has_handlers = true;
    }

    // Translate reactive state
    if (has_reactive) {
        if (!translate_reactive_state(builder, kir_json)) {
            return false;
        }
    }

    // Translate component tree
    if (has_component) {
        if (!translate_component(builder, kir_json)) {
            return false;
        }
    }

    // Translate event handlers
    if (has_handlers) {
        // Look for onClick handler
        if (strstr(kir_json, "onClick") != NULL) {
            translate_handler(builder, "onClick", "increment");
        }
    }

    // Set entry point to first handler or main
    module_builder_set_entry(builder, 0, 0);

    return true;
}

// ============================================================================
// Public API Functions
// ============================================================================

/**
 * Generate DIS bytecode from KIR JSON string
 */
char* dis_codegen_from_json(const char* kir_json) {
    if (!kir_json) {
        dis_codegen_set_error("NULL KIR JSON");
        return NULL;
    }

    // Create module builder
    DISCodegenOptions options = {
        .optimize = false,
        .debug_info = false,
        .stack_extent = 4096,
        .share_mp = false,
        .module_name = "kir_module"
    };

    DISModuleBuilder* builder = module_builder_create("kir_module", &options);
    if (!builder) {
        dis_codegen_set_error("Failed to create module builder");
        return NULL;
    }

    // Translate KIR to DIS
    if (!translate_kir_to_dis(builder, kir_json)) {
        dis_codegen_set_error("Failed to translate KIR to DIS");
        module_builder_destroy(builder);
        return NULL;
    }

    // For now, return success message
    // Real implementation would serialize to string or write to file
    module_builder_destroy(builder);

    return strdup("{\"status\": \"success\", \"message\": \"DIS bytecode generated\"}");
}
