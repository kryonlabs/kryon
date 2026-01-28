/**
 * @file krb_decompiler.h
 * @brief KRB Decompiler - Reconstruct AST from KRB binary format
 *
 * This module decompiles Kryon Binary (KRB) files back to Abstract Syntax Tree (AST)
 * representation, enabling:
 * - Binary inspection and debugging
 * - Round-trip verification (kry → krb → kir → kry)
 * - KIR generation from existing binaries
 * - Source code recovery from compiled files
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_KRB_DECOMPILER_H
#define KRYON_KRB_DECOMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include "parser.h"
#include "krb_format.h"

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

/**
 * @brief KRB decompiler context
 */
typedef struct KryonKrbDecompiler KryonKrbDecompiler;

/**
 * @brief Decompiler configuration
 */
typedef struct {
    bool preserve_ids;              ///< Preserve element IDs in AST metadata
    bool reconstruct_locations;     ///< Generate synthetic location info
    bool expand_templates;          ///< Expand @for/@if templates during decompilation
    bool include_metadata;          ///< Include decompilation metadata in AST
    const char *source_hint;        ///< Optional source file hint for locations
} KryonDecompilerConfig;

/**
 * @brief Decompiler statistics
 */
typedef struct {
    size_t elements_decompiled;     ///< Number of elements reconstructed
    size_t properties_decompiled;   ///< Number of properties reconstructed
    size_t templates_decompiled;    ///< Number of templates reconstructed
    size_t strings_recovered;       ///< Number of strings from string table
    size_t total_ast_nodes;         ///< Total AST nodes created
} KryonDecompilerStats;

// =============================================================================
// CORE API
// =============================================================================

/**
 * @brief Create a new KRB decompiler
 * @param config Configuration (NULL for defaults)
 * @return Decompiler instance, or NULL on failure
 */
KryonKrbDecompiler *kryon_decompiler_create(const KryonDecompilerConfig *config);

/**
 * @brief Destroy a KRB decompiler
 * @param decompiler Decompiler to destroy
 */
void kryon_decompiler_destroy(KryonKrbDecompiler *decompiler);

/**
 * @brief Decompile KRB file to AST
 * @param decompiler The decompiler
 * @param krb_file_path Path to KRB file
 * @param out_ast Output AST root node
 * @return true on success, false on failure
 */
bool kryon_decompile_file(KryonKrbDecompiler *decompiler,
                          const char *krb_file_path,
                          KryonASTNode **out_ast);

/**
 * @brief Decompile KRB from memory buffer
 * @param decompiler The decompiler
 * @param krb_data KRB binary data
 * @param krb_size Size of KRB data
 * @param out_ast Output AST root node
 * @return true on success, false on failure
 */
bool kryon_decompile_memory(KryonKrbDecompiler *decompiler,
                            const uint8_t *krb_data,
                            size_t krb_size,
                            KryonASTNode **out_ast);

/**
 * @brief Decompile from parsed KRB file structure
 * @param decompiler The decompiler
 * @param krb_file Parsed KRB file
 * @param out_ast Output AST root node
 * @return true on success, false on failure
 */
bool kryon_decompile_krb(KryonKrbDecompiler *decompiler,
                         const KryonKrbFile *krb_file,
                         KryonASTNode **out_ast);

// =============================================================================
// QUERY AND DIAGNOSTICS
// =============================================================================

/**
 * @brief Get decompiler error messages
 * @param decompiler The decompiler
 * @param count Output: number of errors
 * @return Array of error messages
 */
const char **kryon_decompiler_get_errors(const KryonKrbDecompiler *decompiler,
                                         size_t *count);

/**
 * @brief Get decompilation statistics
 * @param decompiler The decompiler
 * @return Statistics structure
 */
const KryonDecompilerStats *kryon_decompiler_get_stats(const KryonKrbDecompiler *decompiler);

/**
 * @brief Get default decompiler configuration
 * @return Default configuration
 */
KryonDecompilerConfig kryon_decompiler_default_config(void);

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief One-shot decompilation (file → AST)
 * @param krb_file_path Path to KRB file
 * @param out_ast Output AST root node
 * @return true on success, false on failure
 *
 * Convenience function that creates decompiler, decompiles, and destroys.
 * Use the full API for more control and error handling.
 */
bool kryon_decompile_simple(const char *krb_file_path, KryonASTNode **out_ast);

#ifdef __cplusplus
}
#endif

#endif // KRYON_KRB_DECOMPILER_H
