/**
 * @file kir_format.h
 * @brief Kryon Intermediate Representation (KIR) Format API
 *
 * KIR is the mandatory intermediate format in the Kryon compilation pipeline.
 * All compilations go through: kry → kir → krb
 * Full bidirectional support: kry ↔ kir ↔ krb
 *
 * KIR Format:
 * - JSON-based for machine readability and AI/LLM compatibility
 * - Lossless representation of post-expansion AST
 * - Complete metadata for decompilation
 * - Supports full round-trip transformations
 *
 * @version 0.1.0
 * @author Kryon Labs
 */

#ifndef KRYON_KIR_FORMAT_H
#define KRYON_KIR_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "parser.h"

// =============================================================================
// KIR VERSION
// =============================================================================

#define KIR_FORMAT_VERSION_MAJOR 0
#define KIR_FORMAT_VERSION_MINOR 1
#define KIR_FORMAT_VERSION_PATCH 0
#define KIR_FORMAT_VERSION "0.1.0"
#define KIR_FORMAT_NAME "kir-json"

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonKIRWriter KryonKIRWriter;
typedef struct KryonKIRReader KryonKIRReader;
typedef struct KryonKIRConfig KryonKIRConfig;
typedef struct KryonKIRPrinter KryonKIRPrinter;
typedef struct KryonKIRPrinterConfig KryonKIRPrinterConfig;

// =============================================================================
// KIR CONFIGURATION
// =============================================================================

/**
 * @brief KIR output format options
 */
typedef enum {
    KRYON_KIR_FORMAT_COMPACT,    // Minified JSON (no whitespace)
    KRYON_KIR_FORMAT_READABLE,   // Pretty-printed with 2-space indentation
    KRYON_KIR_FORMAT_VERBOSE     // Readable with additional comments
} KryonKIRFormatStyle;

/**
 * @brief KIR configuration options
 */
struct KryonKIRConfig {
    KryonKIRFormatStyle format_style;  // Output formatting style
    bool include_location_info;         // Include source location metadata
    bool include_node_ids;              // Include unique node IDs
    bool include_timestamps;            // Include generation timestamp
    bool include_compiler_info;         // Include compiler version info
    bool validate_on_write;             // Validate AST before writing
    bool pretty_print;                  // Alias for READABLE format
    const char *source_file;            // Original source filename
};

// =============================================================================
// KIR WRITER API
// =============================================================================

/**
 * @brief Create KIR writer
 * @param config Writer configuration (NULL for defaults)
 * @return Pointer to KIR writer, or NULL on failure
 */
KryonKIRWriter *kryon_kir_writer_create(const KryonKIRConfig *config);

/**
 * @brief Destroy KIR writer
 * @param writer KIR writer to destroy
 */
void kryon_kir_writer_destroy(KryonKIRWriter *writer);

/**
 * @brief Write AST to KIR file
 * @param writer KIR writer
 * @param ast_root Root AST node (post-expansion)
 * @param output_path Output file path
 * @return true on success, false on error
 */
bool kryon_kir_write_file(KryonKIRWriter *writer, const KryonASTNode *ast_root,
                          const char *output_path);

/**
 * @brief Write AST to KIR string
 * @param writer KIR writer
 * @param ast_root Root AST node (post-expansion)
 * @param out_json Output pointer for JSON string (caller must free)
 * @return true on success, false on error
 */
bool kryon_kir_write_string(KryonKIRWriter *writer, const KryonASTNode *ast_root,
                            char **out_json);

/**
 * @brief Write AST to file stream
 * @param writer KIR writer
 * @param ast_root Root AST node (post-expansion)
 * @param file Output file stream
 * @return true on success, false on error
 */
bool kryon_kir_write_stream(KryonKIRWriter *writer, const KryonASTNode *ast_root,
                            FILE *file);

/**
 * @brief Get writer errors
 * @param writer KIR writer
 * @param out_count Output for error count
 * @return Array of error messages
 */
const char **kryon_kir_writer_get_errors(const KryonKIRWriter *writer, size_t *out_count);

// =============================================================================
// KIR READER API
// =============================================================================

/**
 * @brief Create KIR reader
 * @param config Reader configuration (NULL for defaults)
 * @return Pointer to KIR reader, or NULL on failure
 */
KryonKIRReader *kryon_kir_reader_create(const KryonKIRConfig *config);

/**
 * @brief Destroy KIR reader
 * @param reader KIR reader to destroy
 */
void kryon_kir_reader_destroy(KryonKIRReader *reader);

/**
 * @brief Read KIR file and reconstruct AST
 * @param reader KIR reader
 * @param input_path Input KIR file path
 * @param out_ast Output pointer for reconstructed AST
 * @return true on success, false on error
 */
bool kryon_kir_read_file(KryonKIRReader *reader, const char *input_path,
                         KryonASTNode **out_ast);

/**
 * @brief Read KIR from string and reconstruct AST
 * @param reader KIR reader
 * @param json_string KIR JSON string
 * @param out_ast Output pointer for reconstructed AST
 * @return true on success, false on error
 */
bool kryon_kir_read_string(KryonKIRReader *reader, const char *json_string,
                           KryonASTNode **out_ast);

/**
 * @brief Read KIR from file stream and reconstruct AST
 * @param reader KIR reader
 * @param file Input file stream
 * @param out_ast Output pointer for reconstructed AST
 * @return true on success, false on error
 */
