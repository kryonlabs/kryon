/**
 * @file parser_interface.h
 * @brief Unified Parser Interface for Kryon IR System
 *
 * Defines a standard interface that all parsers must implement.
 * This ensures consistent error handling and API patterns across all frontends.
 *
 * Standard Parser Pattern:
 * 1. All parsers return cJSON* (KIR JSON structure)
 * 2. Consistent error reporting to stderr
 * 3. File I/O errors handled before parsing
 * 4. NULL return indicates parsing failure
 */

#ifndef PARSER_INTERFACE_H
#define PARSER_INTERFACE_H

#include <cJSON.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Parser Interface
 * ============================================================================ */

/**
 * @brief Parser capability flags
 */
typedef enum {
    PARSER_CAP_NONE = 0,
    PARSER_CAP_MULTI_FILE = (1 << 0),  // Supports multi-file imports
    PARSER_CAP_LAYOUT = (1 << 1),      // Supports layout attributes
    PARSER_CAP_STYLING = (1 << 2),     // Supports styling
    PARSER_CAP_SCRIPTS = (1 << 3)       // Supports event scripts
} ParserCapabilities;

/**
 * @brief Parser metadata
 */
typedef struct {
    const char* name;           // Parser name (e.g., "KRY", "Tcl")
    const char* version;        // Parser version
    ParserCapabilities caps;    // Capability flags
    const char** extensions;    // Supported file extensions
} ParserInfo;

/**
 * @brief Parser interface vtable
 *
 * All parsers must implement these functions following the standard pattern.
 */
typedef struct {
    /**
     * Get parser metadata
     * @return Parser info (static, do not free)
     */
    const ParserInfo* (*get_info)(void);

    /**
     * Parse file to KIR
     * @param filepath Path to source file
     * @return KIR JSON structure, or NULL on failure
     *
     * Standard error handling:
     * - File not found: fprintf(stderr, "Error: Cannot open file: %s\n", filepath)
     * - Parse error: fprintf(stderr, "Error: Failed to parse <format>: <reason>\n")
     * - Invalid JSON: fprintf(stderr, "Error: Invalid KIR JSON generated\n")
     */
    cJSON* (*parse_file)(const char* filepath);

    /**
     * Validate source file (optional)
     * @param filepath Path to source file
     * @return true if valid, false otherwise
     *
     * Provides pre-parsing validation without generating KIR.
     * Can be NULL if parser doesn't support validation.
     */
    bool (*validate)(const char* filepath);

    /**
     * Cleanup parser resources (optional)
     * Called when parser is unloaded (for dynamic loading)
     */
    void (*cleanup)(void);

} ParserInterface;

/* ============================================================================
 * Parser Registry
 * ============================================================================ */

/**
 * Register a parser
 * @param parser Parser interface (must remain valid for program lifetime)
 * @return 0 on success, non-zero on error
 */
int parser_register(const ParserInterface* parser);

/**
 * Get parser by format name
 * @param format Format name (e.g., "kry", "tcl", "limbo")
 * @return Parser interface, or NULL if not found
 */
const ParserInterface* parser_get_interface(const char* format);

/**
 * Get parser by file extension
 * @param extension File extension (e.g., ".kry", ".tcl")
 * @return Parser interface, or NULL if not found
 */
const ParserInterface* parser_get_interface_by_ext(const char* extension);

/**
 * List all registered parsers
 * @return NULL-terminated array of parser interfaces
 */
const ParserInterface** parser_list_all(void);

/* ============================================================================
 ============================================================================
 * Standard Parser Implementations
 * ============================================================================ */

/**
 * Parse file using appropriate parser based on extension
 * @param filepath Path to source file
 * @return KIR JSON structure, or NULL on failure
 */
cJSON* parser_parse_file(const char* filepath);

/**
 * Parse file using specific parser
 * @param filepath Path to source file
 * @param format Format name (e.g., "kry", "tcl")
 * @return KIR JSON structure, or NULL on failure
 */
cJSON* parser_parse_file_as(const char* filepath, const char* format);

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * Standard error messages for parsers
 */
#define PARSER_ERROR_FILE_NOT_FOUND(filepath) \
    fprintf(stderr, "Error: Cannot open file: %s\n", filepath)

#define PARSER_ERROR_PARSE_FAILED(format, reason) \
    fprintf(stderr, "Error: Failed to parse %s: %s\n", format, reason)

#define PARSER_ERROR_INVALID_JSON(format) \
    fprintf(stderr, "Error: Invalid KIR JSON generated from %s\n", format)

/**
 * Declare a standard parser implementation
 */
#define DECLARE_PARSER(name_str, version_str, caps_val, ext_list) \
    static const ParserInfo name##_parser_info = { \
        .name = name_str, \
        .version = version_str, \
        .caps = caps_val, \
        .extensions = ext_list \
    }; \
    static const ParserInterface name##_parser_interface = { \
        .get_info = name##_parser_get_info, \
        .parse_file = name##_parse_file, \
        .validate = NULL, \
        .cleanup = NULL \
    }

#ifdef __cplusplus
}
#endif

#endif // PARSER_INTERFACE_H
