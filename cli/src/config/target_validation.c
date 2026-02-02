/**
 * Target Validation for CLI
 *
 * Integrates the new combo registry with the existing target handler system.
 * Provides validation and alias expansion while maintaining backward compatibility.
 *
 * Architecture:
 * - Targets use language+toolkit@platform format (all three required)
 * - Platform validation ensures toolkit/language compatibility
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
#include "../../../codegens/platforms/common/platform_registry.h"

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
 * Requires language+toolkit@platform format (all three components required)
 *
 * @param target Input target string (e.g., "limbo+draw@taijios")
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

    // Parse the target (now requires language+toolkit@platform)
    CodegenTarget parsed;
    if (!codegen_parse_target(target, &parsed)) {
        // Invalid target
        fprintf(stderr, "Error: %s\n", parsed.error);

        // If the error is about multiple combinations, we already showed them
        // Otherwise show general help
        if (!strstr(parsed.error, "has multiple valid combinations:")) {
            fprintf(stderr, "\nValid targets use the format: language+toolkit@platform\n");
            fprintf(stderr, "\nRun 'kryon targets' to list all valid combinations.\n");
        }
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

    printf("Language+Toolkit@Platform combinations:\n\n");

    // Get all platforms
    const PlatformProfile* platforms[16];
    size_t platform_count = platform_get_all_registered(platforms, 16);

    // Group by platform, then language
    for (size_t p = 0; p < platform_count; p++) {
        const PlatformProfile* platform = platforms[p];
        printf("%s:\n", platform->name);

        // For each language supported by this platform
        for (size_t l = 0; l < platform->language_count; l++) {
            const char* lang = platform->supported_languages[l];
            printf("  %-12s:", lang);

            // List all toolkits for this language on this platform
            for (size_t t = 0; t < platform->toolkit_count; t++) {
                const char* toolkit = platform->supported_toolkits[t];
                // Check if this language+toolkit combo is technically valid
                if (combo_is_valid(lang, toolkit)) {
                    printf(" %s", toolkit);
                }
            }
            printf("\n");
        }
        printf("\n");
    }

    printf("Usage:\n");
    printf("  kryon run --target=<language>+<toolkit>@<platform> file.kry\n");
    printf("\n");

    printf("Examples:\n");
    printf("  kryon run --target=limbo+tk@taiji main.kry\n");
    printf("  kryon run --target=c+sdl3@desktop main.kry\n");
    printf("  kryon run --target=javascript+dom@web main.kry\n");
    printf("\n");

    printf("Platform aliases:\n");
    printf("  taiji, inferno → taijios\n");
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
