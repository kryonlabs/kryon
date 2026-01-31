/**
 * Targets Command
 * Lists all available build targets (built-in, custom, and aliases)
 */

#include "kryon_cli.h"
#include "target_validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

/* Target capability flags (match target_handlers.c) */
#define TARGET_CAN_BUILD  (1 << 0)
#define TARGET_CAN_RUN    (1 << 1)
#define TARGET_CAN_INSTALL (1 << 2)

/* Built-in target list */
static const char* BUILTIN_TARGETS[] = {
    "web", "limbo", "sdl3", "raylib", "android", NULL
};

/* Target aliases mapping */
typedef struct {
    const char* alias;
    const char* target;
    const char* description;
} TargetAlias;

static const TargetAlias TARGET_ALIASES[] = {
    {"dis", "limbo", "DIS bytecode generation"},
    {"emu", "limbo", "TaijiOS emulator target"},
    {"kotlin", "android", "Android with Kotlin"},
    {NULL, NULL, NULL}
};

/* Built-in target descriptions */
static const char* get_target_description(const char* target) {
    if (strcmp(target, "web") == 0) {
        return "HTML/WebAssembly target";
    } else if (strcmp(target, "limbo") == 0) {
        return "TaijiOS DIS bytecode (via Limbo)";
    } else if (strcmp(target, "sdl3") == 0) {
        return "SDL3 desktop target";
    } else if (strcmp(target, "raylib") == 0) {
        return "Raylib desktop target";
    } else if (strcmp(target, "android") == 0) {
        return "Android/Kotlin target";
    }
    return "";
}

