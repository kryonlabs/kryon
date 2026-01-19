/*
 * Stubs for deprecated parser conversion functions
 * These provide minimal implementations to allow linking
 */

#include <stdlib.h>
#include <string.h>
#include "../include/ir_core.h"

// Stub for ir_kry_to_kir - converts KRY source to KIR format
// NOTE: This function was in kry_to_ir.c which has compilation issues
char* ir_kry_to_kir(const char* source, size_t length) {
    (void)source;
    (void)length;
    // Return empty KIR for now
    char* empty = malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
}

// Stub for ir_html_file_to_kir - converts HTML file to KIR format
// NOTE: This function was in html_parser.c which has compilation issues
char* ir_html_file_to_kir(const char* filepath) {
    (void)filepath;
    // Return empty KIR for now
    char* empty = malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
}
