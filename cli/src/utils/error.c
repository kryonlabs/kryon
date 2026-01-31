/**
 * Error Reporting Utilities
 * Centralized error formatting for consistent error messages
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdarg.h>

/**
 * Print a simple error message to stderr
 */
void error_print(const char* message) {
    if (!message) return;
    fprintf(stderr, "Error: %s\n", message);
}

/**
 * Print an error message with a helpful hint
 */
void error_print_with_hint(const char* message, const char* hint) {
    if (!message) return;

    fprintf(stderr, "Error: %s\n", message);
    if (hint) {
        fprintf(stderr, "Hint: %s\n", hint);
    }
}

/**
 * Print a contextualized error message
 * Context helps identify where the error occurred
 */
void error_print_context(const char* context, const char* message) {
    if (!message) return;

    if (context) {
        fprintf(stderr, "Error in %s: %s\n", context, message);
    } else {
        fprintf(stderr, "Error: %s\n", message);
    }
}