/* Check if a target is built-in */
static bool is_builtin_target(const char* name) {
    for (int i = 0; BUILTIN_TARGETS[i] != NULL; i++) {
        if (strcmp(name, BUILTIN_TARGETS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Get capabilities string for a target */
static const char* get_capabilities_string(const char* target_name) {
    // Query the target handler for capabilities
    TargetHandler* handler = target_handler_find(target_name);
    if (!handler) {
        return "";
    }

    static char caps_buf[64];
    caps_buf[0] = '\0';

    bool has_build = target_has_capability(target_name, TARGET_CAN_BUILD);
    bool has_run = target_has_capability(target_name, TARGET_CAN_RUN);
    bool has_install = target_has_capability(target_name, TARGET_CAN_INSTALL);

    if (has_build && has_run && has_install) {
        snprintf(caps_buf, sizeof(caps_buf), "[BUILD, RUN, INSTALL]");
    } else if (has_build && has_run) {
        snprintf(caps_buf, sizeof(caps_buf), "[BUILD, RUN]");
    } else if (has_build && has_install) {
        snprintf(caps_buf, sizeof(caps_buf), "[BUILD, INSTALL]");
    } else if (has_build) {
        snprintf(caps_buf, sizeof(caps_buf), "[BUILD]");
    } else if (has_run) {
        snprintf(caps_buf, sizeof(caps_buf), "[RUN]");
    } else {
        snprintf(caps_buf, sizeof(caps_buf), "");
    }

    return caps_buf;
}

/* Print usage/help */
static void print_usage(void) {
    printf("Usage: kryon targets [options]\n\n");
    printf("Options:\n");
    printf("  --verbose         Show detailed information\n");
    printf("  --built-in        Show only built-in targets\n");
    printf("  --custom          Show only custom targets\n");
    printf("  --aliases         Show only aliases\n");
    printf("  --combos          Show only language+toolkit combinations\n");
    printf("  --help            Show this help\n\n");
    printf("Description:\n");
    printf("  Lists all available build targets including built-in targets,\n");
    printf("  aliases, custom targets from kryon.toml, and language+toolkit combos.\n");
}

/* List built-in targets */
static void list_builtin_targets(bool verbose) {
    (void)verbose;  // Reserved for future use
    printf("\n%s%sBuilt-in Targets:%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    for (int i = 0; BUILTIN_TARGETS[i] != NULL; i++) {
        const char* name = BUILTIN_TARGETS[i];
        const char* caps = get_capabilities_string(name);
        const char* desc = get_target_description(name);

        printf("  %s%-10s%s %s%-20s%s %s%s%s\n",
               COLOR_BOLD, name, COLOR_RESET,
               COLOR_GREEN, caps, COLOR_RESET,
               COLOR_DIM, desc, COLOR_RESET);
    }
}

/* List aliases */
static void list_aliases(bool verbose) {
    (void)verbose;  // Reserved for future use
    printf("\n%s%sAliases:%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    for (int i = 0; TARGET_ALIASES[i].alias != NULL; i++) {
        printf("  %s%-10s%s %s→%s %-10s %s%s%s\n",
               COLOR_BOLD, TARGET_ALIASES[i].alias, COLOR_RESET,
               COLOR_YELLOW, COLOR_RESET,
               TARGET_ALIASES[i].target,
               COLOR_DIM, TARGET_ALIASES[i].description, COLOR_RESET);
    }
}

/* List language+toolkit combinations */
static void list_lang_toolkit_combos(bool verbose) {
    (void)verbose;  // Reserved for future use
    printf("\n%s%sLanguage+Toolkit Combinations:%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("  (Use --help for more info on combo syntax)\n\n");

    // Use the new validation system to print all valid combos
    target_print_all_valid();
}

/* List custom targets from config */
static void list_custom_targets(const KryonConfig* config, bool verbose) {
    (void)verbose;  // Reserved for future use
    printf("\n%s%sCustom Targets (from kryon.toml):%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    bool found_custom = false;

    // Check if config has custom targets
    if (config && config->targets_count > 0) {
        for (int i = 0; i < config->targets_count; i++) {
            const char* name = config->targets[i].name;

            // Skip built-in targets
            if (is_builtin_target(name)) {
                continue;
            }

            found_custom = true;

            // Get capabilities (command targets have BUILD and RUN if commands are defined)
            const char* caps = get_capabilities_string(name);
            if (caps[0] == '\0') {
                // For command targets without handlers, infer from config
                bool has_build = config->targets[i].build_cmd != NULL;
                bool has_run = config->targets[i].run_cmd != NULL;

                if (has_build && has_run) {
                    caps = "[BUILD, RUN]";
                } else if (has_build) {
                    caps = "[BUILD]";
                } else if (has_run) {
                    caps = "[RUN]";
                } else {
                    caps = "";
                }
            }

            printf("  %s%-10s%s %s%-20s%s %sCommand target%s\n",
                   COLOR_BOLD, name, COLOR_RESET,
                   COLOR_GREEN, caps, COLOR_RESET,
                   COLOR_DIM, COLOR_RESET);
        }
    }

    if (!found_custom) {
        printf("  %s(none)%s\n", COLOR_DIM, COLOR_RESET);
    }
}

/* Main command implementation */
int cmd_targets(int argc, char** argv) {
    bool show_verbose = false;
    bool filter_builtin = false;
    bool filter_custom = false;
    bool filter_aliases = false;

    // Parse flags
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            show_verbose = true;
        } else if (strcmp(argv[i], "--built-in") == 0) {
            filter_builtin = true;
        } else if (strcmp(argv[i], "--custom") == 0) {
            filter_custom = true;
        } else if (strcmp(argv[i], "--aliases") == 0) {
            filter_aliases = true;
        } else if (strcmp(argv[i], "--combos") == 0) {
            // Show only language+toolkit combinations
            target_validation_initialize();
            list_lang_toolkit_combos(show_verbose);
            target_validation_cleanup();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    // Initialize validation system
    target_validation_initialize();

    // If no filters specified, show all
    bool show_all = !filter_builtin && !filter_custom && !filter_aliases;

    // Try to load config (optional - not required for built-in targets)
    KryonConfig* config = config_find_and_load();

    if (!config && show_all) {
        // Show friendly message if no kryon.toml found
        printf("\n%s%sNote:%s No kryon.toml found in current directory.\n",
               COLOR_YELLOW, COLOR_BOLD, COLOR_RESET);
        printf("      Showing built-in targets only.\n");
    }

    // Print header
    printf("\n%s%sKryon Build Targets%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    // Show sections based on filters
    if (show_all || filter_builtin) {
        list_builtin_targets(show_verbose);
    }

    if (show_all || filter_aliases) {
        list_aliases(show_verbose);
    }

    if (show_all || filter_custom) {
        list_custom_targets(config, show_verbose);
    }

    // Show language+toolkit combinations when showing all
    if (show_all) {
        list_lang_toolkit_combos(show_verbose);
    }

    // Count total targets
    int total = 0;

    // Count built-in
    for (int i = 0; BUILTIN_TARGETS[i] != NULL; i++) {
        total++;
    }

    // Count aliases
    for (int i = 0; TARGET_ALIASES[i].alias != NULL; i++) {
        total++;
    }

    // Count custom
    if (config && config->targets_count > 0) {
        for (int i = 0; i < config->targets_count; i++) {
            if (!is_builtin_target(config->targets[i].name)) {
                total++;
            }
        }
    }

    printf("\n%sTotal: %d target%s available%s\n",
           COLOR_DIM, total, total == 1 ? "" : "s", COLOR_RESET);

    // Cleanup
    if (config) {
        config_free(config);
    }

    target_validation_cleanup();

    return 0;
}
