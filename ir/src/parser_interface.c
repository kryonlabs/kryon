/**
 * @file parser_interface.c
 * @brief Parser Registry Implementation
 */

#include "parser_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For strcasecmp
#include <ctype.h>

/* ============================================================================
 * Parser Registry
 * ============================================================================ */

#define MAX_PARSERS 16

static const ParserInterface* g_parsers[MAX_PARSERS] = {NULL};
static int g_parser_count = 0;

int parser_register(const ParserInterface* parser) {
    if (!parser || !parser->get_info || !parser->parse_file) {
        fprintf(stderr, "Error: Invalid parser interface\n");
        return 1;
    }

    if (g_parser_count >= MAX_PARSERS) {
        fprintf(stderr, "Error: Parser registry full\n");
        return 1;
    }

    const ParserInfo* info = parser->get_info();
    if (!info) {
        fprintf(stderr, "Error: Parser get_info() returned NULL\n");
        return 1;
    }

    // Check for duplicates
    for (int i = 0; i < g_parser_count; i++) {
        const ParserInfo* existing = g_parsers[i]->get_info();
        if (existing && strcmp(existing->name, info->name) == 0) {
            fprintf(stderr, "Warning: Parser '%s' already registered\n", info->name);
            return 0;  // Not an error, just skip
        }
    }

    g_parsers[g_parser_count++] = parser;
    printf("[parser] Registered: %s v%s\n", info->name, info->version);
    return 0;
}

const ParserInterface* parser_get_interface(const char* format) {
    if (!format) return NULL;

    // Case-insensitive comparison
    char format_lower[64];
    strncpy(format_lower, format, sizeof(format_lower) - 1);
    format_lower[sizeof(format_lower) - 1] = '\0';
    for (char* p = format_lower; *p; p++) {
        *p = tolower((unsigned char)*p);
    }

    for (int i = 0; i < g_parser_count; i++) {
        const ParserInfo* info = g_parsers[i]->get_info();
        if (!info) continue;

        // Case-insensitive name comparison
        char name_lower[64];
        strncpy(name_lower, info->name, sizeof(name_lower) - 1);
        name_lower[sizeof(name_lower) - 1] = '\0';
        for (char* p = name_lower; *p; p++) {
            *p = tolower((unsigned char)*p);
        }

        if (strcmp(name_lower, format_lower) == 0) {
            return g_parsers[i];
        }
    }

    return NULL;
}

const ParserInterface* parser_get_interface_by_ext(const char* extension) {
    if (!extension) return NULL;

    for (int i = 0; i < g_parser_count; i++) {
        const ParserInfo* info = g_parsers[i]->get_info();
        if (!info || !info->extensions) continue;

        // Check if extension matches
        for (int j = 0; info->extensions[j]; j++) {
            if (strcasecmp(extension, info->extensions[j]) == 0) {
                return g_parsers[i];
            }
        }
    }

    return NULL;
}

const ParserInterface** parser_list_all(void) {
    // Return static array (NULL-terminated)
    static const ParserInterface* all_parsers[MAX_PARSERS + 1] = {NULL};

    int count = 0;
    for (int i = 0; i < g_parser_count && count < MAX_PARSERS; i++) {
        all_parsers[count++] = g_parsers[i];
    }
    all_parsers[count] = NULL;

    return all_parsers;
}

/* ============================================================================
 * Standard Parser Implementations
 * ============================================================================ */

cJSON* parser_parse_file(const char* filepath) {
    if (!filepath) {
        fprintf(stderr, "Error: NULL filepath\n");
        return NULL;
    }

    // Extract extension
    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        fprintf(stderr, "Error: No file extension in: %s\n", filepath);
        return NULL;
    }

    // Get parser by extension
    const ParserInterface* parser = parser_get_interface_by_ext(ext);
    if (!parser) {
        fprintf(stderr, "Error: No parser found for extension: %s\n", ext);
        fprintf(stderr, "Supported formats: ");
        for (int i = 0; i < g_parser_count; i++) {
            const ParserInfo* info = g_parsers[i]->get_info();
            if (info && info->extensions) {
                for (int j = 0; info->extensions[j]; j++) {
                    fprintf(stderr, "%s%s", j > 0 ? ", " : "", info->extensions[j]);
                }
            }
        }
        fprintf(stderr, "\n");
        return NULL;
    }

    // Parse file
    return parser->parse_file(filepath);
}

cJSON* parser_parse_file_as(const char* filepath, const char* format) {
    if (!filepath || !format) {
        fprintf(stderr, "Error: Invalid arguments\n");
        return NULL;
    }

    const ParserInterface* parser = parser_get_interface(format);
    if (!parser) {
        fprintf(stderr, "Error: No parser found for format: %s\n", format);
        return NULL;
    }

    return parser->parse_file(filepath);
}
