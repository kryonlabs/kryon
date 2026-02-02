/**
 * Kryon Platform Registry Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "platform_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_PLATFORMS 16
#define MAX_PLATFORM_ALIASES 32

// ============================================================================
// Registry State
// ============================================================================

typedef struct {
    const char* alias;
    const char* canonical_name;
} PlatformAlias;

static struct {
    const PlatformProfile* profiles[MAX_PLATFORMS];
    int count;
    bool initialized;
    PlatformAlias aliases[MAX_PLATFORM_ALIASES];
    int alias_count;
} g_registry = {0};

// ============================================================================
// Static Buffers
// ============================================================================

static char g_format_buffer[128];

// ============================================================================
// Platform Type String Conversion
// ============================================================================

static const char* PLATFORM_TYPE_STRINGS[] = {
    "terminal",
    "web",
    "desktop",
    "mobile",
    "taijios",
    "none"
};

PlatformType platform_type_from_string(const char* type_str) {
    if (!type_str) return PLATFORM_NONE;

    for (int i = 0; i <= PLATFORM_NONE; i++) {
        if (strcmp(type_str, PLATFORM_TYPE_STRINGS[i]) == 0) {
            return (PlatformType)i;
        }
    }

    return PLATFORM_NONE;
}

const char* platform_type_to_string(PlatformType type) {
    if (type < 0 || type > PLATFORM_NONE) {
        return "unknown";
    }
    return PLATFORM_TYPE_STRINGS[type];
}

bool platform_type_is_valid(PlatformType type) {
    return type >= PLATFORM_TERMINAL && type <= PLATFORM_TAIJIOS;
}

// ============================================================================
// Platform Registry API Implementation
// ============================================================================

bool platform_register(const PlatformProfile* profile) {
    if (!profile || !profile->name) {
        fprintf(stderr, "Error: Invalid platform profile\n");
        return false;
    }

    if (g_registry.count >= MAX_PLATFORMS) {
        fprintf(stderr, "Error: Platform registry full\n");
        return false;
    }

    // Check for duplicate
    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.profiles[i]->name, profile->name) == 0) {
            fprintf(stderr, "Warning: Platform '%s' already registered\n", profile->name);
            return false;
        }
    }

    g_registry.profiles[g_registry.count++] = profile;
    return true;
}

const PlatformProfile* platform_get_profile(const char* name) {
    if (!name) return NULL;

    // Resolve alias first
    const char* canonical_name = platform_resolve_alias(name);

    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.profiles[i]->name, canonical_name) == 0) {
            return g_registry.profiles[i];
        }
    }

    return NULL;
}

const char* platform_resolve_alias(const char* name) {
    if (!name) return NULL;

    // Check if it's an alias
    for (int i = 0; i < g_registry.alias_count; i++) {
        if (strcmp(g_registry.aliases[i].alias, name) == 0) {
            return g_registry.aliases[i].canonical_name;
        }
    }

    // Not an alias, return as-is
    return name;
}

bool platform_register_alias(const char* alias, const char* canonical_name) {
    if (!alias || !canonical_name) {
        fprintf(stderr, "Error: Invalid platform alias (NULL alias or canonical name)\n");
        return false;
    }

    if (g_registry.alias_count >= MAX_PLATFORM_ALIASES) {
        fprintf(stderr, "Error: Platform alias registry full\n");
        return false;
    }

    // Verify canonical platform exists
    const PlatformProfile* profile = platform_get_profile(canonical_name);
    if (!profile) {
        fprintf(stderr, "Error: Cannot register alias '%s' for unknown platform '%s'\n",
                alias, canonical_name);
        return false;
    }

    // Check for duplicate alias
    for (int i = 0; i < g_registry.alias_count; i++) {
        if (strcmp(g_registry.aliases[i].alias, alias) == 0) {
            fprintf(stderr, "Warning: Platform alias '%s' already registered\n", alias);
            return false;
        }
    }

    g_registry.aliases[g_registry.alias_count].alias = alias;
    g_registry.aliases[g_registry.alias_count].canonical_name = canonical_name;
    g_registry.alias_count++;

    return true;
}

const PlatformProfile* platform_get_profile_by_type(PlatformType type) {
    if (!platform_type_is_valid(type)) return NULL;

    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.profiles[i]->type == type) {
            return g_registry.profiles[i];
        }
    }

    return NULL;
}

bool platform_is_registered(const char* name) {
    return platform_get_profile(name) != NULL;
}

bool platform_supports_toolkit(const char* platform_name, const char* toolkit_name) {
    const PlatformProfile* profile = platform_get_profile(platform_name);
    if (!profile || !toolkit_name) return false;

    for (size_t i = 0; i < profile->toolkit_count; i++) {
        if (strcmp(profile->supported_toolkits[i], toolkit_name) == 0) {
            return true;
        }
    }

    return false;
}

bool platform_supports_language(const char* platform_name, const char* language_name) {
    const PlatformProfile* profile = platform_get_profile(platform_name);
    if (!profile || !language_name) return false;

    for (size_t i = 0; i < profile->language_count; i++) {
        if (strcmp(profile->supported_languages[i], language_name) == 0) {
            return true;
        }
    }

    return false;
}

size_t platform_get_all_registered(const PlatformProfile** out_profiles, size_t max_count) {
    if (!out_profiles || max_count == 0) {
        return 0;
    }

    size_t count = (size_t)g_registry.count < max_count ? (size_t)g_registry.count : max_count;
    memcpy(out_profiles, g_registry.profiles, count * sizeof(PlatformProfile*));
    return count;
}

const char* platform_get_default_toolkit(const char* platform_name) {
    const PlatformProfile* profile = platform_get_profile(platform_name);
    if (!profile) return NULL;
    return profile->default_toolkit;
}

// Mark registry as initialized (called by platform_profiles.c)
void platform_registry_mark_initialized(void) {
    g_registry.initialized = true;
}

void platform_registry_init(void) {
    if (g_registry.initialized) {
        return;  // Already initialized
    }

    // Register all built-in platforms
    extern void platform_register_builtins(void);
    platform_register_builtins();
}

void platform_registry_cleanup(void) {
    memset(&g_registry, 0, sizeof(g_registry));
}

void platform_print_table(void) {
    printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
    printf("                         Kryon Platform Registry\n");
    printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

    printf("%-15s %-12s %-20s %-12s\n", "Platform", "Type", "Default Toolkit", "Hot Reload");
    printf("%-15s %-12s %-20s %-12s\n", "──────────────", "────────────", "───────────────────", "────────────");

    for (int i = 0; i < g_registry.count; i++) {
        const PlatformProfile* profile = g_registry.profiles[i];
        printf("%-15s %-12s %-20s %-12s\n",
               profile->name,
               platform_type_to_string(profile->type),
               profile->default_toolkit ? profile->default_toolkit : "none",
               profile->supports_hot_reload ? "✓" : "✗");
    }

    printf("\n");
}

void platform_print_details(const char* platform_name) {
    if (platform_name) {
        // Print specific platform details
        const PlatformProfile* profile = platform_get_profile(platform_name);
        if (!profile) {
            fprintf(stderr, "Error: Unknown platform '%s'\n", platform_name);
            return;
        }

        printf("\n╌──────────────────────────────────────────────────────────────────────────────╌\n");
        printf("                         Platform: %s\n", profile->name);
        printf("╌──────────────────────────────────────────────────────────────────────────────╌\n\n");

        printf("Type: %s\n", platform_type_to_string(profile->type));
        printf("Default Toolkit: %s\n", profile->default_toolkit ? profile->default_toolkit : "none");
        printf("\n");

        printf("Capabilities:\n");
        printf("  Hot Reload: %s\n", profile->supports_hot_reload ? "Yes" : "No");
        printf("  Native Binaries: %s\n", profile->supports_native_binaries ? "Yes" : "No");
        printf("  Requires Compilation: %s\n", profile->requires_compilation ? "Yes" : "No");
        printf("\n");

        printf("Supported Toolkits (%zu):\n", profile->toolkit_count);
        for (size_t i = 0; i < profile->toolkit_count; i++) {
            printf("  - %s\n", profile->supported_toolkits[i]);
        }
        printf("\n");

        printf("Supported Languages (%zu):\n", profile->language_count);
        for (size_t i = 0; i < profile->language_count; i++) {
            printf("  - %s\n", profile->supported_languages[i]);
        }
        printf("\n");
    } else {
        // Print all platforms
        for (int i = 0; i < g_registry.count; i++) {
            platform_print_details(g_registry.profiles[i]->name);
        }
    }
}
