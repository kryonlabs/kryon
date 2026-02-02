/**
 * Kryon Combo Registry Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "combo_registry.h"
#include "languages/common/language_registry.h"
#include "toolkits/common/toolkit_registry.h"
#include "platforms/common/platform_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_CUSTOM_COMBOS 64

// ============================================================================
// Registry State
// ============================================================================

static struct {
    Combo custom_combos[MAX_CUSTOM_COMBOS];
    int custom_combo_count;
    bool initialized;
} g_registry = {0};

// ============================================================================
// Static Buffers
// ============================================================================

static char g_format_buffer[128];

// ============================================================================
// Validation Matrix
// ============================================================================

/**
 * Validation matrix: language (row) x toolkit (col)
 * true = valid combination, false = invalid
 */
static const bool validation_matrix[LANGUAGE_NONE + 1][TOOLKIT_NONE + 1] = {
    //                    Tk    Draw  DOM   AndroidViews  terminal SDL3  Raylib None
    /* TCL */        {  true, false, false, false,      true, false, false, false },
    /* LIMBO */      {  true, false, false, false,      false, false, false, false },
    /* C */          {  true, false, false, false,      true,  true,  true, false },
    /* KOTLIN */     { false, false, false, true,       false, false, false, false },
    /* JAVASCRIPT */ { false, false,  true, false,      false, false, false, false },
    /* LUA */        { false, false, false, false,      true,  true, false, false },
    /* NONE */       { false, false, false, false,      false, false, false,  true },
};

