/**
 * @file parser_registry.c
 * @brief Centralized Parser Registration
 *
 * Registers all parser adapters with the parser interface system.
 * This file is called during IR library initialization to populate
 * the parser registry.
 */

#include "../include/parser_interface.h"
#include <stdio.h>
#include <stdlib.h>

/* External parser interfaces */
extern const ParserInterface kry_parser_interface;
extern const ParserInterface tcl_parser_interface;
extern const ParserInterface limbo_parser_interface;
extern const ParserInterface html_parser_interface;
extern const ParserInterface markdown_parser_interface;
extern const ParserInterface c_parser_interface;

/* ============================================================================
 * Parser Registration
 * ============================================================================ */

/**
 * Register all built-in parsers
 *
 * This function should be called once during IR library initialization.
 * It registers all parser implementations with the parser registry.
 *
 * @return 0 on success, non-zero on error
 */
int parser_registry_register_all(void) {
    int error = 0;

    // Register all parsers
    if (parser_register(&kry_parser_interface) != 0) error++;
    if (parser_register(&tcl_parser_interface) != 0) error++;
    if (parser_register(&limbo_parser_interface) != 0) error++;
    if (parser_register(&html_parser_interface) != 0) error++;
    if (parser_register(&markdown_parser_interface) != 0) error++;
    if (parser_register(&c_parser_interface) != 0) error++;

    if (error > 0) {
        fprintf(stderr, "Warning: %d parser(s) failed to register\n", error);
    }

    return error;
}
