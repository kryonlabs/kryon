/**
 * @file markdown_parser_adapter.c
 * @brief Markdown Parser Adapter for Parser Interface
 *
 * Adapts the existing Markdown parser to implement the unified ParserInterface.
 * The Markdown parser reads source string (not file), so we handle file I/O.
 */

#include "../../include/parser_interface.h"
#include "markdown_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Parser Metadata
 * ============================================================================ */

static const char* markdown_extensions[] = {".md", ".markdown", NULL};

static const ParserInfo markdown_parser_info = {
    .name = "Markdown",
    .version = "alpha",
    .caps = PARSER_CAP_LAYOUT,
    .extensions = markdown_extensions
};

/* ============================================================================
 * Parser Interface Implementation
 * ============================================================================ */

static const ParserInfo* markdown_parser_get_info(void) {
    return &markdown_parser_info;
}

static cJSON* markdown_parser_parse_file(const char* filepath) {
    if (!filepath) {
        PARSER_ERROR_FILE_NOT_FOUND("(null)");
        return NULL;
    }

    // Read file content
    FILE* f = fopen(filepath, "r");
    if (!f) {
        PARSER_ERROR_FILE_NOT_FOUND(filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    // Parse markdown to KIR JSON string
    char* kir_json = ir_markdown_to_kir(content, size);
    free(content);

    if (!kir_json) {
        PARSER_ERROR_PARSE_FAILED("Markdown", "ir_markdown_to_kir failed");
        return NULL;
    }

    // Parse JSON string to cJSON
    cJSON* kir = cJSON_Parse(kir_json);
    free(kir_json);

    if (!kir) {
        PARSER_ERROR_INVALID_JSON("Markdown");
        return NULL;
    }

    return kir;
}

/* ============================================================================
 * Parser Interface Export
 * ============================================================================ */

const ParserInterface markdown_parser_interface = {
    .get_info = markdown_parser_get_info,
    .parse_file = markdown_parser_parse_file,
    .validate = NULL,
    .cleanup = NULL
};
