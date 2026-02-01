/**
 * @file codegen_registry.c
 * @brief Codegen Registry Implementation
 */

#include "codegen_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For strcasecmp
#include <ctype.h>

/* ============================================================================
 * Codegen Registry
 * ============================================================================ */

#define MAX_CODEGENS 16

static const CodegenInterface* g_codegens[MAX_CODEGENS] = {NULL};
static int g_codegen_count = 0;

int codegen_register(const CodegenInterface* codegen) {
    if (!codegen || !codegen->get_info || !codegen->generate) {
        fprintf(stderr, "Error: Invalid codegen interface\n");
        return 1;
    }

    if (g_codegen_count >= MAX_CODEGENS) {
        fprintf(stderr, "Error: Codegen registry full\n");
        return 1;
    }

    const CodegenInfo* info = codegen->get_info();
    if (!info) {
        fprintf(stderr, "Error: Codegen get_info() returned NULL\n");
        return 1;
    }

    // Check for duplicates
    for (int i = 0; i < g_codegen_count; i++) {
        const CodegenInfo* existing = g_codegens[i]->get_info();
        if (existing && strcasecmp(existing->name, info->name) == 0) {
            fprintf(stderr, "Warning: Codegen '%s' already registered\n", info->name);
            return 0;  // Not an error, just skip
        }
    }

    g_codegens[g_codegen_count++] = codegen;
    printf("[codegen] Registered: %s v%s\n", info->name, info->version);
    return 0;
}

const CodegenInterface* codegen_get_interface(const char* target) {
    if (!target) return NULL;

    // Case-insensitive comparison
    char target_lower[64];
    strncpy(target_lower, target, sizeof(target_lower) - 1);
    target_lower[sizeof(target_lower) - 1] = '\0';
    for (char* p = target_lower; *p; p++) {
        *p = tolower((unsigned char)*p);
    }

    for (int i = 0; i < g_codegen_count; i++) {
        const CodegenInfo* info = g_codegens[i]->get_info();
        if (!info) continue;

        // Case-insensitive name comparison
        char name_lower[64];
        strncpy(name_lower, info->name, sizeof(name_lower) - 1);
        name_lower[sizeof(name_lower) - 1] = '\0';
        for (char* p = name_lower; *p; p++) {
            *p = tolower((unsigned char)*p);
        }

        if (strcmp(name_lower, target_lower) == 0) {
            return g_codegens[i];
        }
    }

    return NULL;
}

const CodegenInterface** codegen_list_all(void) {
    // Return static array (NULL-terminated)
    static const CodegenInterface* all_codegens[MAX_CODEGENS + 1] = {NULL};

    int count = 0;
    for (int i = 0; i < g_codegen_count && count < MAX_CODEGENS; i++) {
        all_codegens[count++] = g_codegens[i];
    }
    all_codegens[count] = NULL;

    return all_codegens;
}

/* ============================================================================
 * Standard Codegen Implementations
 * ============================================================================ */

bool codegen_generate_from_registry(const char* kir_path, const char* target, const char* output_path) {
    if (!kir_path || !target || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to codegen_generate_from_registry\n");
        return false;
    }

    const CodegenInterface* codegen = codegen_get_interface(target);
    if (!codegen) {
        fprintf(stderr, "Error: No codegen found for target: %s\n", target);
        fprintf(stderr, "Supported targets: ");
        for (int i = 0; i < g_codegen_count; i++) {
            const CodegenInfo* info = g_codegens[i]->get_info();
            if (info) {
                fprintf(stderr, "%s%s", i > 0 ? ", " : "", info->name);
            }
        }
        fprintf(stderr, "\n");
        return false;
    }

    printf("Generating %s from KIR: %s\n", target, kir_path);
    return codegen->generate(kir_path, output_path);
}
