/**
 * @file target.h
 * @brief Kryon Target System - Deployment target selection and validation
 *
 * This header defines the target system that maps user intent (where to deploy)
 * to concrete renderers (how to render). This provides a more intuitive interface
 * than directly selecting renderers.
 *
 * Targets describe WHERE the app runs:
 * - native: Desktop GUI application (SDL2/Raylib)
 * - emu: TaijiOS emu environment (krbview)
 * - web: Browser (static HTML/CSS/JS)
 * - terminal: Text-only output
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_TARGET_H
#define KRYON_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// TARGET TYPES
// =============================================================================

/**
 * @brief Deployment targets for Kryon applications
 *
 * Each target represents a deployment environment with specific capabilities
 * and constraints. The target system automatically selects appropriate renderers.
 */
typedef enum {
    KRYON_TARGET_NATIVE,    /**< Desktop GUI (SDL2/Raylib) */
    KRYON_TARGET_EMU,       /**< TaijiOS emu (krbview renderer) */
    KRYON_TARGET_WEB,       /**< Static HTML/CSS/JS */
    KRYON_TARGET_TERMINAL,  /**< Text output only */
    KRYON_TARGET_COUNT      /**< Number of target types */
} KryonTargetType;

/**
 * @brief Renderer types that can be selected for a target
 * Note: Enum names avoid conflict with build flags (KRYON_RENDERER_SDL2=1)
 */
typedef enum {
    KRYON_RENDERER_TYPE_NONE = 0,
    KRYON_RENDERER_TYPE_SDL2,
    KRYON_RENDERER_TYPE_RAYLIB,
    KRYON_RENDERER_TYPE_KRBVIEW,
    KRYON_RENDERER_TYPE_WEB,
    KRYON_RENDERER_TYPE_TERMINAL
} KryonRendererType;

/**
 * @brief Result of target resolution
 */
typedef struct {
    KryonTargetType target;
    KryonRendererType renderer;
    bool available;
    const char *error_message;  // Set if !available
} KryonTargetResolution;

// =============================================================================
// TARGET RESOLUTION API
// =============================================================================

/**
 * @brief Check if a target is available in this build
 *
 * This checks both:
 * 1. Build-time availability (was it compiled in?)
 * 2. Runtime availability (are dependencies present?)
 *
 * @param target Target type to check
 * @return true if target can be used, false otherwise
 */
bool kryon_target_is_available(KryonTargetType target);

/**
 * @brief Resolve target to concrete renderer
 *
 * Maps a target to the best available renderer for that target.
 * Uses fallback logic (e.g., native tries SDL2 first, then Raylib).
 *
 * @param target Target type
 * @param preferred_renderer Optional preferred renderer (KRYON_RENDERER_NONE for auto)
 * @return Resolution result with selected renderer or error
 */
KryonTargetResolution kryon_target_resolve(
    KryonTargetType target,
    KryonRendererType preferred_renderer
);

/**
 * @brief Validate target+renderer combination
 *
 * Performs hard validation of compatibility. Some combinations are invalid
 * (e.g., emu target with SDL2 renderer).
 *
 * @param target Target type
 * @param renderer Renderer type
 * @param error_buffer Buffer for error message (can be NULL)
 * @param error_buffer_size Size of error buffer
 * @return true if combination is valid, false otherwise
 */
bool kryon_target_validate(
    KryonTargetType target,
    KryonRendererType renderer,
    char *error_buffer,
    size_t error_buffer_size
);

/**
 * @brief Parse target name string to enum
 *
 * @param name Target name ("native", "emu", "web", "terminal")
 * @param out_target Output target type
 * @return true if parsed successfully, false if unknown
 */
bool kryon_target_parse(const char *name, KryonTargetType *out_target);

/**
 * @brief Parse renderer name string to enum
 *
 * @param name Renderer name ("sdl2", "raylib", "krbview", "web", "terminal")
 * @param out_renderer Output renderer type
 * @return true if parsed successfully, false if unknown
 */
bool kryon_renderer_parse(const char *name, KryonRendererType *out_renderer);

/**
 * @brief Get target name string
 *
 * @param target Target type
 * @return Human-readable target name
 */
const char* kryon_target_name(KryonTargetType target);

/**
 * @brief Get renderer name string
 *
 * @param renderer Renderer type
 * @return Human-readable renderer name
 */
const char* kryon_renderer_name(KryonRendererType renderer);

#ifdef __cplusplus
}
#endif

#endif // KRYON_TARGET_H
