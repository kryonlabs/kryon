/**
 * Target Validation for CLI
 *
 * Provides integration between the combo registry and target handler system.
 */

#ifndef TARGET_VALIDATION_H
#define TARGET_VALIDATION_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize target validation system
 * Must be called before using any other functions
 */
void target_validation_initialize(void);

/**
 * Validate and normalize a target string
 * Handles aliases and validates explicit combos
 *
 * @param target Input target string (e.g., "limbo", "limbo+draw")
 * @param normalized_output Output buffer for normalized target
 * @param output_size Size of output buffer
 * @return true if valid, false otherwise
 */
bool target_validate_and_normalize(const char* target,
                                   char* normalized_output,
                                   size_t output_size);

/**
 * Map a validated combo to a runtime handler
 * For example: "limbo+draw" → "limbo" (runtime handler)
 *
 * @param validated_target Validated combo (e.g., "limbo+draw")
 * @param runtime_handler Output buffer for runtime handler name
 * @param output_size Size of output buffer
 * @return true if mapping found, false otherwise
 */
bool target_map_to_runtime_handler(const char* validated_target,
                                   char* runtime_handler,
                                   size_t output_size);

/**
 * Check if a target is a combo (contains '+') or an alias
 *
 * @param target Target string to check
 * @return true if combo, false otherwise
 */
bool target_is_combo(const char* target);

/**
 * Print all valid targets (for 'kryon targets' command)
 */
void target_print_all_valid(void);

/**
 * Cleanup target validation system
 */
void target_validation_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // TARGET_VALIDATION_H
