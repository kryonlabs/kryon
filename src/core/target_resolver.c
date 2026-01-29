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

        case KRYON_RENDERER_TYPE_KRBVIEW:
#ifdef HAVE_RENDERER_KRBVIEW
            return true;
#else
            return false;
#endif

        case KRYON_RENDERER_TYPE_WEB:
            // Web renderer is always available
            return true;

        case KRYON_RENDERER_TYPE_TERMINAL:
            // Terminal renderer is always available
            return true;

        default:
            return false;
    }
}

bool kryon_target_is_available(KryonTargetType target) {
    switch (target) {
        case KRYON_TARGET_NATIVE:
            // Native target needs SDL2 or Raylib
            return is_renderer_available(KRYON_RENDERER_TYPE_SDL2) ||
                   is_renderer_available(KRYON_RENDERER_TYPE_RAYLIB);

        case KRYON_TARGET_EMU:
            // Emu target needs krbview renderer and Inferno plugin
#if defined(HAVE_RENDERER_KRBVIEW) && defined(KRYON_PLUGIN_INFERNO)
            return true;
#else
            return false;
#endif

        case KRYON_TARGET_WEB:
            // Web target is always available
            return true;

        case KRYON_TARGET_TERMINAL:
            // Terminal target is always available
            return true;

        default:
            return false;
    }
}

// =============================================================================
// TARGET RESOLUTION
// =============================================================================

KryonTargetResolution kryon_target_resolve(
    KryonTargetType target,
    KryonRendererType preferred_renderer
) {
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
        case KRYON_TARGET_NATIVE:
            // If user specified a preferred renderer, try it first
            if (preferred_renderer == KRYON_RENDERER_TYPE_SDL2 &&
                is_renderer_available(KRYON_RENDERER_TYPE_SDL2)) {
                result.renderer = KRYON_RENDERER_TYPE_SDL2;
                result.available = true;
            }
            else if (preferred_renderer == KRYON_RENDERER_TYPE_RAYLIB &&
                     is_renderer_available(KRYON_RENDERER_TYPE_RAYLIB)) {
                result.renderer = KRYON_RENDERER_TYPE_RAYLIB;
                result.available = true;
            }
            // Otherwise, use fallback order: SDL2 > Raylib
            else if (is_renderer_available(KRYON_RENDERER_TYPE_SDL2)) {
                result.renderer = KRYON_RENDERER_TYPE_SDL2;
                result.available = true;
            }
            else if (is_renderer_available(KRYON_RENDERER_TYPE_RAYLIB)) {
                result.renderer = KRYON_RENDERER_TYPE_RAYLIB;
                result.available = true;
            }
            else {
                result.error_message = "No GUI renderer available (SDL2 or Raylib required)";
            }
            break;

        case KRYON_TARGET_EMU:
            // Emu target must use krbview
            if (is_renderer_available(KRYON_RENDERER_TYPE_KRBVIEW)) {
                result.renderer = KRYON_RENDERER_TYPE_KRBVIEW;
                result.available = true;
            } else {
                result.error_message = "krbview renderer not available (requires Inferno build)";
            }
            break;

        case KRYON_TARGET_WEB:
            result.renderer = KRYON_RENDERER_TYPE_WEB;
            result.available = true;
            break;

        case KRYON_TARGET_TERMINAL:
            result.renderer = KRYON_RENDERER_TYPE_TERMINAL;
            result.available = true;
            break;

        default:
            result.error_message = "Unknown target type";
            break;
    }

    return result;
}

// =============================================================================
// VALIDATION
// =============================================================================

