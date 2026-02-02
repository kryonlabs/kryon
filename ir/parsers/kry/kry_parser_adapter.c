/**
 * @file kry_parser_adapter.c
 * @brief KRY Parser Adapter for Parser Interface
 *
 * Adapts the existing KRY parser to implement the unified ParserInterface.
 * The KRY parser returns JSON strings, so we convert to cJSON.
 */

#include "../../include/parser_interface.h"
#include "kry_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Parser Metadata
 * ============================================================================ */

static const char* kry_extensions[] = {".kry", NULL};

static const ParserInfo kry_parser_info = {
    .name = "KRY",
    .version = "alpha",
    .caps = PARSER_CAP_MULTI_FILE | PARSER_CAP_LAYOUT | PARSER_CAP_STYLING | PARSER_CAP_SCRIPTS,
    .extensions = kry_extensions
};

/* ============================================================================
 * Parser Interface Implementation
 * ============================================================================ */

static const ParserInfo* kry_parser_get_info(void) {
    return &kry_parser_info;
}

static cJSON* kry_parser_parse_file(const char* filepath) {
    if (!filepath) {
        PARSER_ERROR_FILE_NOT_FOUND("(null)");
        return NULL;
    }

    // Use ir_kry_to_kir_file which reads file and returns JSON string
    char* kir_json = ir_kry_to_kir_file(filepath);
    if (!kir_json) {
        PARSER_ERROR_PARSE_FAILED("KRY", "ir_kry_to_kir_file failed");
        return NULL;
    }

    // Parse JSON string to cJSON
    cJSON* kir = cJSON_Parse(kir_json);
    free(kir_json);

    if (!kir) {
        PARSER_ERROR_INVALID_JSON("KRY");
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Parser Interface Export
 * ============================================================================ */

const ParserInterface kry_parser_interface = {
    .get_info = kry_parser_get_info,
    .parse_file = kry_parser_parse_file,
    .validate = NULL,
    .cleanup = NULL
};
