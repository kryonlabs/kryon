/**
 * @file html_parser_adapter.c
 * @brief HTML Parser Adapter for Parser Interface
 *
 * Adapts the existing HTML parser to implement the unified ParserInterface.
 * The HTML parser returns JSON strings, so we convert to cJSON.
 */

#include "../../include/parser_interface.h"
#include "html_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Parser Metadata
 * ============================================================================ */

static const char* html_extensions[] = {".html", ".htm", NULL};

static const ParserInfo html_parser_info = {
    .name = "HTML",
    .version = "1.0.0",
    .caps = PARSER_CAP_LAYOUT | PARSER_CAP_STYLING,
    .extensions = html_extensions
};

/* ============================================================================
 * Parser Interface Implementation
 * ============================================================================ */

static const ParserInfo* html_parser_get_info(void) {
    return &html_parser_info;
}

static cJSON* html_parser_parse_file(const char* filepath) {
    if (!filepath) {
        PARSER_ERROR_FILE_NOT_FOUND("(null)");
        return NULL;
    }

    // ir_html_file_to_kir returns JSON string
    char* kir_json = ir_html_file_to_kir(filepath);
    if (!kir_json) {
        PARSER_ERROR_PARSE_FAILED("HTML", "ir_html_file_to_kir failed");
        return NULL;
    }

    // Parse JSON string to cJSON
    cJSON* kir = cJSON_Parse(kir_json);
    free(kir_json);

    if (!kir) {
        PARSER_ERROR_INVALID_JSON("HTML");
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Parser Interface Export
 * ============================================================================ */

const ParserInterface html_parser_interface = {
    .get_info = html_parser_get_info,
    .parse_file = html_parser_parse_file,
    .validate = NULL,
    .cleanup = NULL
};
