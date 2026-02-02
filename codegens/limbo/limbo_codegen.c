/**
 * Limbo Code Generator
 * Generates Limbo source (.b) files from KIR JSON via WIR
 *
 * This is now a thin wrapper that routes to the WIR pipeline.
 * All widget generation logic has been moved to:
 * - codegens/wir/wir_builder.c (KIR → WIR transformation)
 * - codegens/limbo/limbo_from_wir.c (WIR → Limbo emission)
 */

#define _POSIX_C_SOURCE 200809L

#include "limbo_codegen.h"
#include "../codegen_common.h"
#include "../wir/wir.h"
#include "../wir/wir_builder.h"
#include "../wir/wir_emitter.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ============================================================================
 * Public API - Routes to WIR Pipeline
 * ============================================================================ */

/**
 * Generate Limbo source (.b) from KIR file
 */
bool limbo_codegen_generate(const char* kir_path, const char* output_path) {
    return limbo_codegen_generate_via_wir(kir_path, output_path, NULL);
}

/**
 * Generate Limbo source from KIR JSON string
 * Routes to WIR pipeline
 */
char* limbo_codegen_from_json(const char* kir_json) {
    return limbo_codegen_from_json_via_wir(kir_json, NULL);
}

/**
 * Generate Limbo module files from KIR
 */
bool limbo_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    // Build output path using shared helper
    char* output_path = codegen_build_output_path(kir_path, output_dir, ".b");
    if (!output_path) {
        return false;
    }

    bool result = limbo_codegen_generate_via_wir(kir_path, output_path, NULL);
    free(output_path);
    return result;
}

/**
 * Generate Limbo source with options
 */
bool limbo_codegen_generate_with_options(const char* kir_path,
                                          const char* output_path,
                                          LimboCodegenOptions* options) {
    return limbo_codegen_generate_via_wir(kir_path, output_path, options);
}

/* ============================================================================
 * WIR-based Codegen (New Pipeline)
 * ============================================================================ */

/**
 * @brief Generate Limbo source from KIR via WIR
 *
 * This is the new recommended codegen path:
 * KIR → WIR → Limbo
 *
 * @param kir_json KIR JSON string
 * @param options Codegen options (NULL for defaults)
 * @return Allocated Limbo source string (caller must free), or NULL on error
 */
char* limbo_codegen_from_json_via_wir(const char* kir_json, LimboCodegenOptions* options) {
    if (!kir_json) {
        codegen_error("NULL KIR JSON provided");
        return NULL;
    }

    // Step 1: KIR → WIR
    WIRRoot* wir_root = wir_build_from_kir(kir_json, false);
    if (!wir_root) {
        codegen_error("Failed to build WIR from KIR");
        return NULL;
    }

    // Step 2: WIR → JSON (for the emitter)
    char* wir_json = wir_root_to_json(wir_root);
    if (!wir_json) {
        wir_root_free(wir_root);
        codegen_error("Failed to serialize WIR to JSON");
        return NULL;
    }

    // Step 3: WIR → Limbo
    char* output = limbo_codegen_from_wir(wir_json, options);

    // Cleanup
    free(wir_json);
    wir_root_free(wir_root);

    return output;
}

/**
 * @brief Generate Limbo source from KIR file via WIR
 *
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .b file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool limbo_codegen_generate_via_wir(const char* kir_path, const char* output_path,
                                      LimboCodegenOptions* options) {
    if (!kir_path || !output_path) {
        codegen_error("NULL file path provided");
        return false;
    }

    // Read KIR file
    size_t size;
    char* kir_json = codegen_read_kir_file(kir_path, &size);
    if (!kir_json) {
        codegen_error("Failed to read KIR file: %s", kir_path);
        return false;
    }

    // Generate via WIR
    char* output = limbo_codegen_from_json_via_wir(kir_json, options);
    free(kir_json);

    if (!output) {
        return false;
    }

    // Write output file
    bool success = codegen_write_output_file(output_path, output);
    free(output);

    return success;
}
