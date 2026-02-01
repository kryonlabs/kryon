/**
 * @file limbo_parser_adapter.c
 * @brief Limbo Parser Adapter for Parser Interface
 *
 * Adapts the existing Limbo parser to implement the unified ParserInterface.
 * The Limbo parser already has a cJSON* version, so we use that.
 */

#include "../../include/parser_interface.h"
#include "limbo_to_ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Parser Metadata
 * ============================================================================ */

static const char* limbo_extensions[] = {".b", NULL};

static const ParserInfo limbo_parser_info = {
    .name = "Limbo",
    .version = "1.0.0",
    .caps = PARSER_CAP_LAYOUT | PARSER_CAP_STYLING | PARSER_CAP_SCRIPTS,
    .extensions = limbo_extensions
};

/* ============================================================================
 * Parser Interface Implementation
 * ============================================================================ */

static const ParserInfo* limbo_parser_get_info(void) {
    return &limbo_parser_info;
}

static cJSON* limbo_parser_parse_file(const char* filepath) {
    if (!filepath) {
        PARSER_ERROR_FILE_NOT_FOUND("(null)");
        return NULL;
    }

    // limbo_file_to_kir already returns cJSON*
    cJSON* kir = limbo_file_to_kir(filepath);
    if (!kir) {
        PARSER_ERROR_PARSE_FAILED("Limbo", "limbo_file_to_kir failed");
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Parser Interface Export
 * ============================================================================ */

const ParserInterface limbo_parser_interface = {
    .get_info = limbo_parser_get_info,
    .parse_file = limbo_parser_parse_file,
    .validate = NULL,
    .cleanup = NULL
};
