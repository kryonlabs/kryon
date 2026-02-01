/**
 * @file lua_parser_adapter.c
 * @brief Lua Parser Adapter for Parser Interface
 *
 * Adapts the existing Lua parser to implement the unified ParserInterface.
 * The Lua parser executes Lua DSL code with Kryon bindings to generate KIR.
 */

#include "../../include/parser_interface.h"
#include "lua_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Parser Metadata
 * ============================================================================ */

static const char* lua_extensions[] = {".lua", NULL};

static const ParserInfo lua_parser_info = {
    .name = "lua",
    .version = "1.0.0",
    .caps = PARSER_CAP_MULTI_FILE | PARSER_CAP_LAYOUT | PARSER_CAP_SCRIPTS,
    .extensions = lua_extensions
};

/* ============================================================================
 * Parser Interface Implementation
 * ============================================================================ */

static const ParserInfo* lua_parser_get_info(void) {
    return &lua_parser_info;
}

static cJSON* lua_parser_parse_file(const char* filepath) {
    if (!filepath) {
        PARSER_ERROR_FILE_NOT_FOUND("(null)");
        return NULL;
    }

    // ir_lua_file_to_kir returns JSON string
    char* kir_json = ir_lua_file_to_kir(filepath);
    if (!kir_json) {
        PARSER_ERROR_PARSE_FAILED("Lua", "ir_lua_file_to_kir failed");
        return NULL;
    }

    // Parse JSON string to cJSON
    cJSON* kir = cJSON_Parse(kir_json);
    free(kir_json);

    if (!kir) {
        PARSER_ERROR_INVALID_JSON("Lua");
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Parser Interface Export
 * ============================================================================ */

const ParserInterface lua_parser_interface = {
    .get_info = lua_parser_get_info,
    .parse_file = lua_parser_parse_file,
    .validate = NULL,
    .cleanup = NULL
};
