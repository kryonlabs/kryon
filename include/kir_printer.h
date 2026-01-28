/**
 * @file kir_printer.h
 * @brief KIR Pretty-Printer - Generate readable .kry source from KIR/AST
 *
 * This module generates formatted Kryon source code (.kry) from KIR files or AST,
 * enabling:
 * - Source code recovery from KIR intermediate representation
 * - Code formatting and normalization
 * - Full round-trip: kry → kir → kry
 * - Human-readable output from compiled binaries (via krb → kir → kry)
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_KIR_PRINTER_H
#define KRYON_KIR_PRINTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include "parser.h"

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

/**
 * @brief KIR printer context
 */
typedef struct KryonKIRPrinter KryonKIRPrinter;

/**
 * @brief Printer configuration
 */
typedef struct {
    size_t indent_size;             ///< Number of spaces per indent level (default: 2)
    bool use_tabs;                  ///< Use tabs instead of spaces for indentation
    bool compact_mode;              ///< Minimize whitespace (no blank lines)
    bool preserve_comments;         ///< Preserve comments (if available in AST)
    bool explicit_types;            ///< Include type annotations where helpful
    bool sort_properties;           ///< Sort properties alphabetically
    size_t max_line_length;         ///< Maximum line length before wrapping (0 = no limit)
} KryonPrinterConfig;

/**
 * @brief Printer statistics
 */
typedef struct {
    size_t elements_printed;        ///< Number of elements generated
    size_t properties_printed;      ///< Number of properties generated
    size_t lines_generated;         ///< Total lines of source code
    size_t total_bytes;             ///< Total bytes of generated source
} KryonPrinterStats;

// =============================================================================
// CORE API
// =============================================================================

/**
 * @brief Create a new KIR printer
 * @param config Configuration (NULL for defaults)
 * @return Printer instance, or NULL on failure
 */
KryonKIRPrinter *kryon_printer_create(const KryonPrinterConfig *config);

/**
 * @brief Destroy a KIR printer
 * @param printer Printer to destroy
 */
void kryon_printer_destroy(KryonKIRPrinter *printer);

/**
 * @brief Print AST to .kry source code file
 * @param printer The printer
 * @param ast AST root node
 * @param output_path Output file path
 * @return true on success, false on failure
 */
bool kryon_printer_print_file(KryonKIRPrinter *printer,
                              const KryonASTNode *ast,
                              const char *output_path);

/**
 * @brief Print AST to .kry source code string
 * @param printer The printer
 * @param ast AST root node
 * @param out_source Output source code string (caller must free)
 * @return true on success, false on failure
 */
bool kryon_printer_print_string(KryonKIRPrinter *printer,
                                const KryonASTNode *ast,
                                char **out_source);

/**
 * @brief Print KIR file to .kry source code file
 * @param printer The printer
 * @param kir_file_path Path to KIR file
 * @param output_path Output file path
 * @return true on success, false on failure
 *
 * Convenience function that reads KIR, parses to AST, and prints.
 */
bool kryon_printer_kir_to_source(KryonKIRPrinter *printer,
                                 const char *kir_file_path,
                                 const char *output_path);

// =============================================================================
// QUERY AND DIAGNOSTICS
// =============================================================================

/**
 * @brief Get printer error messages
 * @param printer The printer
 * @param count Output: number of errors
 * @return Array of error messages
 */
const char **kryon_printer_get_errors(const KryonKIRPrinter *printer,
                                      size_t *count);

/**
 * @brief Get printer statistics
 * @param printer The printer
 * @return Statistics structure
 */
const KryonPrinterStats *kryon_printer_get_stats(const KryonKIRPrinter *printer);

/**
 * @brief Get default printer configuration
 * @return Default configuration
 */
KryonPrinterConfig kryon_printer_default_config(void);

/**
 * @brief Get compact printer configuration (minimal whitespace)
 * @return Compact configuration
 */
KryonPrinterConfig kryon_printer_compact_config(void);

/**
 * @brief Get readable printer configuration (generous whitespace)
 * @return Readable configuration
 */
KryonPrinterConfig kryon_printer_readable_config(void);

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief One-shot printing (AST → source string)
 * @param ast AST root node
 * @param out_source Output source code string (caller must free)
 * @return true on success, false on failure
 *
 * Uses default configuration.
 */
bool kryon_print_ast_simple(const KryonASTNode *ast, char **out_source);

/**
 * @brief One-shot printing (KIR file → source file)
 * @param kir_file_path Path to KIR file
 * @param output_path Output file path
 * @return true on success, false on failure
 *
 * Uses default configuration.
 */
bool kryon_print_kir_simple(const char *kir_file_path, const char *output_path);

#ifdef __cplusplus
}
#endif

#endif // KRYON_KIR_PRINTER_H
