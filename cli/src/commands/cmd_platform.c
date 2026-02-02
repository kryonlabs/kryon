/**
 * Platforms Command (Short Form)
 *
 * Lists all available platforms.
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <string.h>

int cmd_platform(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused

    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                         Kryon Platforms\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    printf("%-15s %-30s\n", "Platform", "Description");
    printf("%-15s %-30s\n", "───────────────", "──────────────────────────────");

    printf("%-15s %-30s\n", "web", "Web browser applications");
    printf("%-15s %-30s\n", "desktop", "Desktop applications");
    printf("%-15s %-30s\n", "mobile", "Mobile applications");
    printf("%-15s %-30s\n", "taijios", "TaijiOS applications");
    printf("%-15s %-30s\n", "terminal", "Terminal/console applications");

    printf("\n");

    printf("Each platform supports specific toolkits and languages.\n");
    printf("Use 'kryon targets' to see valid language+toolkit@platform combinations.\n");
    printf("\n");

    printf("Usage:\n");
    printf("  kryon run --target=<language>+<toolkit>@<platform> file.kry\n");
    printf("\n");

    printf("Examples:\n");
    printf("  kryon run --target=javascript+dom@web main.kry\n");
    printf("  kryon run --target=c+sdl3@desktop main.kry\n");
    printf("  kryon run --target=kotlin+android@mobile main.kry\n");
    printf("  kryon run --target=limbo+tk@taiji main.kry\n");
    printf("  kryon run --target=c+terminal@terminal main.kry\n");
    printf("\n");

    printf("Platform aliases:\n");
    printf("  taiji, inferno → taijios\n");
    printf("\n");

    return 0;
}