static const char* invalid_reasons[LANGUAGE_NONE + 1][TOOLKIT_NONE + 1] = {
    //                    Tk                      Draw                  DOM                        Android              terminal      SDL3         Raylib
    /* TCL */        {  NULL, "Tcl doesn't support Draw toolkit", "Tcl doesn't support DOM", "Tcl doesn't support Android Views", NULL, "Tcl doesn't support SDL3", "Tcl doesn't support Raylib", NULL },
    /* LIMBO */      { NULL, "Limbo doesn't support Draw", "Limbo doesn't support DOM", "Limbo doesn't support Android Views", NULL, "Limbo doesn't support SDL3", "Limbo doesn't support Raylib", NULL },
    /* C */          { NULL, "C doesn't support Draw", "C doesn't support DOM", "C doesn't support Android Views", NULL, NULL, NULL, NULL },
    /* KOTLIN */     { "Kotlin doesn't support Tk", "Kotlin doesn't support Draw", "Kotlin doesn't support DOM", NULL, "Kotlin doesn't support terminal", "Kotlin doesn't support SDL3", "Kotlin doesn't support Raylib", NULL },
    /* JAVASCRIPT */ { "JavaScript doesn't support Tk", "JavaScript doesn't support Draw", NULL, "JavaScript doesn't support Android Views", "JavaScript doesn't support terminal", "JavaScript doesn't support SDL3", "JavaScript doesn't support Raylib", NULL },
    /* LUA */        { "Lua doesn't support Tk", "Lua doesn't support Draw", "Lua doesn't support DOM", "Lua doesn't support Android Views", NULL, NULL, "Lua doesn't support Raylib", NULL },
    /* NONE */       { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
};

// ============================================================================
// Combo Registry API Implementation
// ============================================================================

void combo_registry_init(void) {
    if (g_registry.initialized) {
        return;  // Already initialized
    }

    g_registry.initialized = true;
}

void combo_registry_cleanup(void) {
    memset(&g_registry, 0, sizeof(g_registry));
}

bool combo_is_valid(const char* language, const char* toolkit) {
    if (!language || !toolkit) {
        return false;
    }

    LanguageType lang_type = language_type_from_string(language);
    ToolkitType tk_type = toolkit_type_from_string(toolkit);

    return combo_is_valid_typed(lang_type, tk_type);
}

bool combo_is_valid_typed(LanguageType language, ToolkitType toolkit) {
    if (!language_type_is_valid(language) || !toolkit_type_is_valid(toolkit)) {
        return false;
    }

    if (language > LANGUAGE_NONE || toolkit > TOOLKIT_NONE) {
        return false;
    }

    return validation_matrix[language][toolkit];
}

const char* combo_get_invalid_reason(const char* language, const char* toolkit) {
    if (!language || !toolkit) {
        return "Invalid language or toolkit";
    }

    LanguageType lang_type = language_type_from_string(language);
    ToolkitType tk_type = toolkit_type_from_string(toolkit);

    if (!language_type_is_valid(lang_type)) {
        return "Unknown language";
    }

    if (!toolkit_type_is_valid(tk_type)) {
        return "Unknown toolkit";
    }

    if (lang_type > LANGUAGE_NONE || tk_type > TOOLKIT_NONE) {
        return "Invalid combination";
    }

    return invalid_reasons[lang_type][tk_type];
}

// Alias table removed - users must specify full language+toolkit[@platform] format
// No shorthand aliases allowed anymore

bool combo_is_valid_with_platform(const char* language, const char* toolkit, const char* platform) {
    if (!language || !toolkit || !platform) {
        return false;
    }

    // Validate platform exists
    if (!platform_is_registered(platform)) {
        return false;
    }

    // Validate platform supports the toolkit
    if (!platform_supports_toolkit(platform, toolkit)) {
        return false;
    }

    // Validate platform supports the language
    if (!platform_supports_language(platform, language)) {
        return false;
    }

    // Validate language+toolkit combination is technically valid
    return combo_is_valid(language, toolkit);
}

const char* combo_get_invalid_reason_with_platform(const char* language, const char* toolkit, const char* platform) {
    if (!language || !toolkit || !platform) {
        return "Invalid language, toolkit, or platform";
    }

    // Validate platform exists
    if (!platform_is_registered(platform)) {
        static char unknown_platform_error[256];
        snprintf(unknown_platform_error, sizeof(unknown_platform_error),
                "Unknown platform '%s'", platform);
        return unknown_platform_error;
    }

    // Validate toolkit exists
    ToolkitType tk_type = toolkit_type_from_string(toolkit);
    if (!toolkit_type_is_valid(tk_type)) {
        static char unknown_toolkit_error[256];
        snprintf(unknown_toolkit_error, sizeof(unknown_toolkit_error),
                "Unknown toolkit '%s'", toolkit);
        return unknown_toolkit_error;
    }

    // Validate language exists
    LanguageType lang_type = language_type_from_string(language);
    if (!language_type_is_valid(lang_type)) {
        static char unknown_language_error[256];
        snprintf(unknown_language_error, sizeof(unknown_language_error),
                "Unknown language '%s'", language);
        return unknown_language_error;
    }

    // Validate platform supports the toolkit
    if (!platform_supports_toolkit(platform, toolkit)) {
        static char unsupported_toolkit_error[512];
        snprintf(unsupported_toolkit_error, sizeof(unsupported_toolkit_error),
                "Platform '%s' does not support toolkit '%s'", platform, toolkit);
        return unsupported_toolkit_error;
    }

    // Validate platform supports the language
    if (!platform_supports_language(platform, language)) {
        static char unsupported_language_error[512];
        snprintf(unsupported_language_error, sizeof(unsupported_language_error),
                "Platform '%s' does not support language '%s'", platform, language);
        return unsupported_language_error;
    }

    // Validate language+toolkit combination
    const char* reason = combo_get_invalid_reason(language, toolkit);
    if (reason) {
        return reason;
    }

    return NULL;  // Valid combination
}

bool combo_resolve(const char* target, ComboResolution* out_resolution) {
    if (!target || !out_resolution) {
        return false;
    }

    memset(out_resolution, 0, sizeof(ComboResolution));
    out_resolution->resolved = false;

    // Parse "language+toolkit@platform" format
    // No alias expansion - user must specify full format
    char* copy = strdup(target);
    if (!copy) {
        strcpy(out_resolution->error, "Memory allocation failed");
        return false;
    }

    // Check for @platform suffix (optional)
    char* at = strchr(copy, '@');
    const char* platform;
    if (at) {
        *at = '\0';
        platform = at + 1;
    } else {
        platform = NULL;
    }

    // Parse "language+toolkit" part
    char* plus = strchr(copy, '+');
    if (!plus) {
        snprintf(out_resolution->error, sizeof(out_resolution->error),
                 "Invalid target format '%s' (expected 'language+toolkit@platform')", target);
        free(copy);
        return false;
    }

    *plus = '\0';
    const char* language = copy;
    const char* toolkit = plus + 1;

    // Trim whitespace
    while (*language == ' ') language++;
    while (*toolkit == ' ') toolkit++;
    if (platform) {
        while (*platform == ' ') platform++;
    }

    // If no platform specified, try to auto-resolve
    if (!platform) {
        platform = combo_resolve_platform_for_combo(language, toolkit);

        if (!platform) {
            // Could not auto-resolve - provide helpful error
            size_t valid_platform_count = 0;
            const char* valid_platforms[16];

            // Collect all valid platforms for this combo
            const PlatformProfile* platforms[16];
            size_t platform_count = platform_get_all_registered(platforms, 16);

            for (size_t i = 0; i < platform_count; i++) {
                if (platform_supports_language(platforms[i]->name, language) &&
                    platform_supports_toolkit(platforms[i]->name, toolkit)) {
                    valid_platforms[valid_platform_count++] = platforms[i]->name;
                }
            }

            if (valid_platform_count == 0) {
                snprintf(out_resolution->error, sizeof(out_resolution->error),
                         "No platform supports '%s+%s'. "
                         "This combination is not available on any platform.",
                         language, toolkit);
                free(copy);
                return false;
            } else {
                // Build error message listing valid platforms
                char platform_list[256] = "";
                for (size_t i = 0; i < valid_platform_count && i < 3; i++) {
                    if (i > 0) strcat(platform_list, ", ");
                    strcat(platform_list, valid_platforms[i]);
                }
                if (valid_platform_count > 3) {
                    strcat(platform_list, ", ...");
                }

                snprintf(out_resolution->error, sizeof(out_resolution->error),
                         "Multiple platforms support '%s+%s' (%s). "
                         "Please specify explicitly using: %s+%s@<platform>\n"
                         "Valid platforms: %s",
                         language, toolkit, platform_list, language, toolkit, platform_list);
                free(copy);
                return false;
            }
        }
    }

    // Resolve profiles
    out_resolution->language_profile = language_get_profile(language);
    out_resolution->toolkit_profile = toolkit_get_profile(toolkit);

    if (!out_resolution->language_profile) {
        snprintf(out_resolution->error, sizeof(out_resolution->error),
                 "Unknown language '%s'", language);
        free(copy);
        return false;
    }

    if (!out_resolution->toolkit_profile) {
        snprintf(out_resolution->error, sizeof(out_resolution->error),
                 "Unknown toolkit '%s'", toolkit);
        free(copy);
        return false;
    }

    // Validate combination
    const char* reason = NULL;

    if (platform) {
        // Platform specified - validate with platform constraints
        reason = combo_get_invalid_reason_with_platform(language, toolkit, platform);
        if (reason) {
            snprintf(out_resolution->error, sizeof(out_resolution->error),
                     "Invalid combination '%s+%s@%s': %s", language, toolkit, platform, reason);
            free(copy);
            return false;
        }
    } else {
        // No platform specified - just validate language+toolkit combination
        reason = combo_get_invalid_reason(language, toolkit);
        if (reason) {
            snprintf(out_resolution->error, sizeof(out_resolution->error),
                     "Invalid combination '%s+%s': %s", language, toolkit, reason);
            free(copy);
            return false;
        }
    }

    // Success!
    out_resolution->combo.language = out_resolution->language_profile->name;
    out_resolution->combo.toolkit = out_resolution->toolkit_profile->name;
    out_resolution->combo.valid = true;
    out_resolution->platform = platform;  // Store the resolved platform
    out_resolution->resolved = true;

    free(copy);
    return true;
}

size_t combo_get_for_language(const char* language, Combo* out_combos, size_t max_count) {
    if (!language || !out_combos || max_count == 0) {
        return 0;
    }

    LanguageType lang_type = language_type_from_string(language);
    if (!language_type_is_valid(lang_type)) {
        return 0;
    }

    size_t count = 0;
    for (ToolkitType tk = TOOLKIT_TK; tk <= TOOLKIT_NONE; tk++) {
        if (combo_is_valid_typed(lang_type, tk) && count < max_count) {
            out_combos[count].language = language;
            out_combos[count].toolkit = toolkit_type_to_string(tk);
            out_combos[count].valid = true;
            out_combos[count].alias = NULL;  // No aliases
            count++;
        }
    }

    return count;
}

size_t combo_get_for_toolkit(const char* toolkit, Combo* out_combos, size_t max_count) {
    if (!toolkit || !out_combos || max_count == 0) {
        return 0;
    }

    ToolkitType tk_type = toolkit_type_from_string(toolkit);
    if (!toolkit_type_is_valid(tk_type)) {
        return 0;
    }

    size_t count = 0;
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_NONE; lang++) {
        if (combo_is_valid_typed(lang, tk_type) && count < max_count) {
            out_combos[count].language = language_type_to_string(lang);
            out_combos[count].toolkit = toolkit;
            out_combos[count].valid = true;
            out_combos[count].alias = NULL;
            count++;
        }
    }

    return count;
}

