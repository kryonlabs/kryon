/**
 * @file target_resolver.c
 * @brief Target resolution and validation implementation
 */

#include "target.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// AVAILABILITY DETECTION
// =============================================================================

/**
 * Check if a renderer is available at compile time and runtime
 */
static bool is_renderer_available(KryonRendererType renderer) {
    switch (renderer) {
        case KRYON_RENDERER_TYPE_SDL2:
#ifdef HAVE_RENDERER_SDL2
            return true;
#else
            return false;
#endif

        case KRYON_RENDERER_TYPE_RAYLIB:
#ifdef HAVE_RENDERER_RAYLIB
            return true;
#else
            return false;
#endif

        case KRYON_RENDERER_TYPE_WEB:
            // Web renderer is always available
            return true;

        case KRYON_RENDERER_TYPE_KRBVIEW:
#ifdef HAVE_RENDERER_KRBVIEW
            return true;
#else
            return false;
#endif

        default:
            return false;
    }
}

bool kryon_target_is_available(KryonTargetType target) {
    switch (target) {
        case KRYON_TARGET_SDL2:
            return is_renderer_available(KRYON_RENDERER_TYPE_SDL2);

        case KRYON_TARGET_RAYLIB:
            return is_renderer_available(KRYON_RENDERER_TYPE_RAYLIB);

        case KRYON_TARGET_WEB:
            // Web target is always available
            return true;

        case KRYON_TARGET_EMU:
            return is_renderer_available(KRYON_RENDERER_TYPE_KRBVIEW);

        default:
            return false;
    }
}

// =============================================================================
// TARGET RESOLUTION
// =============================================================================

KryonTargetResolution kryon_target_resolve(KryonTargetType target) {
    KryonTargetResolution result = {
        .target = target,
        .renderer = KRYON_RENDERER_TYPE_NONE,
        .available = false,
        .error_message = NULL
    };

    // Check if target is available
    if (!kryon_target_is_available(target)) {
        result.error_message = "Target not available in this build";
        return result;
    }

    // Resolve based on target type
    switch (target) {
        case KRYON_TARGET_SDL2:
            result.renderer = KRYON_RENDERER_TYPE_SDL2;
            result.available = true;
            break;

        case KRYON_TARGET_RAYLIB:
            result.renderer = KRYON_RENDERER_TYPE_RAYLIB;
            result.available = true;
            break;

        case KRYON_TARGET_WEB:
            result.renderer = KRYON_RENDERER_TYPE_WEB;
            result.available = true;
            break;

        case KRYON_TARGET_EMU:
            result.renderer = KRYON_RENDERER_TYPE_KRBVIEW;
            result.available = true;
            break;

        default:
            result.error_message = "Unknown target type";
            break;
    }

    return result;
}

// =============================================================================
// PARSING
// =============================================================================

bool kryon_target_parse(const char *name, KryonTargetType *out_target) {
    if (!name || !out_target) {
        return false;
    }

    if (strcmp(name, "sdl2") == 0) {
        *out_target = KRYON_TARGET_SDL2;
        return true;
    }
    else if (strcmp(name, "raylib") == 0) {
        *out_target = KRYON_TARGET_RAYLIB;
        return true;
    }
    else if (strcmp(name, "web") == 0) {
        *out_target = KRYON_TARGET_WEB;
        return true;
    }
    else if (strcmp(name, "emu") == 0) {
        *out_target = KRYON_TARGET_EMU;
        return true;
    }

    return false;
}

// =============================================================================
// STRING CONVERSION
// =============================================================================

const char* kryon_target_name(KryonTargetType target) {
    switch (target) {
        case KRYON_TARGET_SDL2:   return "sdl2";
        case KRYON_TARGET_RAYLIB: return "raylib";
        case KRYON_TARGET_WEB:    return "web";
        case KRYON_TARGET_EMU:    return "emu";
        default:                  return "unknown";
    }
}

const char* kryon_renderer_name(KryonRendererType renderer) {
    switch (renderer) {
        case KRYON_RENDERER_TYPE_NONE:     return "none";
        case KRYON_RENDERER_TYPE_SDL2:     return "sdl2";
        case KRYON_RENDERER_TYPE_RAYLIB:   return "raylib";
        case KRYON_RENDERER_TYPE_WEB:      return "web";
        case KRYON_RENDERER_TYPE_KRBVIEW:  return "krbview";
        default:                           return "unknown";
    }
}
