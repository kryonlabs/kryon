/**
 * Languages Command (Short Form)
 *
 * Lists all available languages.
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
void language_registry_init(void);

typedef enum {
    LANGUAGE_TCL,
    LANGUAGE_LIMBO,
    LANGUAGE_C,
    LANGUAGE_KOTLIN,
    LANGUAGE_JAVASCRIPT,
    LANGUAGE_LUA,
    LANGUAGE_NONE
} LanguageType;

const char* language_type_to_string(LanguageType type);

typedef struct {
    const char* name;
    LanguageType type;
    const char* source_extension;
} LanguageProfile;

size_t language_get_all_registered(const LanguageProfile** out_profiles, size_t max_count);

int cmd_lang(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused

    // Initialize registry
    language_registry_init();

    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                         Kryon Languages\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    printf("%-15s %-20s %-15s\n", "Language", "Type", "Extension");
    printf("%-15s %-20s %-15s\n", "───────────────", "───────────────────", "───────────────");

    const LanguageProfile* profiles[16];
    size_t count = language_get_all_registered(profiles, 16);
    for (size_t i = 0; i < count; i++) {
        const LanguageProfile* profile = profiles[i];
        printf("%-15s %-20s %-15s\n",
               profile->name,
               language_type_to_string(profile->type),
               profile->source_extension);
    }

    printf("\n");

    printf("Usage:\n");
    printf("  kryon run --target=<language>+<toolkit>@<platform> file.kry\n");
    printf("\n");

    printf("Examples:\n");
    printf("  kryon run --target=c+sdl3@desktop main.kry\n");
    printf("  kryon run --target=limbo+tk@taiji main.kry\n");
    printf("  kryon run --target=javascript+dom@web main.kry\n");
    printf("\n");

    printf("Note: Each language supports specific toolkits on specific platforms.\n");
    printf("      Use 'kryon targets' to see valid language+toolkit@platform combinations.\n");
    printf("\n");

    return 0;
}