size_t combo_get_all(Combo* out_combos, size_t max_count) {
    if (!out_combos || max_count == 0) {
        return 0;
    }

    size_t count = 0;
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_NONE; lang++) {
        for (ToolkitType tk = TOOLKIT_TK; tk <= TOOLKIT_NONE; tk++) {
            if (combo_is_valid_typed(lang, tk) && count < max_count) {
                out_combos[count].language = language_type_to_string(lang);
                out_combos[count].toolkit = toolkit_type_to_string(tk);
                out_combos[count].valid = true;
                out_combos[count].alias = NULL;
                count++;
            }
        }
    }

    return count;
}

bool combo_register(const Combo* combo) {
    if (!combo || !combo->language || !combo->toolkit) {
        return false;
    }

    if (g_registry.custom_combo_count >= MAX_CUSTOM_COMBOS) {
        fprintf(stderr, "Error: Custom combo registry full\n");
        return false;
    }

    g_registry.custom_combos[g_registry.custom_combo_count++] = *combo;
    return true;
}

const char* combo_format(const Combo* combo) {
    if (!combo || !combo->language || !combo->toolkit) {
        return NULL;
    }

    return combo_format_parts(combo->language, combo->toolkit);
}

const char* combo_format_parts(const char* language, const char* toolkit) {
    if (!language || !toolkit) {
        return NULL;
    }

    snprintf(g_format_buffer, sizeof(g_format_buffer), "%s+%s", language, toolkit);
    return g_format_buffer;
}

