/**
 * KRY Struct Parser Implementation
 *
 * This file contains the implementation of struct parsing.
 * Note: The actual parsing functions are implemented in kry_parser.c
 * because they need access to static helper functions.
 * This file exists for documentation and build system organization.
 */

#include "kry_struct_parser.h"
#include <string.h>

// Check if identifier matches struct keyword
bool kry_is_struct_keyword(const char* identifier) {
    if (!identifier) return false;
    return strcmp(identifier, "struct") == 0;
}

// Note: kry_parse_struct_declaration and kry_parse_struct_instantiation
// are implemented in kry_parser.c to access static helper functions
