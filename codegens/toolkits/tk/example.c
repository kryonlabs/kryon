/**
 * @file example.c
 * @brief TKIR Pipeline Example
 *
 * Demonstrates the KIR → TKIR → Target code generation pipeline.
 *
 * Build:
 *   gcc -o tkir_example example.c -I../include -I../ir/include \
 *       -I../third_party/cJSON -L../build -lkryon_tkir -lkryon_codegen_common \
 *       -lcjson -lm
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#define _POSIX_C_SOURCE 200809L

#include "tkir/tkir.h"
#include "tkir/tkir_builder.h"
#include "tkir/tkir_emitter.h"
#include "../codegen_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Sample KIR JSON
 * ============================================================================ */

static const char* sample_kir =
    "{\n"
    "  \"format\": \"kir\",\n"
    "  \"version\": \"1.0\",\n"
    "  \"window\": {\n"
    "    \"title\": \"Hello TKIR\",\n"
    "    \"width\": 400,\n"
    "    \"height\": 300,\n"
    "    \"resizable\": true,\n"
    "    \"background\": \"#1a1a2e\"\n"
    "  },\n"
    "  \"root\": {\n"
    "    \"type\": \"Column\",\n"
    "    \"background\": \"#16213e\",\n"
    "    \"children\": [\n"
    "      {\n"
    "        \"type\": \"Text\",\n"
    "        \"text\": \"Welcome to TKIR!\",\n"
    "        \"color\": \"#ffffff\",\n"
    "        \"font\": {\"family\": \"Arial\", \"size\": 24}\n"
    "      },\n"
    "      {\n"
    "        \"type\": \"Button\",\n"
    "        \"text\": \"Click Me\",\n"
    "        \"background\": \"#0f3460\",\n"
    "        \"color\": \"#e94560\"\n"
    "      }\n"
    "    ]\n"
    "  }\n"
    "}\n";

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Print separator line
 */
static void print_separator(const char* title) {
    printf("\n");
    printf("================================================================\n");
    printf("  %s\n", title);
    printf("================================================================\n");
}

/**
 * Write content to file
 */
static bool write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", path);
        return false;
    }
    fputs(content, f);
    fclose(f);
    printf("Written to: %s\n", path);
    return true;
}

/* ============================================================================
 * Pipeline Demo Functions
 * ============================================================================ */

/**
 * Step 1: KIR → TKIR transformation
 */
static TKIRRoot* kir_to_tkir(const char* kir_json) {
    print_separator("Step 1: KIR → TKIR");

    TKIRRoot* root = tkir_build_from_kir(kir_json, true);
    if (!root) {
        fprintf(stderr, "Failed to build TKIR from KIR\n");
        return NULL;
    }

    printf("✓ TKIR built successfully\n");
    printf("  Window: %s (%dx%d)\n", root->title, root->width, root->height);
    printf("  Widgets: %d\n", cJSON_GetArraySize(root->widgets));
    printf("  Handlers: %d\n", cJSON_GetArraySize(root->handlers));

    return root;
}

/**
 * Step 2: TKIR → JSON serialization
 */
static char* tkir_to_json(TKIRRoot* root) {
    print_separator("Step 2: TKIR → JSON");

    char* tkir_json = tkir_root_to_json(root);
    if (!tkir_json) {
        fprintf(stderr, "Failed to serialize TKIR to JSON\n");
        return NULL;
    }

    printf("✓ TKIR serialized to JSON (%zu bytes)\n", strlen(tkir_json));

    // Write to file
    write_file("example_output.tkir", tkir_json);

    return tkir_json;
}

/**
 * Step 3a: TKIR → Tcl/Tk
 */
