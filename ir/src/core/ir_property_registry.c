#include "ir_property_registry.h"
#include <string.h>
#include <stdlib.h>

#define MAX_PROPERTY_PARSERS 64

typedef struct {
    char property_name[64];
    kryon_property_parser_fn parser;
    bool used;
} PropertyParser;

static PropertyParser g_parsers[MAX_PROPERTY_PARSERS] = {0};
static int g_parser_count = 0;

bool ir_register_property_parser(const char* name, kryon_property_parser_fn parser) {
    if (!name || !parser) {
        return false;
    }

    if (g_parser_count >= MAX_PROPERTY_PARSERS) {
        return false;
    }

    // Check if already registered
    for (int i = 0; i < g_parser_count; i++) {
        if (strcmp(g_parsers[i].property_name, name) == 0) {
            return false; // Already registered
        }
    }

    // Register new parser
    strncpy(g_parsers[g_parser_count].property_name, name, 63);
    g_parsers[g_parser_count].property_name[63] = '\0';
    g_parsers[g_parser_count].parser = parser;
    g_parsers[g_parser_count].used = true;
    g_parser_count++;

    return true;
}

kryon_property_parser_fn ir_get_property_parser(const char* name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < g_parser_count; i++) {
        if (g_parsers[i].used && strcmp(g_parsers[i].property_name, name) == 0) {
            return g_parsers[i].parser;
        }
    }

    return NULL; // Not found
}
