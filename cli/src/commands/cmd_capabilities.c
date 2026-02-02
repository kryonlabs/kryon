/**
 * Capabilities Command
 *
 * Displays valid language+toolkit combinations
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
void combo_registry_init(void);
void combo_print_valid_targets(bool include_aliases);

typedef struct {
    const char* language;
    const char* toolkit;
    const char* alias;
    bool valid;
    const char* output_dir;
} Combo;

size_t combo_get_all(Combo* out_combos, size_t max_count);
size_t combo_get_for_language(const char* language, Combo* out_combos, size_t max_count);
size_t combo_get_for_toolkit(const char* toolkit, Combo* out_combos, size_t max_count);
bool combo_is_valid(const char* language, const char* toolkit);

int cmd_capabilities(int argc, char** argv) {
    (void)argc;  // Unused

    // Initialize registry
    combo_registry_init();

    // Parse command-line arguments (basic support for now)
    const char* filter_lang = NULL;
    const char* filter_toolkit = NULL;

    for (int i = 0; i < argc && argv[i]; i++) {
        if (strncmp(argv[i], "--lang=", 7) == 0) {
            filter_lang = argv[i] + 7;
        } else if (strncmp(argv[i], "--toolkit=", 10) == 0) {
            filter_toolkit = argv[i] + 10;
        }
    }

    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                         Kryon Capabilities\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    // Filter by language
    if (filter_lang) {
        printf("Language: %s\n\n", filter_lang);

        printf("Valid Toolkits (by platform):\n");
        Combo combos[32];
        size_t count = combo_get_for_language(filter_lang, combos, 32);
        if (count == 0) {
            printf("  (no valid combinations - unknown language)\n");
        } else {
            for (size_t i = 0; i < count; i++) {
                printf("  %s\n", combos[i].toolkit);
            }
        }
        printf("\n");
        printf("Note: Use 'kryon targets' to see platform-specific combinations.\n");
        printf("\n");
        return 0;
    }

    // Filter by toolkit
    if (filter_toolkit) {
        printf("Toolkit: %s\n\n", filter_toolkit);

        printf("Valid Languages:\n");
        Combo combos[32];
        size_t count = combo_get_for_toolkit(filter_toolkit, combos, 32);
        if (count == 0) {
            printf("  (no valid combinations - unknown toolkit)\n");
        } else {
            for (size_t i = 0; i < count; i++) {
                printf("  %s\n", combos[i].language);
            }
        }
        printf("\n");
        printf("Note: Use 'kryon targets' to see platform-specific combinations.\n");
        printf("\n");
        return 0;
    }

    // Show all combinations
    printf("Valid Language+Toolkit Combinations (technical compatibility):\n\n");
    combo_print_valid_targets(0);
    printf("\n");

    printf("Note: This shows technical language+toolkit compatibility.\n");
    printf("      Actual deployment requires specifying a platform:\n");
    printf("      language+toolkit@platform (e.g., c+sdl3@desktop)\n");
    printf("      Use 'kryon targets' to see valid platform-specific combinations.\n");
    printf("\n");

    printf("Usage:\n");
    printf("  kryon capabilities                    # Show all combinations\n");
    printf("  kryon capabilities --lang=<language>  # Show toolkits for language\n");
    printf("  kryon capabilities --toolkit=<toolkit> # Show languages for toolkit\n");
    printf("\n");

    printf("Examples:\n");
    printf("  kryon capabilities --lang=c\n");
    printf("  kryon capabilities --toolkit=sdl3\n");
    printf("\n");

    return 0;
}
