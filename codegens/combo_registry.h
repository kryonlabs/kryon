/**
 * Kryon Combo Registry
 *
 * Registry for validating and managing language+toolkit combinations.
 * Ensures only valid combinations are allowed (e.g., tcl+tk is valid,
 * but tcl+draw is not).
 */

#ifndef COMBO_REGISTRY_H
#define COMBO_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include "toolkits/common/toolkit_registry.h"
#include "languages/common/language_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Combo Definition
// ============================================================================

/**
 * A language+toolkit combination
 */
typedef struct {
    const char* language;        // Language name (e.g., "tcl")
    const char* toolkit;         // Toolkit name (e.g., "tk")
    const char* alias;           // Optional alias for backward compat (e.g., "tcltk", "limbo")
    bool valid;                  // Whether this combination is valid
    const char* output_dir;      // Default output directory (or NULL)
} Combo;

/**
 * Combo resolution result
 */
typedef struct {
    Combo combo;
    const LanguageProfile* language_profile;
    const ToolkitProfile* toolkit_profile;
    bool resolved;               // Successfully resolved
    char error[256];             // Error message if not resolved
} ComboResolution;

// ============================================================================
// Combo Registry API
// ============================================================================

/**
 * Initialize combo registry
 * Must be called after language_registry_init() and toolkit_registry_init()
 */
void combo_registry_init(void);

/**
 * Cleanup combo registry
 */
void combo_registry_cleanup(void);

/**
 * Parse target string into language and toolkit components
 * Supports formats:
 *   "language+toolkit" - Explicit combo (e.g., "tcl+tk", "limbo+draw")
 *   "alias" - Predefined alias (e.g., "tcltk" → "tcl+tk", "limbo" → "limbo+draw")
 *
 * @param target Target string to parse
 * @param out_resolution Output for resolution result
 * @return true if successfully resolved, false otherwise
 */
bool combo_resolve(const char* target, ComboResolution* out_resolution);

/**
 * Check if a language+toolkit combination is valid
 *
 * @param language Language name
 * @param toolkit Toolkit name
 * @return true if valid combination, false otherwise
 */
bool combo_is_valid(const char* language, const char* toolkit);

/**
 * Check if a language+toolkit combination is valid (by type)
 *
 * @param language Language type
 * @param toolkit Toolkit type
 * @return true if valid combination, false otherwise
 */
bool combo_is_valid_typed(LanguageType language, ToolkitType toolkit);

/**
 * Check if a language+toolkit@platform combination is valid
 * Validates all three components: language exists, toolkit exists, platform exists,
 * platform supports the toolkit, platform supports the language, and the
 * language+toolkit combination is technically valid.
 *
 * @param language Language name
 * @param toolkit Toolkit name
 * @param platform Platform name
 * @return true if valid combination, false otherwise
 */
bool combo_is_valid_with_platform(const char* language, const char* toolkit, const char* platform);

/**
 * Get reason why a language+toolkit@platform combination is invalid
 * Returns detailed error message explaining what's wrong with the combination.
 *
 * @param language Language name
 * @param toolkit Toolkit name
 * @param platform Platform name
 * @return Error message, or NULL if combination is valid
 */
const char* combo_get_invalid_reason_with_platform(const char* language, const char* toolkit, const char* platform);

/**
 * Get all valid combinations for a specific language
 *
 * @param language Language name
 * @param out_combos Output array for combos
 * @param max_count Maximum number of combos to return
 * @return Number of combos returned
 */
size_t combo_get_for_language(const char* language, Combo* out_combos, size_t max_count);

/**
 * Get all valid combinations for a specific toolkit
 *
 * @param toolkit Toolkit name
 * @param out_combos Output array for combos
 * @param max_count Maximum number of combos to return
 * @return Number of combos returned
 */
size_t combo_get_for_toolkit(const char* toolkit, Combo* out_combos, size_t max_count);

/**
 * Get all valid combinations
 *
 * @param out_combos Output array for combos
 * @param max_count Maximum number of combos to return
 * @return Number of combos returned
 */
size_t combo_get_all(Combo* out_combos, size_t max_count);

/**
 * Get combo alias mapping
 * Returns the explicit target string for an alias
 *
 * @param alias Alias string (e.g., "tcltk", "limbo")
 * @return Explicit target string (e.g., "tcl+tk", "limbo+draw"), or NULL if not found
 */
const char* combo_get_alias(const char* alias);

/**
 * Register a custom combination
 * Allows adding new valid combos at runtime
 *
 * @param combo Combo to register
 * @return true on success, false on failure
 */
bool combo_register(const Combo* combo);

/**
 * Register an alias mapping
 *
 * @param alias Alias string
 * @param target Explicit target string (e.g., "tcl+tk")
 * @return true on success, false on failure
 */
bool combo_register_alias(const char* alias, const char* target);

/**
 * Format a combination as a string
 * Returns "language+toolkit" format
 *
 * @param combo Combo to format
 * @return Static buffer containing formatted string, or NULL on error
 */
const char* combo_format(const Combo* combo);

/**
 * Format language and toolkit as a string
 * Returns "language+toolkit" format
 *
 * @param language Language name
 * @param toolkit Toolkit name
 * @return Static buffer containing formatted string, or NULL on error
 */
const char* combo_format_parts(const char* language, const char* toolkit);

// ============================================================================
// CLI Helper Functions
// ============================================================================

/**
 * Parse and validate target string for CLI
 * Convenience function for CLI commands to parse target strings
 * Handles aliases and prints error messages to stderr
 *
 * @param target Target string (e.g., "limbo+draw", "tcltk", "limbo")
 * @param out_resolution Output for resolution result
 * @param print_errors Whether to print errors to stderr
 * @return true if successfully resolved, false otherwise
 */
bool combo_resolve_for_cli(const char* target, ComboResolution* out_resolution, bool print_errors);

/**
 * Print list of valid targets to stdout
 * For use with "kryon targets" command
 *
 * @param include_aliases Whether to include alias mappings
 */
void combo_print_valid_targets(bool include_aliases);

// ============================================================================
// Validation Matrix
// ============================================================================

/**
 * Print validation matrix to stdout
 * Shows all valid/invalid language+toolkit combinations
 */
void combo_print_matrix(void);

/**
 * Auto-resolve a language to its only valid platform+toolkit combination
 * If the language has exactly one valid platform+toolkit combo, returns it.
 * If multiple or none, returns NULL for outputs.
 *
 * @param language Language name to auto-resolve
 * @param out_toolkit Output pointer for toolkit name (static storage)
 * @param out_platform Output pointer for platform name (static storage)
 * @return true if exactly one valid combination found, false otherwise
 */
bool combo_auto_resolve_language(const char* language, const char** out_toolkit, const char** out_platform);

/**
 * Get reason why a combination is invalid
 *
 * @param language Language name
 * @param toolkit Toolkit name
 * @return Error message, or NULL if combination is valid
 */
const char* combo_get_invalid_reason(const char* language, const char* toolkit);

#ifdef __cplusplus
}
#endif

#endif // COMBO_REGISTRY_H
