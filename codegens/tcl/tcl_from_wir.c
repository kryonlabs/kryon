/**
 * @file tcl_from_wir.c
 * @brief Tcl Code Generator from WIR (Adapter)
 *
 * This file serves as an adapter between the WIR system and Tcl code generation.
 * Uses the WIR composer to combine Tcl language emitter with the appropriate toolkit.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#define _POSIX_C_SOURCE 200809L

#include "tcl_codegen.h"
#include "../wir/wir.h"
#include "../wir/wir_emitter.h"
#include "../common/wir_composer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Public API - Tcl Codegen from WIR
// ============================================================================

/**
 * Generate Tcl code from WIR root
 * @param root WIR root
 * @param toolkit Toolkit name ("tk", "terminal", etc.)
 * @param output Output file
 * @return true on success, false on failure
 */
bool tcl_codegen_from_wir(WIRRoot* root, const char* toolkit, FILE* output) {
    if (!root || !toolkit || !output) return false;

    // Use the WIR composer to generate code
    WIRComposerOptions options = {0};
    options.include_comments = true;
    options.verbose = false;

    char* result = wir_compose_and_emit("tcl", toolkit, root, &options);
    if (!result) {
        return false;
    }

    fputs(result, output);
    free(result);

    return true;
}

/**
 * Generate Tcl code from WIR root (returns string)
 * @param root WIR root
 * @param toolkit Toolkit name ("tk", "terminal", etc.)
 * @return Newly allocated Tcl code (caller must free), or NULL on error
 */
char* tcl_codegen_from_wir_to_string(WIRRoot* root, const char* toolkit) {
    if (!root || !toolkit) return NULL;

    // Use the WIR composer to generate code
    WIRComposerOptions options = {0};
    options.include_comments = true;
    options.verbose = false;

    return wir_compose_and_emit("tcl", toolkit, root, &options);
}
