/**
 * Limbo Code Generator
 * Generates Limbo source (.b) files from KIR JSON via TKIR
 *
 * This is now a thin wrapper that routes to the TKIR pipeline.
 * All widget generation logic has been moved to:
 * - codegens/tkir/tkir_builder.c (KIR → TKIR transformation)
 * - codegens/limbo/limbo_from_tkir.c (TKIR → Limbo emission)
 */

#define _POSIX_C_SOURCE 200809L

#include "limbo_codegen.h"
#include "../codegen_common.h"
#include "../tkir/tkir.h"
#include "../tkir/tkir_builder.h"
#include "../tkir/tkir_emitter.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ============================================================================
 * Public API - Routes to TKIR Pipeline
 * ============================================================================ */

/**
 * Generate Limbo source (.b) from KIR file
 */
bool limbo_codegen_generate(const char* kir_path, const char* output_path) {
    return limbo_codegen_generate_via_tkir(kir_path, output_path, NULL);
}

/**
 * Generate Limbo source from KIR JSON string
 * Routes to TKIR pipeline
 */
char* limbo_codegen_from_json(const char* kir_json) {
    return limbo_codegen_from_json_via_tkir(kir_json, NULL);
}

/**
 * Generate Limbo module files from KIR
 */
bool limbo_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    // For now, just call single-file generation
    if (!output_dir) {
        return limbo_codegen_generate_via_tkir(kir_path, "output.b", NULL);
    }

    // Extract project name from kir_path
    const char* filename = strrchr(kir_path, '/');
    if (!filename) filename = kir_path;
    else filename++;

    char project_name[256];
    snprintf(project_name, sizeof(project_name), "%.255s", filename);

    // Remove .kir extension if present
    char* ext = strstr(project_name, ".kir");
    if (ext) *ext = '\0';

    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/%s.b", output_dir, project_name);

    return limbo_codegen_generate_via_tkir(kir_path, output_path, NULL);
}

/**
 * Generate Limbo source with options
 */
bool limbo_codegen_generate_with_options(const char* kir_path,
                                          const char* output_path,
                                          LimboCodegenOptions* options) {
    return limbo_codegen_generate_via_tkir(kir_path, output_path, options);
}

/* ============================================================================
 * TKIR-based Codegen (New Pipeline)
 * ============================================================================ */

/**
 * @brief Generate Limbo source from KIR via TKIR
 *
 * This is the new recommended codegen path:
 * KIR → TKIR → Limbo
 *
 * @param kir_json KIR JSON string
 * @param options Codegen options (NULL for defaults)
 * @return Allocated Limbo source string (caller must free), or NULL on error
 */
char* limbo_codegen_from_json_via_tkir(const char* kir_json, LimboCodegenOptions* options) {
    if (!kir_json) {
        codegen_error("NULL KIR JSON provided");
        return NULL;
    }

    // Step 1: KIR → TKIR
    TKIRRoot* tkir_root = tkir_build_from_kir(kir_json, false);
    if (!tkir_root) {
        codegen_error("Failed to build TKIR from KIR");
        return NULL;
    }

    // Step 2: TKIR → JSON (for the emitter)
    char* tkir_json = tkir_root_to_json(tkir_root);
    if (!tkir_json) {
        tkir_root_free(tkir_root);
        codegen_error("Failed to serialize TKIR to JSON");
        return NULL;
    }

    // Step 3: TKIR → Limbo
    char* output = limbo_codegen_from_tkir(tkir_json, options);

    // Cleanup
    free(tkir_json);
    tkir_root_free(tkir_root);

    return output;
}

/**
 * @brief Generate Limbo source from KIR file via TKIR
 *
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .b file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool limbo_codegen_generate_via_tkir(const char* kir_path, const char* output_path,
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

    // Generate via TKIR
    char* output = limbo_codegen_from_json_via_tkir(kir_json, options);
    free(kir_json);

    if (!output) {
        return false;
    }

    // Write output file
    bool success = codegen_write_output_file(output_path, output);
    free(output);

    return success;
}
