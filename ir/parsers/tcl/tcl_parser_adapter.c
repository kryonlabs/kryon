/**
 * @file tcl_parser_adapter.c
 * @brief Tcl/Tk Parser Adapter for Parser Interface
 *
 * Adapts the existing Tcl/Tk parser to implement the unified ParserInterface.
 * The Tcl parser already returns cJSON*, so it's a direct wrapper.
 */

#include "../../include/parser_interface.h"
#include "tcl_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Parser Metadata
 * ============================================================================ */

static const char* tcl_extensions[] = {".tcl", NULL};

static const ParserInfo tcl_parser_info = {
    .name = "Tcl",
    .version = "1.0.0",
    .caps = PARSER_CAP_LAYOUT | PARSER_CAP_STYLING | PARSER_CAP_SCRIPTS,
    .extensions = tcl_extensions
};

/* ============================================================================
 * Parser Interface Implementation
 * ============================================================================ */

static const ParserInfo* tcl_parser_get_info(void) {
    return &tcl_parser_info;
}

static cJSON* tcl_parser_parse_file(const char* filepath) {
    if (!filepath) {
        PARSER_ERROR_FILE_NOT_FOUND("(null)");
        return NULL;
    }

    // tcl_parse_file_to_kir already returns cJSON*
    cJSON* kir = tcl_parse_file_to_kir(filepath);
    if (!kir) {
        PARSER_ERROR_PARSE_FAILED("Tcl", "tcl_parse_file_to_kir failed");
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Parser Interface Export
 * ============================================================================ */

const ParserInterface tcl_parser_interface = {
    .get_info = tcl_parser_get_info,
    .parse_file = tcl_parser_parse_file,
    .validate = NULL,
    .cleanup = NULL
};
