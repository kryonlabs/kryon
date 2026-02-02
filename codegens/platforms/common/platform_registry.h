/**
 * Kryon Platform Registry
 *
 * Registry system for platform profiles.
 * Platforms define deployment targets (desktop, web, mobile, etc.) with
 * associated capabilities and default toolkits.
 */

#ifndef PLATFORM_REGISTRY_H
#define PLATFORM_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Platform Type Enumeration
// ============================================================================

/**
 * Supported deployment platforms
 */
typedef enum {
    PLATFORM_TERMINAL,     // Terminal/console applications
    PLATFORM_WEB,          // Web browser applications
    PLATFORM_DESKTOP,      // Desktop applications (windowed)
    PLATFORM_MOBILE,       // Mobile applications (Android/iOS)
    PLATFORM_TAIJIOS,      // TaijiOS applications
    PLATFORM_NONE          // No platform
} PlatformType;

/**
 * Convert platform type string to enum
 * @param type_str Platform type string (e.g., "terminal", "web", "desktop")
 * @return PlatformType enum, or PLATFORM_NONE if invalid
 */
PlatformType platform_type_from_string(const char* type_str);

/**
 * Convert platform enum to string
 * @param type PlatformType enum
 * @return Static string representation, or "unknown" if invalid
 */
const char* platform_type_to_string(PlatformType type);

/**
 * Check if platform type is valid
 * @param type PlatformType to check
 * @return true if valid, false otherwise
 */
bool platform_type_is_valid(PlatformType type);

// ============================================================================
// Platform Profile
// ============================================================================

/**
 * Platform profile
 * Defines all characteristics of a deployment platform
 */
typedef struct {
    const char* name;              // Platform name (e.g., "desktop", "web")

    PlatformType type;             // Platform type enum

    // Capabilities
    const char** supported_toolkits;  // Array of toolkit names
    size_t toolkit_count;             // Number of supported toolkits

    const char** supported_languages; // Array of language names
    size_t language_count;            // Number of supported languages

    // Defaults
    const char* default_toolkit;      // Default toolkit for this platform

    // Feature flags
    bool supports_hot_reload;         // Platform supports hot-reload during development
    bool supports_native_binaries;    // Platform can compile to native binaries
    bool requires_compilation;        // Platform requires compilation step

} PlatformProfile;

// ============================================================================
// Platform Registry API
// ============================================================================

/**
 * Register a platform profile
 * @param profile Platform profile to register (static storage, not copied)
 * @return true on success, false on failure
 */
bool platform_register(const PlatformProfile* profile);

/**
 * Get platform profile by name
 * @param name Platform name (e.g., "desktop", "web", "mobile")
 * @return Platform profile, or NULL if not found
 */
const PlatformProfile* platform_get_profile(const char* name);

/**
 * Resolve platform alias to canonical name
 * Handles aliases like "taiji" → "taijios", "inferno" → "taijios"
 * @param name Platform name or alias
 * @return Canonical platform name, or the input if not an alias
 */
const char* platform_resolve_alias(const char* name);

/**
 * Register a platform alias
 * @param alias Alias name (e.g., "taiji", "inferno")
 * @param canonical_name Canonical platform name (e.g., "taijios")
 * @return true on success, false on failure
 */
bool platform_register_alias(const char* alias, const char* canonical_name);

/**
 * Get platform profile by type
 * @param type PlatformType enum
 * @return Platform profile, or NULL if not found
 */
const PlatformProfile* platform_get_profile_by_type(PlatformType type);

/**
 * Check if platform is registered
 * @param name Platform name
 * @return true if registered, false otherwise
 */
bool platform_is_registered(const char* name);

/**
 * Check if platform supports a specific toolkit
 * @param platform_name Platform name
 * @param toolkit_name Toolkit name
 * @return true if supported, false otherwise
 */
bool platform_supports_toolkit(const char* platform_name, const char* toolkit_name);

/**
 * Check if platform supports a specific language
 * @param platform_name Platform name
 * @param language_name Language name
 * @return true if supported, false otherwise
 */
bool platform_supports_language(const char* platform_name, const char* language_name);

/**
 * Get all registered platforms
 * @param out_profiles Output array for platform profiles
 * @param max_count Maximum number of platforms to return
 * @return Number of platforms returned
 */
size_t platform_get_all_registered(const PlatformProfile** out_profiles, size_t max_count);

/**
 * Get default toolkit for a platform
 * @param platform_name Platform name
 * @return Default toolkit name, or NULL if not found
 */
const char* platform_get_default_toolkit(const char* platform_name);

/**
 * Initialize platform registry with all built-in platforms
 * Called automatically during system initialization
 */
void platform_registry_init(void);

/**
 * Internal function: Mark registry as initialized
 * Called by platform_profiles.c
 */
void platform_registry_mark_initialized(void);

/**
 * Internal function: Register all built-in platform profiles
 * Called by platform_registry_init()
 */
void platform_register_builtins(void);

/**
 * Cleanup platform registry
 */
void platform_registry_cleanup(void);

/**
 * Print all registered platforms in a table format
 */
void platform_print_table(void);

/**
 * Print platform details including supported languages and toolkits
 * @param platform_name Platform name (NULL to print all platforms)
 */
void platform_print_details(const char* platform_name);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_REGISTRY_H
