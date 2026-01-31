/**
 * Kryon Codegen Target Helper Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "codegen_target.h"
#include "combo_registry.h"
#include "languages/common/language_registry.h"
#include "toolkits/common/toolkit_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Initialization
// ============================================================================

void codegen_target_init(void) {
    language_registry_init();
    toolkit_registry_init();
    combo_registry_init();
}

void codegen_target_cleanup(void) {
    combo_registry_cleanup();
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

    // Resolve the combo
    ComboResolution resolution;
    if (!combo_resolve(target_string, &resolution)) {
        snprintf(out_target->error, sizeof(out_target->error),
                "Invalid target '%s': %s",
                target_string, resolution.error);
        return false;
    }

    // Check if it was an alias
    const char* alias_expansion = combo_get_alias(target_string);
    out_target->is_alias = (alias_expansion != NULL);

    // Fill in the target info
    out_target->language = resolution.language_profile->name;
    out_target->toolkit = resolution.toolkit_profile->name;
    out_target->resolved_target = combo_format(&resolution.combo);
    out_target->valid = true;

    return true;
}

bool codegen_is_valid_target(const char* target_string) {
    if (!target_string) return false;

    ComboResolution resolution;
    return combo_resolve(target_string, &resolution);
}

char* codegen_list_valid_targets(void) {
    // Get all valid combos
    Combo combos[64];
    size_t count = combo_get_all(combos, 64);

    // Calculate buffer size
    size_t buffer_size = 1024;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;

    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset,
                     "Valid language+toolkit targets:\n\n");

    // Group by language
    for (LanguageType lang = LANGUAGE_TCL; lang <= LANGUAGE_RUST; lang++) {
        const char* lang_name = language_type_to_string(lang);
        bool has_combos = false;

        // Check if language has any valid combos
        for (size_t i = 0; i < count; i++) {
            if (strcmp(combos[i].language, lang_name) == 0) {
                if (!has_combos) {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                     "  %-12s:", lang_name);
                    has_combos = true;
                }
                offset += snprintf(buffer + offset, buffer_size - offset,
                                 " %s", combos[i].toolkit);
            }
        }

        if (has_combos) {
            offset += snprintf(buffer + offset, buffer_size - offset, "\n");
        }
    }

    offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    offset += snprintf(buffer + offset, buffer_size - offset,
                     "Usage: kryon run --target=<language>+<toolkit> file.kry\n");

    return buffer;
}
