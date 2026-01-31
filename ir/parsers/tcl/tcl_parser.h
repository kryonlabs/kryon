/**
 * Tcl/Tk Parser
 * Parses Tcl/Tk source files and converts to KIR (Kryon Intermediate Representation)
 *
 * This is the reverse of the Tcl/Tk code generator - it takes generated Tcl code
 * and reconstructs KIR JSON, enabling round-trip conversion:
 *   KRY → KIR → Tcl → KIR → KRY
 */

#ifndef TCL_PARSER_H
#define TCL_PARSER_H

#include <stdbool.h>
#include "../../../third_party/cJSON/cJSON.h"
#include "../../include/ir_core.h"
#include <stdint.h>

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * Parse Tcl/Tk source code and convert to KIR
 *
 * @param tcl_source The Tcl/Tk source code as a string
 * @return cJSON* KIR JSON object (caller must free with cJSON_Delete), or NULL on error
 */
cJSON* tcl_parse_to_kir(const char* tcl_source);

/**
 * Parse Tcl/Tk file and convert to KIR
 *
 * @param filepath Path to the Tcl/Tk file
 * @return cJSON* KIR JSON object (caller must free with cJSON_Delete), or NULL on error
 */
cJSON* tcl_parse_file_to_kir(const char* filepath);

/**
 * Set parser options
 */
typedef struct {
    bool preserve_comments;     // Preserve comments as metadata
    bool strict_mode;           // Fail on warnings instead of continuing
    bool debug_mode;            // Add debug metadata to output
} TclParserOptions;

/**
 * Parse with options
 *
 * @param tcl_source The Tcl/Tk source code
 * @param options Parser options (can be NULL for defaults)
 * @return cJSON* KIR JSON object, or NULL on error
 */
cJSON* tcl_parse_to_kir_ex(const char* tcl_source, const TclParserOptions* options);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

typedef enum {
    TCL_PARSE_OK = 0,
    TCL_PARSE_ERROR_SYNTAX,
    TCL_PARSE_ERROR_LEXICAL,
    TCL_PARSE_ERROR_SEMANTIC,
    TCL_PARSE_ERROR_MEMORY,
    TCL_PARSE_ERROR_IO
} TclParseError;

/**
 * Get last error message
 *
 * @return const char* Error message, or NULL if no error
 */
const char* tcl_parse_get_last_error(void);

/**
 * Get last error code
 *
 * @return TclParseError Error code
 */
TclParseError tcl_parse_get_last_error_code(void);

/**
 * Set error (internal use)
 */
void set_error(TclParseError code, const char* message);

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * Validate Tcl syntax without full parsing
 *
 * @param tcl_source The Tcl/Tk source code
 * @return true if syntax appears valid, false otherwise
 */
bool tcl_validate_syntax(const char* tcl_source);

/**
 * Extract widget tree from Tcl source (for debugging/inspection)
 *
 * @param tcl_source The Tcl/Tk source code
 * @return cJSON* Widget tree JSON, or NULL on error
 */
cJSON* tcl_extract_widget_tree(const char* tcl_source);

/* ============================================================================
 * Native IR API (matches KRY parser pattern)
 * ============================================================================ */

/**
 * Parse Tcl/Tk source to IR component tree
 *
 * Converts Tcl/Tk source into a native Kryon IR component hierarchy.
 * Matches the API pattern of ir_kry_parse().
 *
 * @param source Tcl/Tk source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return IRComponent* Root component, or NULL on error
 *
 * @note The caller is responsible for freeing the returned component tree
 *       using ir_destroy_component().
 */
IRComponent* ir_tcl_parse(const char* source, size_t length);

/**
 * Convert Tcl/Tk source to KIR JSON string
 *
 * Convenience function that combines parsing and JSON serialization.
 * Matches the API pattern of ir_kry_to_kir().
 *
 * @param source Tcl/Tk source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* JSON string in KIR format (caller must free), or NULL on error
 */
char* ir_tcl_to_kir(const char* source, size_t length);

/**
 * Parse Tcl/Tk and report errors
 *
 * Same as ir_tcl_parse(), but also provides detailed error messages.
 *
 * @param source Tcl/Tk source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @param error_message Output parameter - error message (caller must free)
 * @param error_line Output parameter - line number where error occurred
 * @param error_column Output parameter - column number where error occurred
 * @return IRComponent* Parsed tree, or NULL on error
 */
IRComponent* ir_tcl_parse_with_errors(const char* source, size_t length,
                                     char** error_message,
                                     uint32_t* error_line,
                                     uint32_t* error_column);

/**
 * Get parser version string
 *
 * @return const char* Version string (e.g., "1.0.0")
 */
const char* ir_tcl_parser_version(void);

/**
 * Convert Tcl/Tk file to KIR JSON string
 *
 * Convenience function that reads a file and converts to KIR JSON.
 *
 * @param filepath Path to Tcl/Tk file
 * @return char* JSON string in KIR format (caller must free), or NULL on error
 */
char* ir_tcl_to_kir_file(const char* filepath);

#endif /* TCL_PARSER_H */
