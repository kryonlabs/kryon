/**
 * Kryon Codegen Target Helper
 *
 * Helper functions for CLI commands to parse and validate target strings.
 * Provides a convenient interface to the combo_registry for CLI code.
 */

#ifndef CODEGEN_TARGET_H
#define CODEGEN_TARGET_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parsed target information
 * Returned by codegen_parse_target() for use in CLI commands
 */
typedef struct {
    const char* language;              // Language name (e.g., "limbo", "tcl")
    const char* toolkit;               // Toolkit name (e.g., "draw", "tk")
    const char* platform;              // Platform name (e.g., "desktop", "web", "taijios")
    const char* original_target;       // Original target string (for error messages)
    const char* resolved_target;       // Resolved target (e.g., "limbo+draw@desktop")
    bool is_alias;                     // Whether original was an alias
    bool valid;                        // Whether combination is valid
    char error[256];                   // Error message if invalid
} CodegenTarget;

/**
 * Parse a target string for CLI use
 * Handles:
 *   - Explicit combos: "limbo+draw", "tcl+tk"
 *   - Aliases: "limbo", "tcltk"
 *   - Old-style targets (with deprecation warning)
 *
 * @param target_string Target string to parse
 * @param out_target Output for parsed target info
 * @return true if valid, false otherwise
 */
bool codegen_parse_target(const char* target_string, CodegenTarget* out_target);

/**
 * Check if a target string is a valid target
 * Convenience function that doesn't require allocating CodegenTarget
 *
 * @param target_string Target string to check
 * @return true if valid, false otherwise
 */
bool codegen_is_valid_target(const char* target_string);

/**
 * Get a list of all valid targets as a formatted string
 * Caller must free the returned string
 *
 * @return Formatted string listing all valid targets, or NULL on error
 */
char* codegen_list_valid_targets(void);

/**
 * Get a list of valid targets for a specific language
 * Returns a comma-separated list of language+toolkit@platform combinations
 * Caller must free the returned string
 *
 * @param language Language name (e.g., "limbo", "c", "javascript")
 * @return Formatted string listing valid targets for this language, or NULL on error
 */
char* codegen_list_valid_targets_for_language(const char* language);

/**
 * Initialize the codegen target system
 * Must be called before using any other functions
 * Initializes language_registry, toolkit_registry, and combo_registry
 */
void codegen_target_init(void);

/**
 * Cleanup the codegen target system
 */
void codegen_target_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // CODEGEN_TARGET_H