bool combo_auto_resolve_language(const char* language, const char** out_toolkit, const char** out_platform) {
    if (!language || !out_toolkit || !out_platform) {
        return false;
    }

    // Validate language exists
    LanguageType lang_type = language_type_from_string(language);
    if (!language_type_is_valid(lang_type)) {
        return false;
    }

    // Static storage for results
    static const char* s_toolkit = NULL;
    static const char* s_platform = NULL;
    s_toolkit = NULL;
    s_platform = NULL;

    // Get all platforms
    const PlatformProfile* platforms[16];
    size_t platform_count = platform_get_all_registered(platforms, 16);

    int valid_count = 0;
    const char* found_toolkit = NULL;
    const char* found_platform = NULL;

    // For each platform, check if it supports this language
    for (size_t p = 0; p < platform_count; p++) {
        const PlatformProfile* plat = platforms[p];

        // Check if platform supports this language
        if (!platform_supports_language(plat->name, language)) {
            continue;
        }

        // Check each toolkit supported by this platform
        for (size_t t = 0; t < plat->toolkit_count; t++) {
            const char* toolkit = plat->supported_toolkits[t];

            // Check if language+toolkit combo is technically valid
            if (combo_is_valid(language, toolkit)) {
                found_toolkit = toolkit;
                found_platform = plat->name;
                valid_count++;

                // Early exit if more than one found
                if (valid_count > 1) {
                    return false;
                }
            }
        }
    }

    // Return results only if exactly one valid combination
    if (valid_count == 1) {
        s_toolkit = found_toolkit;
        s_platform = found_platform;
        *out_toolkit = s_toolkit;
        *out_platform = s_platform;
        return true;
    }

    return false;
}

