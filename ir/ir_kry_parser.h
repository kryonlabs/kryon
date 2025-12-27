/**
 * Kryon .kry Parser - Core Implementation
 *
 * Parses .kry DSL files to native Kryon IR component trees.
 *
 * The .kry format is a declarative DSL for building UI:
 *   App {
 *       windowTitle = "My App"
 *       Container {
 *           Text {
 *               text = "Hello World"
 *           }
 *       }
 *   }
 *
 * Supported Features:
 *   - Component hierarchy (App, Container, Row, Column, Text, Button, etc.)
 *   - Property assignments (text = "value", width = 200)
 *   - Nested components
 *   - String literals, numbers, identifiers
 *   - Comments (single-line and multi-line)
 */

#ifndef IR_KRY_PARSER_H
#define IR_KRY_PARSER_H

#include "ir_core.h"
#include "ir_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse .kry source to IR component tree
 *
 * Converts .kry DSL into a native Kryon IR component hierarchy.
 * The returned component is the root component defined in the .kry file
 * (typically an App or Container).
 *
 * @param source .kry source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return IRComponent* Root component, or NULL on error
 *
 * @note The caller is responsible for freeing the returned component tree
 *       using ir_destroy_component().
 *
 * @example
 *   const char* kry = "App { Text { text = \"Hello\" } }";
 *   IRComponent* root = ir_kry_parse(kry, 0);
 *   if (root) {
 *       // Render or serialize the component tree
 *       ir_destroy_component(root);
 *   }
 */
IRComponent* ir_kry_parse(const char* source, size_t length);

/**
 * Convert .kry source to KIR JSON string
 *
 * Convenience function that combines parsing and JSON serialization.
 * Useful for CLI tools and build pipelines that need to generate
 * .kir files from .kry sources.
 *
 * @param source .kry source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* JSON string in KIR v3.0 format (caller must free), or NULL on error
 *
 * @example
 *   const char* kry = readFile("app.kry");
 *   char* kir_json = ir_kry_to_kir(kry, 0);
 *   if (kir_json) {
 *       writeFile("app.kir", kir_json);
 *       free(kir_json);
 *   }
 */
char* ir_kry_to_kir(const char* source, size_t length);

/**
 * Parse .kry and report errors
 *
 * Same as ir_kry_parse(), but also provides detailed error messages.
 *
 * @param source .kry source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @param error_message Output parameter - error message (caller must free)
 * @param error_line Output parameter - line number where error occurred
 * @param error_column Output parameter - column number where error occurred
 * @return IRComponent* Parsed tree, or NULL on error
 *
 * @example
 *   char* error = NULL;
 *   uint32_t line, col;
 *   IRComponent* root = ir_kry_parse_with_errors(kry, 0, &error, &line, &col);
 *   if (!root && error) {
 *       fprintf(stderr, "Parse error at %u:%u: %s\n", line, col, error);
 *       free(error);
 *   }
 */
IRComponent* ir_kry_parse_with_errors(const char* source, size_t length,
                                       char** error_message,
                                       uint32_t* error_line,
                                       uint32_t* error_column);

/**
 * Get parser version string
 *
 * @return const char* Version string (e.g., "1.0.0")
 */
const char* ir_kry_parser_version(void);

#ifdef __cplusplus
}
#endif

#endif // IR_KRY_PARSER_H
