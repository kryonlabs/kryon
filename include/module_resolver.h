/**
 * @file module_resolver.h
 * @brief Kryon Module Resolver - Module path resolution and validation
 *
 * This header defines the module resolution system that handles finding
 * modules across multiple search paths with platform-specific support
 * for TaijiOS system resources.
 *
 * Search Order:
 * 1. Relative to current file (./module.kry, ../lib/helper.kry)
 * 2. Project local (/kryon/modules/ on TaijiOS)
 * 3. System library (/lib/kryon/ on TaijiOS)
 * 4. Environment ($KRYON_MODULE_PATH)
 * 5. Built-in (<kryon/std> with angle brackets)
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_MODULE_RESOLVER_H
#define KRYON_MODULE_RESOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// MODULE RESOLVER
// =============================================================================

/**
 * @brief Module resolver context
 *
 * Manages search paths and resolution state for module imports.
 */
typedef struct ModuleResolver {
    const char **search_paths;     /**< Array of search paths */
    size_t path_count;             /**< Number of search paths */
    const char *project_root;      /**< Project root directory */
    const char *current_file_dir;  /**< Directory of file being compiled */
    bool is_taijios;               /**< Running on TaijiOS/Inferno */
} ModuleResolver;

/**
 * @brief Module resolution result
 */
typedef struct {
    char *resolved_path;           /**< Resolved absolute path (caller must free) */
    bool found;                    /**< Whether module was found */
    const char *error_message;     /**< Error description if !found */
} ModuleResolution;

// =============================================================================
// MODULE RESOLVER API
// =============================================================================

/**
 * @brief Create a module resolver
 *
 * Initializes resolver with default search paths based on platform.
 *
 * @param base_path Base path for relative resolution (usually current file directory)
 * @return Resolver instance or NULL on failure
 */
ModuleResolver* module_resolver_create(const char *base_path);

/**
 * @brief Destroy a module resolver
 *
 * @param resolver Resolver to destroy
 */
void module_resolver_destroy(ModuleResolver *resolver);

/**
 * @brief Resolve a module path to an absolute file path
 *
 * Searches through all configured paths to find the module.
 * Supports:
 * - Relative paths: ./module.kry, ../lib/helper.kry
 * - System paths: <kryon/std/theme.kry> (angle brackets)
 * - Environment: $KRYON_LIB/utils.kry
 * - TaijiOS paths: /lib/kryon/, /kryon/modules/
 *
 * @param resolver Module resolver
 * @param module_path Module path to resolve
 * @return Resolution result (caller must free resolved_path if found)
 */
ModuleResolution module_resolver_resolve(
    ModuleResolver *resolver,
    const char *module_path
);

/**
 * @brief Check if a path can be accessed
 *
 * Validates that the path is safe and accessible.
 * Prevents directory traversal attacks.
 *
 * @param resolver Module resolver
 * @param path Path to validate
 * @return true if path is safe and accessible
 */
bool module_resolver_can_access_path(
    ModuleResolver *resolver,
    const char *path
);

/**
 * @brief Add a search path to the resolver
 *
 * @param resolver Module resolver
 * @param path Path to add
 * @return true on success
 */
bool module_resolver_add_search_path(
    ModuleResolver *resolver,
    const char *path
);

/**
 * @brief Check if running on TaijiOS
 *
 * Detects TaijiOS/Inferno environment based on build flags
 * and available services.
 *
 * @return true if on TaijiOS
 */
bool module_resolver_is_taijios(void);

/**
 * @brief Normalize a path
 *
 * Resolves . and .. components, removes duplicate slashes.
 *
 * @param path Path to normalize
 * @return Normalized path (caller must free) or NULL on error
 */
char* module_resolver_normalize_path(const char *path);

/**
 * @brief Check if path is absolute
 *
 * @param path Path to check
 * @return true if absolute
 */
bool module_resolver_is_absolute(const char *path);

#ifdef __cplusplus
}
#endif

#endif // KRYON_MODULE_RESOLVER_H
