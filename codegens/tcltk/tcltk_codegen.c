/**
 * Tcl/Tk Code Generator
 * Generates Tcl/Tk scripts from KIR JSON via TKIR
 *
 * This is now a thin wrapper that routes to the TKIR pipeline.
 * All widget generation logic has been moved to:
 * - codegens/tkir/tkir_builder.c (KIR → TKIR transformation)
 * - codegens/tcltk/tcltk_from_tkir.c (TKIR → Tcl/Tk emission)
 */

#define _POSIX_C_SOURCE 200809L

#include "tcltk_codegen.h"
#include "../codegen_common.h"
#include "../tkir/tkir.h"
#include "../tkir/tkir_builder.h"
#include "../tkir/tkir_emitter.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void generate_tcl_from_component(StringBuilder* sb, cJSON* component, const char* parent_path);

/* ============================================================================
 * Public API - Routes to TKIR Pipeline
 * ============================================================================ */

/**
 * Generate Tcl/Tk script from KIR file
 */
bool tcltk_codegen_generate(const char* kir_path, const char* output_path) {
    // Use simple KIR → Tcl conversion (bypasses TKIR to avoid memory issues)
    char* tcl_code = tcltk_codegen_from_json_simple(kir_path);
    if (!tcl_code) {
        codegen_error("Simple codegen failed");
        return false;
    }

    FILE* f = fopen(output_path, "w");
    if (!f) {
        codegen_error("Could not open output file: %s", output_path);
        free(tcl_code);
        return false;
    }

    fputs(tcl_code, f);
    fclose(f);
    free(tcl_code);
    return true;

    // TKIR pipeline disabled due to memory issues
    // return tcltk_codegen_generate_via_tkir(kir_path, output_path, NULL);
}

/**
 * Generate Tcl/Tk script from KIR JSON string
 * Routes to TKIR pipeline
 */
char* tcltk_codegen_from_json(const char* kir_json) {
    return tcltk_codegen_from_json_via_tkir(kir_json, NULL);
}

/**
 * Generate Tcl/Tk module files from KIR
 */
bool tcltk_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    if (!output_dir) {
        return tcltk_codegen_generate(kir_path, "output.tcl");
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
    snprintf(output_path, sizeof(output_path), "%s/%s.tcl", output_dir, project_name);

    return tcltk_codegen_generate(kir_path, output_path);
}

/**
 * Generate Tcl/Tk script with options
 */
bool tcltk_codegen_generate_with_options(const char* kir_path,
                                         const char* output_path,
                                         TclTkCodegenOptions* options) {
    return tcltk_codegen_generate_via_tkir(kir_path, output_path, options);
}

/* ============================================================================
 * Simple KIR → Tcl Codegen (Bypasses TKIR)
 * ============================================================================ */

/**
 * Generate Tcl code from a KIR component
 */
static void generate_tcl_from_component(StringBuilder* sb, cJSON* component, const char* parent_path) {
    if (!component) return;

    // Get component type
    const char* type = codegen_get_component_type(component);
    if (!type) return;

    // Get widget path if available
    cJSON* path_item = cJSON_GetObjectItem(component, "_widgetPath");
    const char* widget_path = path_item && cJSON_IsString(path_item) ? path_item->valuestring : NULL;

    // Generate Tcl command based on type
    if (strcmp(type, "Button") == 0) {
        if (widget_path) {
            sb_append(sb, widget_path);
            sb_append(sb, " configure -text ");

            // Get text property
            cJSON* text = cJSON_GetObjectItem(component, "text");
            if (text && cJSON_IsString(text)) {
                sb_append(sb, "\"");
                sb_append(sb, text->valuestring);
                sb_append(sb, "\"");
            }
            sb_append(sb, "\n");
        }
    } else if (strcmp(type, "Text") == 0) {
        if (widget_path) {
            sb_append(sb, widget_path);
            sb_append(sb, " configure -text ");

            // Get text property
            cJSON* text = cJSON_GetObjectItem(component, "text");
            if (text && cJSON_IsString(text)) {
                sb_append(sb, "\"");
                sb_append(sb, text->valuestring);
                sb_append(sb, "\"");
            }
            sb_append(sb, "\n");
        }
    } else if (strcmp(type, "Container") == 0) {
        if (widget_path) {
            // Get background color
            cJSON* bg = cJSON_GetObjectItem(component, "background");
            if (bg && cJSON_IsString(bg) && strcmp(bg->valuestring, "#00000000") != 0) {
                sb_append(sb, widget_path);
                sb_append(sb, " configure -background ");
                sb_append(sb, bg->valuestring);
                sb_append(sb, "\n");
            }
        }
    }

    // Process children
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            generate_tcl_from_component(sb, child, widget_path);
        }
    }
}

/**
 * Simple KIR to Tcl converter
 * Reads a KIR file and generates Tcl code directly
 */
char* tcltk_codegen_from_json_simple(const char* kir_path) {
    // Read KIR file
    size_t size;
    char* kir_json = codegen_read_kir_file(kir_path, &size);
    if (!kir_json) {
        return NULL;
    }

    // Parse KIR JSON
    cJSON* kir_root = codegen_parse_kir_json(kir_json);
    free(kir_json);
    if (!kir_root) {
        return NULL;
    }

    // Extract root component
    cJSON* root_component = codegen_extract_root_component(kir_root);
    if (!root_component) {
        cJSON_Delete(kir_root);
        return NULL;
    }

    // Create string builder
    StringBuilder* sb = sb_create(8192);
    if (!sb) {
        cJSON_Delete(kir_root);
        return NULL;
    }

    // Generate Tcl code from root component
    generate_tcl_from_component(sb, root_component, NULL);

    // Cleanup
    cJSON_Delete(kir_root);

    char* result = sb_get(sb);
    sb_free(sb);
    return result;
}

/* ============================================================================
 * TKIR-based Codegen (New Pipeline)
 * ============================================================================ */

/**
 * @brief Generate Tcl/Tk script from KIR via TKIR
 *
 * This is the new recommended codegen path:
 * KIR → TKIR → Tcl/Tk
 *
 * @param kir_json KIR JSON string
 * @param options Codegen options (NULL for defaults)
 * @return Allocated Tcl/Tk script string (caller must free), or NULL on error
 */
char* tcltk_codegen_from_json_via_tkir(const char* kir_json, TclTkCodegenOptions* options) {
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

    // Step 3: TKIR → Tcl/Tk
    char* output = tcltk_codegen_from_tkir(tkir_json, options);

    // Cleanup
    free(tkir_json);
    tkir_root_free(tkir_root);

    return output;
}

/**
 * @brief Generate Tcl/Tk script from KIR file via TKIR
 *
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .tcl file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool tcltk_codegen_generate_via_tkir(const char* kir_path, const char* output_path,
                                      TclTkCodegenOptions* options) {
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
    char* output = tcltk_codegen_from_json_via_tkir(kir_json, options);
    free(kir_json);

    if (!output) {
        return false;
    }

    // Write output file
    bool success = codegen_write_output_file(output_path, output);
    free(output);

    return success;
}
