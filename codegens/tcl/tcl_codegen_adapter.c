/**
 * @file tcl_codegen_adapter.c
 * @brief Tcl Codegen Adapter for Codegen Interface
 *
 * Adapts the Tcl codegen to implement the unified CodegenInterface.
 * Uses the WIR composer to combine Tcl language emitter with the appropriate toolkit.
 * Uses a custom implementation for WIR composition.
 */

#include "../codegen_adapter_macros.h"
#include "../codegen_io.h"
#include "../codegen_common.h"
#include "../common/wir_composer.h"
#include "tcl_wir_emitter.h"
#include "../toolkits/tk/tk_wir_emitter.h"
#include "../wir/wir.h"
#include "../wir/wir_builder.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Codegen Adapter - Custom Implementation
 * ============================================================================ */

/**
 * Custom generate implementation using WIR composer.
 * Combines Tcl language emitter with Tk toolkit emitter.
 */
static bool tcl_codegen_generate_with_composer(const char* kir_path, const char* output_path) {
    codegen_set_error_prefix("Tcl");

    if (!kir_path || !output_path) {
        codegen_error("Invalid arguments to Tcl codegen");
        return false;
    }

    // Load KIR input
    CodegenInput* input = codegen_load_input(kir_path);
    if (!input) {
        return false;
    }

    // Build WIR from KIR
    WIRRoot* root = wir_build_from_cJSON(input->root, false);
    if (!root) {
        codegen_error("Failed to build WIR from KIR");
        codegen_free_input(input);
        return false;
    }

    // Create composed emitter (tcl + tk)
    WIRComposerOptions options = {0};
    options.include_comments = true;
    options.verbose = false;

    char* output = wir_compose_and_emit("tcl", "tk", root, &options);
    if (!output) {
        codegen_error("Failed to generate code using WIR composer");
        wir_root_free(root);
        codegen_free_input(input);
        return false;
    }

    // Write output file
    bool success = codegen_write_output_file(output_path, output);
    free(output);

    // Cleanup
    wir_root_free(root);
    codegen_free_input(input);

    if (!success) {
        codegen_error("Failed to write output file: %s", output_path);
        return false;
    }

    return true;
}

/* Use macro with custom generate function */
IMPLEMENT_CODEGEN_ADAPTER(Tcl, "Tcl code generator (separated from Tk)", tcl_codegen_generate_with_composer, ".tcl", NULL)
