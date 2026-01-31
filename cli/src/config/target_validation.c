/**
 * Target Validation for CLI
 *
 * Integrates the new combo registry with the existing target handler system.
 * Provides validation and alias expansion while maintaining backward compatibility.
 *
 * Architecture:
 * - Old targets (limbo, tcltk, etc.) continue to work via aliases
 * - New explicit combos (limbo+draw, tcl+tk) are validated
 * - Mapping between combos and runtime handlers
 */

#define _POSIX_C_SOURCE 200809L

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include codegen combo registry
#include "../../../codegens/codegen_target.h"
#include "../../../codegens/combo_registry.h"
#include "../../../codegens/languages/common/language_registry.h"
#include "../../../codegens/toolkits/common/toolkit_registry.h"

// Static initialization flag
static bool g_validation_initialized = false;

/**
 * Initialize target validation system
 */
void target_validation_initialize(void) {
    if (g_validation_initialized) {
        return;
    }

    // Initialize the codegen target system
    codegen_target_init();

    g_validation_initialized = true;
}

/**
 * Validate and normalize a target string
 * Handles aliases (limbo → limbo+draw) and validates explicit combos (limbo+draw)
 *
 * @param target Input target string (e.g., "limbo", "limbo+draw")
 * @param normalized_output Output buffer for normalized target
 * @param output_size Size of output buffer
 * @return true if valid, false otherwise
 */
bool target_validate_and_normalize(const char* target,
                                   char* normalized_output,
                                   size_t output_size) {
    if (!target || !normalized_output || output_size == 0) {
        return false;
    }

    // Initialize if needed
    if (!g_validation_initialized) {
        target_validation_initialize();
    }

    // Parse the target
    CodegenTarget parsed;
    if (!codegen_parse_target(target, &parsed)) {
        // Invalid target
        fprintf(stderr, "Error: %s\n", parsed.error);
        return false;
    }

    // Valid target - return normalized form
    strncpy(normalized_output, parsed.resolved_target, output_size - 1);
    normalized_output[output_size - 1] = '\0';

    // Print deprecation notice for aliases
    if (parsed.is_alias) {
        fprintf(stderr, "Note: '%s' is an alias for '%s'\n", target, parsed.resolved_target);
        fprintf(stderr, "      Consider using the explicit form in the future.\n\n");
    }

    return true;
}

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
                                   size_t output_size) {
    if (!validated_target || !runtime_handler || output_size == 0) {
        return false;
    }

    // Parse the validated target
    CodegenTarget parsed;
    if (!codegen_parse_target(validated_target, &parsed)) {
        return false;
    }

    // Map language+toolkit to runtime handler
    const char* handler = NULL;

    if (strcmp(parsed.language, "limbo") == 0) {
        handler = "limbo";  // Limbo VM handler
    } else if (strcmp(parsed.language, "tcl") == 0) {
        handler = "tcltk";  // Tcl/Tk wish handler
    } else if (strcmp(parsed.language, "c") == 0) {
        if (strcmp(parsed.toolkit, "sdl3") == 0) {
            handler = "sdl3";
        } else if (strcmp(parsed.toolkit, "raylib") == 0) {
            handler = "raylib";
        } else {
            handler = "desktop";  // Default C desktop
        }
    } else if (strcmp(parsed.language, "kotlin") == 0) {
        handler = "android";  // Android handler
    } else if (strcmp(parsed.language, "javascript") == 0 ||
               strcmp(parsed.language, "typescript") == 0) {
        handler = "web";  // Web handler
    } else {
        return false;  // Unknown mapping
    }

    strncpy(runtime_handler, handler, output_size - 1);
    runtime_handler[output_size - 1] = '\0';
    return true;
}

/**
 * Check if a target is a combo (contains '+') or an alias
 */
bool target_is_combo(const char* target) {
    if (!target) return false;
    return strchr(target, '+') != NULL;
}

/**
 * Print all valid targets (for 'kryon targets' command)
 */
void target_print_all_valid(void) {
    // Initialize if needed
    if (!g_validation_initialized) {
        target_validation_initialize();
    }

    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                          Valid Kryon Targets\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    printf("Language+Toolkit combinations (explicit syntax):\n\n");

    // Get all valid combos
    Combo combos[64];
    size_t count = combo_get_all(combos, 64);

    // Group by language
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_RUST; lang++) {
        const char* lang_name = language_type_to_string(lang);
        bool has_combos = false;

        // Check if language has any valid combos
        for (size_t i = 0; i < count; i++) {
            if (strcmp(combos[i].language, lang_name) == 0) {
                if (!has_combos) {
                    printf("  %-12s:", lang_name);
                    has_combos = true;
                }
                printf(" %s", combos[i].toolkit);
            }
        }

        if (has_combos) {
            printf("\n");
        }
    }

    printf("\n");
    printf("Aliases (backward compatibility):\n");
    printf("  tcltk        → tcl+tk\n");
    printf("  limbo        → limbo+draw\n");
    printf("  sdl3         → c+sdl3\n");
    printf("  raylib       → c+raylib\n");
    printf("\n");

    printf("Usage:\n");
    printf("  kryon run --target=<language>+<toolkit> file.kry\n");
    printf("  kryon run --target=<alias> file.kry\n");
    printf("\n");
}

/**
 * Cleanup target validation system
 */
void target_validation_cleanup(void) {
    if (g_validation_initialized) {
        codegen_target_cleanup();
        g_validation_initialized = false;
    }
}
