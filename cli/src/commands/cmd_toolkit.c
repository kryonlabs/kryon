/**
 * Toolkits Command (Short Form)
 *
 * Lists all available toolkits.
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <string.h>

// Forward declarations
void combo_registry_init(void);

typedef struct {
    const char* language;
    const char* toolkit;
    const char* alias;
    int valid;
    const char* output_dir;
} Combo;

size_t combo_get_all(Combo* out_combos, size_t max_count);

int cmd_toolkit(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused

    // Initialize registry
    combo_registry_init();

    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                         Kryon Toolkits\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    printf("%-15s %-30s\n", "Toolkit", "Description");
    printf("%-15s %-30s\n", "───────────────", "──────────────────────────────");

    printf("%-15s %-30s\n", "tk", "Tcl/Tk widgets");
    printf("%-15s %-30s\n", "draw", "Inferno Draw API (Limbo)");
    printf("%-15s %-30s\n", "dom", "HTML/CSS (Web)");
    printf("%-15s %-30s\n", "sdl3", "SDL3 graphics");
    printf("%-15s %-30s\n", "raylib", "Raylib graphics");
    printf("%-15s %-30s\n", "terminal", "Terminal/console I/O");
    printf("%-15s %-30s\n", "android", "Android Views (Mobile)");

    printf("\n");

    printf("Valid Language Combinations:\n\n");

    // Get all combos and group by toolkit
    Combo combos[64];
    size_t combo_count = combo_get_all(combos, 64);

    // Track which toolkits we've seen
    const char* seen_toolkits[16];
    int seen_count = 0;

    for (size_t i = 0; i < combo_count; i++) {
        const char* toolkit = combos[i].toolkit;

        // Check if we've already shown this toolkit
        int already_shown = 0;
        for (int j = 0; j < seen_count; j++) {
            if (strcmp(seen_toolkits[j], toolkit) == 0) {
                already_shown = 1;
                break;
            }
        }

        if (!already_shown) {
            // Find all languages for this toolkit
            char lang_str[256] = "";
            int lang_count = 0;
            for (size_t k = 0; k < combo_count; k++) {
                if (strcmp(combos[k].toolkit, toolkit) == 0) {
                    if (lang_count > 0) strcat(lang_str, ", ");
                    strcat(lang_str, combos[k].language);
                    lang_count++;
                }
            }

            printf("  %-12s → %s\n", toolkit, lang_str);
            seen_toolkits[seen_count++] = toolkit;
        }
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

    printf("Note: Not all toolkits are available on all platforms.\n");
    printf("      Use 'kryon targets' to see valid combinations.\n");
    printf("\n");

    return 0;
}