const char* combo_resolve_platform_for_combo(const char* language, const char* toolkit) {
    if (!language || !toolkit) {
        return NULL;
    }

    // Get all platforms
    const PlatformProfile* platforms[16];
    size_t platform_count = platform_get_all_registered(platforms, 16);

    const char* matching_platform = NULL;
    int match_count = 0;

    // For each platform, check if it supports both language and toolkit
    for (size_t i = 0; i < platform_count; i++) {
        const PlatformProfile* platform = platforms[i];

        // Check if platform supports both language and toolkit
        if (platform_supports_language(platform->name, language) &&
            platform_supports_toolkit(platform->name, toolkit)) {
            matching_platform = platform->name;
            match_count++;
        }
    }

    // Return platform only if exactly one match
    return (match_count == 1) ? matching_platform : NULL;
}

void combo_print_matrix(void) {
    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                         Kryon Language+Toolkit Matrix\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    // Print header row
    printf("%-15s", "");
    for (ToolkitType tk = TOOLKIT_TK; tk <= TOOLKIT_RAYLIB; tk++) {
        printf("%-10s", toolkit_type_to_string(tk));
    }
    printf("\n");

    // Print separator
    printf("%-15s", "");
    for (ToolkitType tk = TOOLKIT_TK; tk <= TOOLKIT_RAYLIB; tk++) {
        printf("%-10s", "────────");
    }
    printf("\n");

    // Print each language row
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_LUA; lang++) {
        printf("%-15s", language_type_to_string(lang));
        for (ToolkitType tk = TOOLKIT_TK; tk <= TOOLKIT_RAYLIB; tk++) {
            if (combo_is_valid_typed(lang, tk)) {
                printf("%-10s", "✓");
            } else {
                printf("%-10s", "✗");
            }
        }
        printf("\n");
    }

    printf("\n");
}

// ============================================================================
// CLI Helper Functions
// ============================================================================

bool combo_resolve_for_cli(const char* target, ComboResolution* out_resolution, bool print_errors) {
    if (!target || !out_resolution) {
        if (print_errors) {
            fprintf(stderr, "Error: Invalid target or resolution output\n");
        }
        return false;
    }

    // Resolve the combo (no alias expansion)
    bool success = combo_resolve(target, out_resolution);

    if (!success && print_errors) {
        fprintf(stderr, "Error: %s\n", out_resolution->error);
        fprintf(stderr, "\nValid targets:\n");
        combo_print_valid_targets(false);
    }

    return success;
}

void combo_print_valid_targets(bool include_aliases) {
    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                          Valid Language+Toolkit Targets\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    // Get all valid combos
    Combo combos[64];
    size_t count = combo_get_all(combos, 64);

    // Group by language
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_LUA; lang++) {
        const char* lang_name = language_type_to_string(lang);
        bool has_combos = false;

        // Check if language has any valid combos
        for (size_t i = 0; i < count; i++) {
            if (strcmp(combos[i].language, lang_name) == 0) {
                if (!has_combos) {
                    printf("%-12s:", lang_name);
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

    printf("Usage:\n");
    printf("  kryon run --target=<language>+<toolkit>@<platform> file.kry\n");
    printf("\n");
    printf("Examples:\n");
    printf("  kryon run --target=limbo+tk@taiji main.kry\n");
    printf("  kryon run --target=tcl+tk@desktop main.kry\n");
    printf("\n");
}
