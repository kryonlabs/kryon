/**
 * Kryon Codegen Target Helper Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "codegen_target.h"
#include "combo_registry.h"
#include "languages/common/language_registry.h"
#include "toolkits/common/toolkit_registry.h"
#include "platforms/common/platform_registry.h"
#include "common/wir_composer.h"
#include "tcl/tcl_wir_emitter.h"
#include "toolkits/tk/tk_wir_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Initialization
// ============================================================================

void codegen_target_init(void) {
    language_registry_init();
    toolkit_registry_init();
    platform_registry_init();
    combo_registry_init();

    // Initialize WIR composer
    wir_composer_init();

    // Register language emitters
    tcl_wir_emitter_init();

    // Register toolkit emitters
    tk_wir_emitter_init();
}

void codegen_target_cleanup(void) {
    // Cleanup WIR emitters
    tk_wir_emitter_cleanup();
    tcl_wir_emitter_cleanup();
    wir_composer_cleanup();

    combo_registry_cleanup();
    platform_registry_cleanup();
    toolkit_registry_cleanup();
    language_registry_cleanup();
}

// ============================================================================
// Target Parsing
// ============================================================================

bool codegen_parse_target(const char* target_string, CodegenTarget* out_target) {
    if (!target_string || !out_target) {
        return false;
    }

    memset(out_target, 0, sizeof(CodegenTarget));
    out_target->original_target = target_string;
    out_target->valid = false;

    // Check if target contains '+' or '@'
    bool has_plus = strchr(target_string, '+') != NULL;
    bool has_at = strchr(target_string, '@') != NULL;

    // If it's just a language name (no +, no @), try auto-resolution
    if (!has_plus && !has_at) {
        const char* auto_toolkit = NULL;
        const char* auto_platform = NULL;

        if (combo_auto_resolve_language(target_string, &auto_toolkit, &auto_platform)) {
            // Exactly one valid combination - use it!
            char auto_target[256];
            snprintf(auto_target, sizeof(auto_target), "%s+%s@%s",
                     target_string, auto_toolkit, auto_platform);

            // Parse the auto-resolved target
            bool result = codegen_parse_target(auto_target, out_target);
            if (result) {
                // Note: we auto-resolved (store in error field for info)
                snprintf(out_target->error, sizeof(out_target->error),
                        "Auto-resolved to: %s", auto_target);
            }
            return result;
        }

        // Language exists but has multiple/no options - give helpful error
        const LanguageProfile* lang = language_get_profile(target_string);
        if (lang) {
            // Get actual valid combinations for this language
            char* valid_targets = codegen_list_valid_targets_for_language(target_string);
            if (valid_targets) {
                snprintf(out_target->error, sizeof(out_target->error),
                        "Language '%s' has multiple valid combinations:\n"
                        "  %s\n"
                        "Please specify explicitly, e.g., kryon run --target=%s main.kry",
                        target_string, valid_targets, target_string);
                free(valid_targets);
            } else {
                snprintf(out_target->error, sizeof(out_target->error),
                        "Language '%s' has multiple valid combinations.\n"
                        "Please specify explicitly, e.g., kryon run --target=%s+<toolkit>@<platform> main.kry",
                        target_string, target_string);
            }
            return false;
        }
    }

    // Resolve the combo (requires language+toolkit@platform format)
    ComboResolution resolution;
    if (!combo_resolve(target_string, &resolution)) {
        snprintf(out_target->error, sizeof(out_target->error),
                "Invalid target '%s': %s",
                target_string, resolution.error);
        return false;
    }

    // Fill in the target info (no alias checking - aliases removed)
    out_target->language = resolution.language_profile->name;
    out_target->toolkit = resolution.toolkit_profile->name;
    out_target->platform = resolution.platform;

    // Format resolved target as language+toolkit@platform
    static char resolved_buffer[256];
    snprintf(resolved_buffer, sizeof(resolved_buffer), "%s+%s@%s",
             out_target->language, out_target->toolkit,
             out_target->platform ? out_target->platform : "unknown");
    out_target->resolved_target = resolved_buffer;

    out_target->valid = true;

    return true;
}

bool codegen_is_valid_target(const char* target_string) {
    if (!target_string) return false;

    ComboResolution resolution;
    return combo_resolve(target_string, &resolution);
}

char* codegen_list_valid_targets_for_language(const char* language) {
    if (!language) {
        return NULL;
    }

    // Validate language exists
    const LanguageProfile* lang = language_get_profile(language);
    if (!lang) {
        return NULL;
    }

    // Calculate buffer size needed
    size_t buffer_size = 2048;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;

    size_t offset = 0;
    bool first = true;

    // Get all platforms
    const PlatformProfile* platforms[16];
    size_t platform_count = platform_get_all_registered(platforms, 16);

    // For each platform, check if it supports this language with any toolkit
    for (size_t p = 0; p < platform_count; p++) {
        const PlatformProfile* platform = platforms[p];

        // Check if platform supports this language
        if (!platform_supports_language(platform->name, language)) {
            continue;
        }

        // Check each toolkit supported by this platform
        for (size_t t = 0; t < platform->toolkit_count; t++) {
            const char* toolkit = platform->supported_toolkits[t];

            // Check if language+toolkit combo is technically valid
            if (combo_is_valid(language, toolkit)) {
                // Add separator if not first
                if (!first) {
                    offset += snprintf(buffer + offset, buffer_size - offset, ", ");
                }
                first = false;

                // Add this combination: language+toolkit@platform
                offset += snprintf(buffer + offset, buffer_size - offset,
                                 "%s+%s@%s", language, toolkit, platform->name);
            }
        }
    }

    // If no valid combinations found, return empty string
    if (first) {
        strcpy(buffer, "No valid combinations found");
    }

    return buffer;
}

char* codegen_list_valid_targets(void) {
    // Get all valid combos
    Combo combos[64];
    size_t count = combo_get_all(combos, 64);

    // Calculate buffer size
    size_t buffer_size = 4096;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;

    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset,
                     "Valid language+toolkit@platform targets:\n\n");

    // Get all platforms
    const PlatformProfile* platforms[16];
    size_t platform_count = platform_get_all_registered(platforms, 16);

    // Group by platform, then language
    for (size_t p = 0; p < platform_count; p++) {
        const PlatformProfile* platform = platforms[p];
        offset += snprintf(buffer + offset, buffer_size - offset,
                         "%s:\n", platform->name);

        // For each language supported by this platform
        for (size_t l = 0; l < platform->language_count; l++) {
            const char* lang = platform->supported_languages[l];
            offset += snprintf(buffer + offset, buffer_size - offset,
                             "  %-12s:", lang);

            // List all toolkits for this language on this platform
            for (size_t t = 0; t < platform->toolkit_count; t++) {
                const char* toolkit = platform->supported_toolkits[t];
                // Check if this language+toolkit combo is technically valid
                if (combo_is_valid(lang, toolkit)) {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                     " %s", toolkit);
                }
            }
            offset += snprintf(buffer + offset, buffer_size - offset, "\n");
        }
        offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    }

    offset += snprintf(buffer + offset, buffer_size - offset,
                     "Usage: kryon run --target=<language>+<toolkit>@<platform> file.kry\n");

    return buffer;
}