static bool tkir_to_tcltk(const char* tkir_json) {
    print_separator("Step 3a: TKIR → Tcl/Tk");

    // Initialize Tcl/Tk emitter
    tcltk_tkir_emitter_init();

    // Create emitter context
    TKIREmitterOptions opts = {
        .include_comments = true,
        .verbose = false
    };
    TKIREmitterContext* ctx = tkir_emitter_create(TKIR_EMITTER_TCLTK, &opts);
    if (!ctx) {
        fprintf(stderr, "Failed to create Tcl/Tk emitter\n");
        return false;
    }

    // Parse TKIR
    cJSON* tkir_root = codegen_parse_kir_json(tkir_json);
    if (!tkir_root) {
        fprintf(stderr, "Failed to parse TKIR JSON\n");
        tkir_emitter_context_free(ctx);
        return false;
    }

    // Create TKIRRoot
    TKIRRoot* root = tkir_root_from_cJSON(tkir_root);
    if (!root) {
        fprintf(stderr, "Failed to create TKIRRoot\n");
        cJSON_Delete(tkir_root);
        tkir_emitter_context_free(ctx);
        return false;
    }

    // Generate Tcl/Tk
    char* tcl_output = tkir_emit(ctx, root);
    if (!tcl_output) {
        fprintf(stderr, "Failed to generate Tcl/Tk\n");
        tkir_root_free(root);
        cJSON_Delete(tkir_root);
        tkir_emitter_context_free(ctx);
        return false;
    }

    printf("✓ Tcl/Tk generated (%zu bytes)\n", strlen(tcl_output));

    // Write to file
    write_file("example_output.tcl", tcl_output);

    // Cleanup
    free(tcl_output);
    tkir_root_free(root);
    cJSON_Delete(tkir_root);
    tkir_emitter_context_free(ctx);
    tcltk_tkir_emitter_cleanup();

    return true;
}

/**
 * Step 3b: TKIR → Limbo
 */
static bool tkir_to_limbo(const char* tkir_json) {
    print_separator("Step 3b: TKIR → Limbo");

    // Initialize Limbo emitter
    limbo_tkir_emitter_init();

    // Create emitter context
    TKIREmitterOptions opts = {
        .include_comments = true,
        .verbose = false
    };
    TKIREmitterContext* ctx = tkir_emitter_create(TKIR_EMITTER_LIMBO, &opts);
    if (!ctx) {
        fprintf(stderr, "Failed to create Limbo emitter\n");
        return false;
    }

    // Parse TKIR
    cJSON* tkir_root = codegen_parse_kir_json(tkir_json);
    if (!tkir_root) {
        fprintf(stderr, "Failed to parse TKIR JSON\n");
        tkir_emitter_context_free(ctx);
        return false;
    }

    // Create TKIRRoot
    TKIRRoot* root = tkir_root_from_cJSON(tkir_root);
    if (!root) {
        fprintf(stderr, "Failed to create TKIRRoot\n");
        cJSON_Delete(tkir_root);
        tkir_emitter_context_free(ctx);
        return false;
    }

    // Set module name for Limbo context
    LimboEmitterContext* limbo_ctx = (LimboEmitterContext*)ctx;
    limbo_ctx->module_name = "TkirExample";

    // Generate Limbo
    char* limbo_output = tkir_emit(ctx, root);
    if (!limbo_output) {
        fprintf(stderr, "Failed to generate Limbo\n");
        tkir_root_free(root);
        cJSON_Delete(tkir_root);
        tkir_emitter_context_free(ctx);
        return false;
    }

    printf("✓ Limbo generated (%zu bytes)\n", strlen(limbo_output));

    // Write to file
    write_file("example_output.b", limbo_output);

    // Cleanup
    free(limbo_output);
    tkir_root_free(root);
    cJSON_Delete(tkir_root);
    tkir_emitter_context_free(ctx);
    limbo_tkir_emitter_cleanup();

    return true;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TKIR Pipeline Example                                    ║\n");
    printf("║  Demonstrates KIR → TKIR → Target code generation        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    // Step 1: KIR → TKIR
    TKIRRoot* root = kir_to_tkir(sample_kir);
    if (!root) {
        return 1;
    }

    // Step 2: TKIR → JSON
    char* tkir_json = tkir_to_json(root);
    if (!tkir_json) {
        tkir_root_free(root);
        return 1;
    }

    // Step 3a: TKIR → Tcl/Tk
    if (!tkir_to_tcltk(tkir_json)) {
        fprintf(stderr, "Tcl/Tk generation failed\n");
    }

    // Step 3b: TKIR → Limbo
    if (!tkir_to_limbo(tkir_json)) {
        fprintf(stderr, "Limbo generation failed\n");
    }

    // Cleanup
    free(tkir_json);
    tkir_root_free(root);

    print_separator("Complete");
    printf("✓ All outputs generated successfully!\n");
    printf("\nGenerated files:\n");
    printf("  - example_output.tkir (TKIR JSON)\n");
    printf("  - example_output.tcl  (Tcl/Tk script)\n");
    printf("  - example_output.b    (Limbo source)\n");
    printf("\n");

    return 0;
}
