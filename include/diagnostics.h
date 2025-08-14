/**
 * @file diagnostics.h
 * @brief Kryon Compiler Diagnostics System
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#ifndef KRYON_INTERNAL_DIAGNOSTICS_H
#define KRYON_INTERNAL_DIAGNOSTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "lexer.h"

// =============================================================================
// DIAGNOSTIC TYPES
// =============================================================================

/**
 * @brief Diagnostic severity levels
 */
typedef enum {
    KRYON_DIAG_NOTE = 0,        // Informational note
    KRYON_DIAG_WARNING,         // Warning that doesn't prevent compilation
    KRYON_DIAG_ERROR,           // Error that prevents successful compilation
    KRYON_DIAG_FATAL,           // Fatal error that stops compilation
    KRYON_DIAG_INFO             // General information
} KryonDiagnosticSeverity;

/**
 * @brief Individual diagnostic message
 */
typedef struct {
    KryonDiagnosticSeverity severity;  // Severity level
    char *message;                     // Diagnostic message
    char *suggestion;                  // Optional suggestion for fix
    KryonSourceLocation location;      // Source location of issue
    char *source_filename;             // Source filename (copy)
    time_t timestamp;                  // When diagnostic was created
} KryonDiagnostic;

/**
 * @brief Diagnostic manager configuration
 */
typedef struct {
    bool enable_colors;         // Use ANSI colors in output
    bool show_source_context;   // Show source code context
    size_t context_lines;       // Number of context lines to show
    size_t max_diagnostics;     // Maximum number of diagnostics to store
    size_t error_limit;         // Stop after this many errors
    size_t warning_limit;       // Stop after this many warnings
} KryonDiagnosticConfig;

/**
 * @brief Diagnostic manager
 */
typedef struct {
    KryonDiagnostic *diagnostics;      // Array of diagnostics
    size_t diagnostic_count;           // Number of diagnostics
    size_t diagnostic_capacity;        // Capacity of diagnostic array
    
    // Counters by severity
    size_t error_count;                // Number of errors
    size_t warning_count;              // Number of warnings
    size_t note_count;                 // Number of notes/info
    
    // Configuration
    KryonDiagnosticConfig config;      // Manager configuration
} KryonDiagnosticManager;

// =============================================================================
// DIAGNOSTIC MANAGER API
// =============================================================================

/**
 * @brief Create diagnostic manager
 * @return Pointer to manager, or NULL on failure
 */
KryonDiagnosticManager *kryon_diagnostic_create(void);

/**
 * @brief Destroy diagnostic manager
 * @param manager Manager to destroy
 */
void kryon_diagnostic_destroy(KryonDiagnosticManager *manager);

/**
 * @brief Report a diagnostic
 * @param manager Diagnostic manager
 * @param severity Severity level
 * @param location Source location (can be NULL)
 * @param message Diagnostic message
 * @param suggestion Optional suggestion (can be NULL)
 */
void kryon_diagnostic_report(KryonDiagnosticManager *manager,
                            KryonDiagnosticSeverity severity,
                            const KryonSourceLocation *location,
                            const char *message,
                            const char *suggestion);

/**
 * @brief Report a formatted diagnostic
 * @param manager Diagnostic manager
 * @param severity Severity level
 * @param location Source location (can be NULL)
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void kryon_diagnostic_reportf(KryonDiagnosticManager *manager,
                             KryonDiagnosticSeverity severity,
                             const KryonSourceLocation *location,
                             const char *format, ...);

// =============================================================================
// DIAGNOSTIC OUTPUT
// =============================================================================

/**
 * @brief Print a single diagnostic
 * @param manager Diagnostic manager
 * @param diag Diagnostic to print
 * @param file Output file
 */
void kryon_diagnostic_print(const KryonDiagnosticManager *manager,
                           const KryonDiagnostic *diag,
                           FILE *file);

/**
 * @brief Print all diagnostics
 * @param manager Diagnostic manager
 * @param file Output file
 */
void kryon_diagnostic_print_all(const KryonDiagnosticManager *manager, FILE *file);

/**
 * @brief Print diagnostic summary
 * @param manager Diagnostic manager
 * @param file Output file
 */
void kryon_diagnostic_print_summary(const KryonDiagnosticManager *manager, FILE *file);

// =============================================================================
// DIAGNOSTIC QUERIES
// =============================================================================

/**
 * @brief Get number of diagnostics by severity
 * @param manager Diagnostic manager
 * @param severity Severity level
 * @return Number of diagnostics with that severity
 */
size_t kryon_diagnostic_get_count(const KryonDiagnosticManager *manager,
                                 KryonDiagnosticSeverity severity);

/**
 * @brief Check if there are any errors
 * @param manager Diagnostic manager
 * @return true if there are errors, false otherwise
 */
bool kryon_diagnostic_has_errors(const KryonDiagnosticManager *manager);

/**
 * @brief Check if there are any warnings
 * @param manager Diagnostic manager
 * @return true if there are warnings, false otherwise
 */
bool kryon_diagnostic_has_warnings(const KryonDiagnosticManager *manager);

/**
 * @brief Get all diagnostics
 * @param manager Diagnostic manager
 * @param out_count Output for diagnostic count
 * @return Array of diagnostics
 */
const KryonDiagnostic *kryon_diagnostic_get_all(const KryonDiagnosticManager *manager,
                                               size_t *out_count);

// =============================================================================
// CONFIGURATION
// =============================================================================

/**
 * @brief Set diagnostic configuration
 * @param manager Diagnostic manager
 * @param config Configuration to set
 */
void kryon_diagnostic_set_config(KryonDiagnosticManager *manager,
                                 const KryonDiagnosticConfig *config);

/**
 * @brief Get diagnostic configuration
 * @param manager Diagnostic manager
 * @return Pointer to configuration
 */
const KryonDiagnosticConfig *kryon_diagnostic_get_config(const KryonDiagnosticManager *manager);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Clear all diagnostics
 * @param manager Diagnostic manager
 */
void kryon_diagnostic_clear(KryonDiagnosticManager *manager);

// =============================================================================
// CONVENIENCE MACROS
// =============================================================================

#define KRYON_DIAG_ERROR_AT(manager, location, msg) \
    kryon_diagnostic_report(manager, KRYON_DIAG_ERROR, location, msg, NULL)

#define KRYON_DIAG_WARNING_AT(manager, location, msg) \
    kryon_diagnostic_report(manager, KRYON_DIAG_WARNING, location, msg, NULL)

#define KRYON_DIAG_NOTE_AT(manager, location, msg) \
    kryon_diagnostic_report(manager, KRYON_DIAG_NOTE, location, msg, NULL)

#define KRYON_DIAG_ERRORF_AT(manager, location, fmt, ...) \
    kryon_diagnostic_reportf(manager, KRYON_DIAG_ERROR, location, fmt, __VA_ARGS__)

#define KRYON_DIAG_WARNINGF_AT(manager, location, fmt, ...) \
    kryon_diagnostic_reportf(manager, KRYON_DIAG_WARNING, location, fmt, __VA_ARGS__)

#define KRYON_DIAG_NOTEF_AT(manager, location, fmt, ...) \
    kryon_diagnostic_reportf(manager, KRYON_DIAG_NOTE, location, fmt, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_DIAGNOSTICS_H