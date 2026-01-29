/**
 * @file module_resolver.c
 * @brief Module resolver implementation
 */

#include "module_resolver.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

// =============================================================================
// PLATFORM DETECTION
// =============================================================================

bool module_resolver_is_taijios(void) {
#ifdef KRYON_PLATFORM_TAIJIOS
    return true;
#elif defined(KRYON_PLUGIN_INFERNO)
    // Check if running in Inferno emu environment
    return (getenv("emuroot") != NULL);
#else
    return false;
#endif
}

// =============================================================================
// PATH UTILITIES
// =============================================================================

bool module_resolver_is_absolute(const char *path) {
    if (!path || !path[0]) {
        return false;
    }

    // Unix-style absolute path
    return path[0] == '/';
}

char* module_resolver_normalize_path(const char *path) {
    if (!path) {
        return NULL;
    }

    // For now, just duplicate the path
    // TODO: Implement proper normalization (remove .., ., duplicate /)
    return strdup(path);
}

// =============================================================================
// MODULE RESOLVER CREATION
// =============================================================================

ModuleResolver* module_resolver_create(const char *base_path) {
    ModuleResolver *resolver = kryon_alloc(sizeof(ModuleResolver));
    if (!resolver) {
        return NULL;
    }

    memset(resolver, 0, sizeof(ModuleResolver));

    // Set current file directory
    if (base_path) {
        resolver->current_file_dir = strdup(base_path);
    }

    // Detect platform
    resolver->is_taijios = module_resolver_is_taijios();

    // Initialize search paths array
    resolver->search_paths = kryon_alloc(sizeof(char*) * 16);
    if (!resolver->search_paths) {
        kryon_free(resolver);
        return NULL;
    }

    // Add default search paths
    size_t idx = 0;

    // 1. Current directory (if provided)
    if (base_path) {
        resolver->search_paths[idx++] = strdup(base_path);
    }

    // 2. Environment variable paths
    const char *kryon_path = getenv("KRYON_MODULE_PATH");
    if (kryon_path) {
        // Parse colon-separated paths
        char *path_copy = strdup(kryon_path);
        char *token = strtok(path_copy, ":");
        while (token && idx < 16) {
            resolver->search_paths[idx++] = strdup(token);
            token = strtok(NULL, ":");
        }
        free(path_copy);
    }

    // 3. TaijiOS-specific paths
    if (resolver->is_taijios) {
        resolver->search_paths[idx++] = strdup("/kryon/modules");
        resolver->search_paths[idx++] = strdup("/lib/kryon");
    }

    // 4. Standard system paths (Unix-like)
    resolver->search_paths[idx++] = strdup("/usr/local/lib/kryon");
    resolver->search_paths[idx++] = strdup("/usr/lib/kryon");

    resolver->path_count = idx;

    return resolver;
}

void module_resolver_destroy(ModuleResolver *resolver) {
    if (!resolver) {
        return;
    }

    if (resolver->search_paths) {
        for (size_t i = 0; i < resolver->path_count; i++) {
            free((void*)resolver->search_paths[i]);
        }
        kryon_free(resolver->search_paths);
    }

    if (resolver->current_file_dir) {
        free((void*)resolver->current_file_dir);
    }

    if (resolver->project_root) {
        free((void*)resolver->project_root);
    }

    kryon_free(resolver);
}

bool module_resolver_add_search_path(
    ModuleResolver *resolver,
    const char *path
) {
    if (!resolver || !path) {
        return false;
    }

    // Check capacity (simple fixed size for now)
    if (resolver->path_count >= 16) {
        return false;
    }

    resolver->search_paths[resolver->path_count++] = strdup(path);
    return true;
}

// =============================================================================
// PATH VALIDATION
// =============================================================================

bool module_resolver_can_access_path(
    ModuleResolver *resolver,
    const char *path
) {
    (void)resolver;

    if (!path) {
        return false;
    }

    // Check for directory traversal attacks
    if (strstr(path, "..") != NULL) {
        // Allow .. only in specific safe patterns
        // For now, be conservative
        // TODO: Implement proper path canonicalization
    }

    // Check if file exists and is readable
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }

    // Check if it's a regular file
    if (!S_ISREG(st.st_mode)) {
        return false;
    }

    // Check if readable
    if (access(path, R_OK) != 0) {
        return false;
    }

    return true;
}

// =============================================================================
// MODULE RESOLUTION
// =============================================================================

/**
 * Try to resolve a path relative to a base directory
 */
static char* try_resolve_relative(const char *base_dir, const char *module_path) {
    if (!base_dir || !module_path) {
        return NULL;
    }

    // Build full path
    size_t base_len = strlen(base_dir);
    size_t module_len = strlen(module_path);
    size_t total_len = base_len + 1 + module_len + 1;

    char *full_path = malloc(total_len);
    if (!full_path) {
        return NULL;
    }

    snprintf(full_path, total_len, "%s/%s", base_dir, module_path);

    // Check if it exists
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
        return full_path;
    }

    free(full_path);
    return NULL;
}

/**
 * Resolve angle-bracket includes (<kryon/std>)
 */
static char* resolve_system_include(ModuleResolver *resolver, const char *module_path) {
    // Remove angle brackets if present
    const char *clean_path = module_path;
    if (module_path[0] == '<') {
        clean_path = module_path + 1;
        size_t len = strlen(clean_path);
        if (len > 0 && clean_path[len-1] == '>') {
            char *temp = strdup(clean_path);
            temp[len-1] = '\0';
            clean_path = temp;
        }
    }

    // Try system library paths
    for (size_t i = 0; i < resolver->path_count; i++) {
        char *resolved = try_resolve_relative(resolver->search_paths[i], clean_path);
        if (resolved) {
            if (clean_path != module_path && clean_path != module_path + 1) {
                free((void*)clean_path);
            }
            return resolved;
        }
    }

    if (clean_path != module_path && clean_path != module_path + 1) {
        free((void*)clean_path);
    }

    return NULL;
}

ModuleResolution module_resolver_resolve(
    ModuleResolver *resolver,
    const char *module_path
) {
    ModuleResolution result = {
        .resolved_path = NULL,
        .found = false,
        .error_message = NULL
    };

    if (!resolver || !module_path) {
        result.error_message = "Invalid resolver or module path";
        return result;
    }

    // 1. Check if it's an absolute path
    if (module_resolver_is_absolute(module_path)) {
        if (module_resolver_can_access_path(resolver, module_path)) {
            result.resolved_path = strdup(module_path);
            result.found = true;
            return result;
        } else {
            result.error_message = "Absolute path not found or not accessible";
            return result;
        }
    }

    // 2. Check for angle brackets (system include)
    if (module_path[0] == '<') {
        char *resolved = resolve_system_include(resolver, module_path);
        if (resolved) {
            result.resolved_path = resolved;
            result.found = true;
            return result;
        } else {
            result.error_message = "System module not found";
            return result;
        }
    }

    // 3. Try relative to current file
    if (resolver->current_file_dir) {
        char *resolved = try_resolve_relative(resolver->current_file_dir, module_path);
        if (resolved) {
            result.resolved_path = resolved;
            result.found = true;
            return result;
        }
    }

    // 4. Try all search paths
    for (size_t i = 0; i < resolver->path_count; i++) {
        char *resolved = try_resolve_relative(resolver->search_paths[i], module_path);
        if (resolved) {
            result.resolved_path = resolved;
            result.found = true;
            return result;
        }
    }

    // Not found
    result.error_message = "Module not found in any search path";
    return result;
}
