/**

 * @file targets_command.c
 * @brief Kryon CLI Targets Command
 */
#include "lib9.h"
#include "target.h"

int targets_command(int argc, char *argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;

    fprintf(stderr, "Available targets:\n");

    // Check each target
    for (int i = 0; i < KRYON_TARGET_COUNT; i++) {
        KryonTargetType target = (KryonTargetType)i;
        const char *name = kryon_target_name(target);

        if (kryon_target_is_available(target)) {
            fprintf(stderr, "  ✓ %s\n", name);
        } else {
            fprintf(stderr, "  ✗ %s (unsupported)\n", name);
        }
    }

    fprintf(stderr, "\nLegend:\n");
    fprintf(stderr, "  ✓ = Available in this build\n");
    fprintf(stderr, "  ✗ = Not available (missing dependencies or not compiled)\n");

    return 0;
}
