/**
 * Kryon Combo Registry Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "combo_registry.h"
#include "languages/common/language_registry.h"
#include "toolkits/common/toolkit_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_ALIASES 32
#define MAX_CUSTOM_COMBOS 64

// ============================================================================
// Registry State
// ============================================================================

typedef struct {
    const char* alias;
    const char* target;
} AliasMapping;

static struct {
    AliasMapping aliases[MAX_ALIASES];
    Combo custom_combos[MAX_CUSTOM_COMBOS];
    int alias_count;
    int custom_combo_count;
    bool initialized;
} g_registry = {0};

// ============================================================================
// Static Buffers
// ============================================================================

static char g_format_buffer[128];

// ============================================================================
// Built-in Aliases (Backward Compatibility)
// ============================================================================

static void init_builtin_aliases(void) {
    // Backward compatibility aliases
    combo_register_alias("tcltk", "tcl+tk");
    combo_register_alias("limbo", "limbo+draw");
    combo_register_alias("sdl3", "c+sdl3");
    combo_register_alias("raylib", "c+raylib");
}

// ============================================================================
// Validation Matrix
// ============================================================================

/**
 * Validation matrix: language (row) x toolkit (col)
 * true = valid combination, false = invalid
 */
static const bool validation_matrix[LANGUAGE_NONE + 1][TOOLKIT_NONE + 1] = {
    //                    Tk    Draw  DOM   Android  stdio SDL3  Raylib None
    /* TCL */        {  true, false, false, false,  true, false, false, false },
    /* LIMBO */      { false,  true, false, false, false, false, false, false },
    /* C */          { false, false, false, false,  true,  true,  true, false },
    /* KOTLIN */     { false, false, false,  true, false, false, false, false },
    /* JAVASCRIPT */ { false, false,  true, false, false, false, false, false },
    /* TYPESCRIPT */ { false, false,  true, false, false, false, false, false },
    /* PYTHON */     { false, false, false, false,  true,  true, false, false },
    /* RUST */       { false, false, false, false, false,  true,  true, false },
    /* NONE */       { false, false, false, false, false, false, false,  true },
};

