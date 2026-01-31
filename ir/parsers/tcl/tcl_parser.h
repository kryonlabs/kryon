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

#endif /* TCL_PARSER_H */
