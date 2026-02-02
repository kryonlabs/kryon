/**
 * @file codegen_registration.c
 * @brief Centralized Codegen Registration
 *
 * Registers all codegen adapters with the codegen interface system.
 * This file is called during codegen library initialization to populate
 * the codegen registry.
 */

#include "codegen_interface.h"
#include <stdio.h>
#include <stdlib.h>

/* External codegen interfaces */
extern const CodegenInterface KRY_codegen_interface;
extern const CodegenInterface C_codegen_interface;
extern const CodegenInterface Limbo_codegen_interface;
extern const CodegenInterface Tcl_codegen_interface;
extern const CodegenInterface Markdown_codegen_interface;

/* ============================================================================
 * Codegen Registration
 * ============================================================================ */

/**
 * Register all built-in codegens
 *
 * This function should be called once during codegen library initialization.
 * It registers all codegen implementations with the codegen registry.
 *
 * @return 0 on success, non-zero on error
 */
int codegen_registry_register_all(void) {
    int error = 0;

    // Register all codegens
    if (codegen_register(&KRY_codegen_interface) != 0) error++;
    if (codegen_register(&C_codegen_interface) != 0) error++;
    if (codegen_register(&Limbo_codegen_interface) != 0) error++;
    if (codegen_register(&Tcl_codegen_interface) != 0) error++;
    if (codegen_register(&Markdown_codegen_interface) != 0) error++;

    if (error > 0) {
        fprintf(stderr, "Warning: %d codegen(s) failed to register\n", error);
    }

    return error;
}