static const char* invalid_reasons[LANGUAGE_NONE + 1][TOOLKIT_NONE + 1] = {
    //                    Tk                      Draw                      DOM                        Android              stdio      SDL3         Raylib
    /* TCL */        {  NULL, "Tcl doesn't support Draw toolkit", "Tcl doesn't support DOM", "Tcl doesn't support Android Views", NULL, "Tcl doesn't support SDL3", "Tcl doesn't support Raylib", NULL },
    /* LIMBO */      { "Limbo doesn't support Tk", NULL, "Limbo doesn't support DOM", "Limbo doesn't support Android Views", "Limbo doesn't support stdio", "Limbo doesn't support SDL3", "Limbo doesn't support Raylib", NULL },
    /* C */          { "C doesn't support Tk", "C doesn't support Draw", "C doesn't support DOM", "C doesn't support Android Views", NULL, NULL, NULL, NULL },
    /* KOTLIN */     { "Kotlin doesn't support Tk", "Kotlin doesn't support Draw", "Kotlin doesn't support DOM", NULL, "Kotlin doesn't support stdio", "Kotlin doesn't support SDL3", "Kotlin doesn't support Raylib", NULL },
    /* JAVASCRIPT */ { "JavaScript doesn't support Tk", "JavaScript doesn't support Draw", NULL, "JavaScript doesn't support Android Views", "JavaScript doesn't support stdio", "JavaScript doesn't support SDL3", "JavaScript doesn't support Raylib", NULL },
    /* TYPESCRIPT */ { "TypeScript doesn't support Tk", "TypeScript doesn't support Draw", NULL, "TypeScript doesn't support Android Views", "TypeScript doesn't support stdio", "TypeScript doesn't support SDL3", "TypeScript doesn't support Raylib", NULL },
    /* PYTHON */     { "Python doesn't support Tk", "Python doesn't support Draw", "Python doesn't support DOM", "Python doesn't support Android Views", NULL, NULL, "Python doesn't support Raylib", NULL },
    /* RUST */       { "Rust doesn't support Tk", "Rust doesn't support Draw", "Rust doesn't support DOM", "Rust doesn't support Android Views", "Rust doesn't support stdio", NULL, NULL, NULL },
    /* NONE */       { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
};

// ============================================================================
// Combo Registry API Implementation
// ============================================================================

void combo_registry_init(void) {
    if (g_registry.initialized) {
        return;  // Already initialized
    }

    init_builtin_aliases();
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

bool combo_resolve(const char* target, ComboResolution* out_resolution) {
    if (!target || !out_resolution) {
        return false;
    }

    memset(out_resolution, 0, sizeof(ComboResolution));
    out_resolution->resolved = false;

    // Check if it's an alias
    const char* expanded = combo_get_alias(target);
    if (expanded) {
        target = expanded;
    }

    // Parse "language+toolkit" format
    char* copy = strdup(target);
    if (!copy) {
        strcpy(out_resolution->error, "Memory allocation failed");
        return false;
    }

    char* plus = strchr(copy, '+');
    if (!plus) {
        snprintf(out_resolution->error, sizeof(out_resolution->error),
                 "Invalid target format '%s' (expected 'language+toolkit')", target);
        free(copy);
        return false;
    }

    *plus = '\0';
    const char* language = copy;
    const char* toolkit = plus + 1;

    // Trim whitespace
    while (*language == ' ') language++;
    while (*toolkit == ' ') toolkit++;

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
    if (!combo_is_valid(language, toolkit)) {
        const char* reason = combo_get_invalid_reason(language, toolkit);
        snprintf(out_resolution->error, sizeof(out_resolution->error),
                 "Invalid combination '%s+%s': %s", language, toolkit, reason);
        free(copy);
        return false;
    }

    // Success!
    out_resolution->combo.language = out_resolution->language_profile->name;
    out_resolution->combo.toolkit = out_resolution->toolkit_profile->name;
    out_resolution->combo.valid = true;
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
            out_combos[count].alias = combo_get_alias(out_combos[count].toolkit);
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

const char* combo_get_alias(const char* alias) {
    if (!alias) return NULL;

    for (int i = 0; i < g_registry.alias_count; i++) {
        if (strcmp(g_registry.aliases[i].alias, alias) == 0) {
            return g_registry.aliases[i].target;
        }
    }

    return NULL;
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

bool combo_register_alias(const char* alias, const char* target) {
    if (!alias || !target) {
        return false;
    }

    if (g_registry.alias_count >= MAX_ALIASES) {
        fprintf(stderr, "Error: Alias registry full\n");
        return false;
    }

    // Check for duplicate
    for (int i = 0; i < g_registry.alias_count; i++) {
        if (strcmp(g_registry.aliases[i].alias, alias) == 0) {
            fprintf(stderr, "Warning: Alias '%s' already registered\n", alias);
            return false;
        }
    }

    g_registry.aliases[g_registry.alias_count].alias = strdup(alias);
    g_registry.aliases[g_registry.alias_count].target = strdup(target);
    g_registry.alias_count++;

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
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_RUST; lang++) {
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

    // Check for alias first
    const char* expanded = combo_get_alias(target);
    if (expanded) {
        if (print_errors) {
            fprintf(stderr, "Note: '%s' is an alias for '%s'\n", target, expanded);
        }
    }

    // Resolve the combo
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
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_RUST; lang++) {
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

    // Print aliases if requested
    if (include_aliases && g_registry.alias_count > 0) {
        printf("Aliases (backward compatibility):\n");
        for (int i = 0; i < g_registry.alias_count; i++) {
            printf("  %-20s → %s\n",
                   g_registry.aliases[i].alias,
                   g_registry.aliases[i].target);
        }
        printf("\n");
    }

    printf("Usage:\n");
    printf("  kryon run --target=<language>+<toolkit> file.kry\n");
    printf("  kryon run --target=<alias> file.kry\n");
    printf("\n");
    printf("Examples:\n");
    printf("  kryon run --target=limbo+draw main.kry\n");
    printf("  kryon run --target=tcl+tk main.kry\n");
    printf("  kryon run --target=limbo main.kry         # Alias for limbo+draw\n");
    printf("  kryon run --target=tcltk main.kry         # Alias for tcl+tk\n");
    printf("\n");
}