bool kryon_kir_read_stream(KryonKIRReader *reader, FILE *file,
                           KryonASTNode **out_ast);

/**
 * @brief Get reader errors
 * @param reader KIR reader
 * @param out_count Output for error count
 * @return Array of error messages
 */
const char **kryon_kir_reader_get_errors(const KryonKIRReader *reader, size_t *out_count);

/**
 * @brief Get KIR format version from file
 * @param reader KIR reader
 * @param input_path Input KIR file path
 * @param major Output for major version
 * @param minor Output for minor version
 * @param patch Output for patch version
 * @return true on success, false on error
 */
bool kryon_kir_get_version(KryonKIRReader *reader, const char *input_path,
                           int *major, int *minor, int *patch);

// =============================================================================
// KIR PRINTER API (KIR → KRY source generation)
// =============================================================================

/**
 * @brief KIR printer formatting options
 */
typedef enum {
    KRYON_KIR_PRINT_COMPACT,     // Minimal whitespace
    KRYON_KIR_PRINT_READABLE,    // Standard formatting (2-space indent)
    KRYON_KIR_PRINT_VERBOSE      // Include expansion comments
} KryonKIRPrintStyle;

/**
 * @brief Create KIR printer
 * @param config Printer configuration (NULL for defaults)
 * @return Pointer to KIR printer, or NULL on failure
 */
KryonKIRPrinter *kryon_kir_printer_create(const struct KryonKIRPrinterConfig *config);

/**
 * @brief Destroy KIR printer
 * @param printer KIR printer to destroy
 */
void kryon_kir_printer_destroy(KryonKIRPrinter *printer);

/**
 * @brief Print KIR AST to source code file
 * @param printer KIR printer
 * @param ast_root Root AST node (from KIR)
 * @param output_path Output .kry file path
 * @return true on success, false on error
 */
bool kryon_kir_print_file(KryonKIRPrinter *printer, const KryonASTNode *ast_root,
                          const char *output_path);

/**
 * @brief Print KIR AST to source code string
 * @param printer KIR printer
 * @param ast_root Root AST node (from KIR)
 * @param out_source Output pointer for source string (caller must free)
 * @return true on success, false on error
 */
bool kryon_kir_print_string(KryonKIRPrinter *printer, const KryonASTNode *ast_root,
                            char **out_source);

/**
 * @brief Print KIR AST to file stream
 * @param printer KIR printer
 * @param ast_root Root AST node (from KIR)
 * @param file Output file stream
 * @return true on success, false on error
 */
bool kryon_kir_print_stream(KryonKIRPrinter *printer, const KryonASTNode *ast_root,
                            FILE *file);

// =============================================================================
// KIR VALIDATION API
// =============================================================================

/**
 * @brief Validate KIR file structure
 * @param input_path Input KIR file path
 * @param errors Output buffer for error messages
 * @param max_errors Maximum errors to report
 * @return Number of errors found (0 = valid)
 */
size_t kryon_kir_validate_file(const char *input_path, char **errors, size_t max_errors);

/**
 * @brief Validate KIR JSON string
 * @param json_string KIR JSON string
 * @param errors Output buffer for error messages
 * @param max_errors Maximum errors to report
 * @return Number of errors found (0 = valid)
 */
size_t kryon_kir_validate_string(const char *json_string, char **errors, size_t max_errors);

/**
 * @brief Check KIR format version compatibility
 * @param input_path Input KIR file path
 * @param min_major Minimum supported major version
 * @param min_minor Minimum supported minor version
 * @return true if compatible, false otherwise
 */
bool kryon_kir_check_version_compat(const char *input_path, int min_major, int min_minor);

// =============================================================================
// CONFIGURATION HELPERS
// =============================================================================

/**
 * @brief Get default KIR configuration
 * @return Default configuration
 */
KryonKIRConfig kryon_kir_default_config(void);

/**
 * @brief Get compact KIR configuration
 * @return Configuration for compact output
 */
KryonKIRConfig kryon_kir_compact_config(void);

/**
 * @brief Get verbose KIR configuration
 * @return Configuration for verbose output
 */
KryonKIRConfig kryon_kir_verbose_config(void);

/**
 * @brief Get default KIR printer configuration
 * @param config Output pointer for configuration structure
 */
void kryon_kir_default_printer_config(struct KryonKIRPrinterConfig *config);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Detect if file is KIR format
 * @param file_path File path to check
 * @return true if file is KIR format, false otherwise
 */
bool kryon_is_kir_file(const char *file_path);

/**
 * @brief Get recommended KIR output path from source path
 * @param source_path Source .kry file path
 * @return Allocated KIR output path (caller must free), or NULL on error
 */
char *kryon_kir_get_output_path(const char *source_path);

/**
 * @brief Compare two KIR files for semantic equivalence
 * @param file1_path First KIR file path
 * @param file2_path Second KIR file path
 * @param ignore_metadata Ignore metadata differences
 * @param ignore_locations Ignore source location differences
 * @return 0 if semantically equivalent, >0 if different, <0 on error
 */
int kryon_kir_compare_files(const char *file1_path, const char *file2_path,
                            bool ignore_metadata, bool ignore_locations);

#ifdef __cplusplus
}
#endif

#endif // KRYON_KIR_FORMAT_H
