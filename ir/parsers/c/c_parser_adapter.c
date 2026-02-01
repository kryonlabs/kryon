/**
 * @file c_parser_adapter.c
 * @brief C Parser Adapter for Parser Interface
 *
 * Adapts the existing C parser to implement the unified ParserInterface.
 * The C parser has both file and string versions.
 */

#include "../../include/parser_interface.h"
#include "c_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Parser Metadata
 * ============================================================================ */

static const char* c_extensions[] = {".c", ".h", NULL};

static const ParserInfo c_parser_info = {
    .name = "C",
    .version = "1.0.0",
    .caps = PARSER_CAP_LAYOUT | PARSER_CAP_SCRIPTS,
    .extensions = c_extensions
};

/* ============================================================================
 * Parser Interface Implementation
 * ============================================================================ */

static const ParserInfo* c_parser_get_info(void) {
    return &c_parser_info;
}

static cJSON* c_parser_parse_file(const char* filepath) {
    if (!filepath) {
        PARSER_ERROR_FILE_NOT_FOUND("(null)");
        return NULL;
    }

    // ir_c_file_to_kir returns JSON string
    char* kir_json = ir_c_file_to_kir(filepath);
    if (!kir_json) {
        PARSER_ERROR_PARSE_FAILED("C", "ir_c_file_to_kir failed");
        return NULL;
    }

    // Parse JSON string to cJSON
    cJSON* kir = cJSON_Parse(kir_json);
    free(kir_json);

    if (!kir) {
        PARSER_ERROR_INVALID_JSON("C");
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Parser Interface Export
 * ============================================================================ */

const ParserInterface c_parser_interface = {
    .get_info = c_parser_get_info,
    .parse_file = c_parser_parse_file,
    .validate = NULL,
    .cleanup = NULL
};