bool kryon_target_validate(
    KryonTargetType target,
    KryonRendererType renderer,
    char *error_buffer,
    size_t error_buffer_size
) {
    // Check valid combinations
    switch (target) {
        case KRYON_TARGET_NATIVE:
            if (renderer != KRYON_RENDERER_TYPE_SDL2 &&
                renderer != KRYON_RENDERER_TYPE_RAYLIB) {
                if (error_buffer && error_buffer_size > 0) {
                    snprintf(error_buffer, error_buffer_size,
                            "Target 'native' requires SDL2 or Raylib renderer, got '%s'",
                            kryon_renderer_name(renderer));
                }
                return false;
            }
            break;

        case KRYON_TARGET_EMU:
            if (renderer != KRYON_RENDERER_TYPE_KRBVIEW) {
                if (error_buffer && error_buffer_size > 0) {
                    snprintf(error_buffer, error_buffer_size,
                            "Target 'emu' requires krbview renderer, got '%s'\n\n"
                            "The 'emu' target runs in TaijiOS emu and uses krbview for rendering.\n"
                            "SDL2 and Raylib are only compatible with the 'native' target.\n\n"
                            "Did you mean:\n"
                            "  kryon run app.kry --target native --renderer %s",
                            kryon_renderer_name(renderer),
                            kryon_renderer_name(renderer));
                }
                return false;
            }
            break;

        case KRYON_TARGET_WEB:
            if (renderer != KRYON_RENDERER_TYPE_WEB) {
                if (error_buffer && error_buffer_size > 0) {
                    snprintf(error_buffer, error_buffer_size,
                            "Target 'web' requires web renderer, got '%s'",
                            kryon_renderer_name(renderer));
                }
                return false;
            }
            break;

        case KRYON_TARGET_TERMINAL:
            if (renderer != KRYON_RENDERER_TYPE_TERMINAL) {
                if (error_buffer && error_buffer_size > 0) {
                    snprintf(error_buffer, error_buffer_size,
                            "Target 'terminal' requires terminal renderer, got '%s'",
                            kryon_renderer_name(renderer));
                }
                return false;
            }
            break;

        default:
            if (error_buffer && error_buffer_size > 0) {
                snprintf(error_buffer, error_buffer_size, "Unknown target type");
            }
            return false;
    }

    // Check if renderer is available
    if (!is_renderer_available(renderer)) {
        if (error_buffer && error_buffer_size > 0) {
            snprintf(error_buffer, error_buffer_size,
                    "Renderer '%s' is not available in this build",
                    kryon_renderer_name(renderer));
        }
        return false;
    }

    return true;
}

// =============================================================================
// PARSING
// =============================================================================

bool kryon_target_parse(const char *name, KryonTargetType *out_target) {
    if (!name || !out_target) {
        return false;
    }

    if (strcmp(name, "native") == 0) {
        *out_target = KRYON_TARGET_NATIVE;
        return true;
    }
    else if (strcmp(name, "emu") == 0) {
        *out_target = KRYON_TARGET_EMU;
        return true;
    }
    else if (strcmp(name, "web") == 0) {
        *out_target = KRYON_TARGET_WEB;
        return true;
    }
    else if (strcmp(name, "terminal") == 0) {
        *out_target = KRYON_TARGET_TERMINAL;
        return true;
    }

    return false;
}

bool kryon_renderer_parse(const char *name, KryonRendererType *out_renderer) {
    if (!name || !out_renderer) {
        return false;
    }

    if (strcmp(name, "sdl2") == 0) {
        *out_renderer = KRYON_RENDERER_TYPE_SDL2;
        return true;
    }
    else if (strcmp(name, "raylib") == 0) {
        *out_renderer = KRYON_RENDERER_TYPE_RAYLIB;
        return true;
    }
    else if (strcmp(name, "krbview") == 0) {
        *out_renderer = KRYON_RENDERER_TYPE_KRBVIEW;
        return true;
    }
    else if (strcmp(name, "web") == 0) {
        *out_renderer = KRYON_RENDERER_TYPE_WEB;
        return true;
    }
    else if (strcmp(name, "terminal") == 0) {
        *out_renderer = KRYON_RENDERER_TYPE_TERMINAL;
        return true;
    }

    return false;
}

// =============================================================================
// STRING CONVERSION
// =============================================================================

const char* kryon_target_name(KryonTargetType target) {
    switch (target) {
        case KRYON_TARGET_NATIVE:   return "native";
        case KRYON_TARGET_EMU:      return "emu";
        case KRYON_TARGET_WEB:      return "web";
        case KRYON_TARGET_TERMINAL: return "terminal";
        default:                    return "unknown";
    }
}

const char* kryon_renderer_name(KryonRendererType renderer) {
    switch (renderer) {
        case KRYON_RENDERER_TYPE_NONE:     return "none";
        case KRYON_RENDERER_TYPE_SDL2:     return "sdl2";
        case KRYON_RENDERER_TYPE_RAYLIB:   return "raylib";
        case KRYON_RENDERER_TYPE_KRBVIEW:  return "krbview";
        case KRYON_RENDERER_TYPE_WEB:      return "web";
        case KRYON_RENDERER_TYPE_TERMINAL: return "terminal";
        default:                      return "unknown";
    }
}
