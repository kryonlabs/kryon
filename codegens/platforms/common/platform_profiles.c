/**
 * Kryon Built-in Platform Profiles
 *
 * Defines all built-in platform profiles with their capabilities,
 * supported languages, and default toolkits.
 */

#define _POSIX_C_SOURCE 200809L

#include "platform_registry.h"

// ============================================================================
// Static Arrays for Platform Profiles
// ============================================================================

// Terminal platform
static const char* terminal_toolkits[] = {"terminal", NULL};
static const char* terminal_languages[] = {"c", "python", "rust", "tcl", NULL};

// Web platform
static const char* web_toolkits[] = {"dom", NULL};
static const char* web_languages[] = {"javascript", "typescript", NULL};

// Desktop platform
static const char* desktop_toolkits[] = {"sdl3", "raylib", "tk", NULL};
static const char* desktop_languages[] = {"c", "lua", "python", "rust", "tcl", NULL};

// Mobile platform
static const char* mobile_toolkits[] = {"android", NULL};
static const char* mobile_languages[] = {"kotlin", "java", NULL};

// TaijiOS platform
static const char* taijios_toolkits[] = {"tk", NULL};
static const char* taijios_languages[] = {"limbo", NULL};

// ============================================================================
// Platform Profile Definitions
// ============================================================================

static const PlatformProfile g_platform_terminal = {
    .name = "terminal",
    .type = PLATFORM_TERMINAL,
    .supported_toolkits = terminal_toolkits,
    .toolkit_count = 1,
    .supported_languages = terminal_languages,
    .language_count = 4,
    .default_toolkit = "terminal",
    .supports_hot_reload = false,
    .supports_native_binaries = true,
    .requires_compilation = true,
};

static const PlatformProfile g_platform_web = {
    .name = "web",
    .type = PLATFORM_WEB,
    .supported_toolkits = web_toolkits,
    .toolkit_count = 1,
    .supported_languages = web_languages,
    .language_count = 2,
    .default_toolkit = "dom",
    .supports_hot_reload = true,
    .supports_native_binaries = false,
    .requires_compilation = false,
};

static const PlatformProfile g_platform_desktop = {
    .name = "desktop",
    .type = PLATFORM_DESKTOP,
    .supported_toolkits = desktop_toolkits,
    .toolkit_count = 3,
    .supported_languages = desktop_languages,
    .language_count = 5,
    .default_toolkit = "sdl3",  // SDL3 is preferred over Raylib
    .supports_hot_reload = false,
    .supports_native_binaries = true,
    .requires_compilation = true,
};

static const PlatformProfile g_platform_mobile = {
    .name = "mobile",
    .type = PLATFORM_MOBILE,
    .supported_toolkits = mobile_toolkits,
    .toolkit_count = 1,
    .supported_languages = mobile_languages,
    .language_count = 2,
    .default_toolkit = "android",
    .supports_hot_reload = false,
    .supports_native_binaries = true,
    .requires_compilation = true,
};

static const PlatformProfile g_platform_taijios = {
    .name = "taijios",
    .type = PLATFORM_TAIJIOS,
    .supported_toolkits = taijios_toolkits,
    .toolkit_count = 1,
    .supported_languages = taijios_languages,
    .language_count = 1,
    .default_toolkit = "tk",
    .supports_hot_reload = false,
    .supports_native_binaries = false,
    .requires_compilation = true,
};

// ============================================================================
// Platform Profiles Initialization
// ============================================================================

void platform_register_builtins(void) {
    // Register all built-in platforms
    platform_register(&g_platform_terminal);
    platform_register(&g_platform_web);
    platform_register(&g_platform_desktop);
    platform_register(&g_platform_mobile);
    platform_register(&g_platform_taijios);

    // Register platform aliases
    platform_register_alias("taiji", "taijios");   // Short name for TaijiOS
    platform_register_alias("inferno", "taijios"); // Inferno was the predecessor to TaijiOS

    // Mark registry as initialized
    extern void platform_registry_mark_initialized(void);
    platform_registry_mark_initialized();
}
