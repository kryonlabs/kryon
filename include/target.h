/**
 * @file target.h
 * @brief Kryon Target System - Deployment target selection and validation
 *
 * This header defines the target system that maps user intent (where to deploy)
 * to concrete renderers (how to render).
 *
 * Targets describe WHERE the app runs:
 * - sdl2: Desktop GUI via SDL2
 * - raylib: Desktop GUI via Raylib
 * - web: Browser (static HTML/CSS/JS)
 *
 * @version 2.0.0
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
 * and constraints.
 */
typedef enum {
    KRYON_TARGET_SDL2,      /**< Desktop GUI via SDL2 */
    KRYON_TARGET_RAYLIB,    /**< Desktop GUI via Raylib */
    KRYON_TARGET_WEB,       /**< Static HTML/CSS/JS */
    KRYON_TARGET_COUNT      /**< Number of target types */
} KryonTargetType;

/**
 * @brief Renderer types that can be selected for a target
 */
typedef enum {
    KRYON_RENDERER_TYPE_NONE = 0,
    KRYON_RENDERER_TYPE_SDL2,
    KRYON_RENDERER_TYPE_RAYLIB,
    KRYON_RENDERER_TYPE_WEB
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
 * @param target Target type
 * @return Resolution result with selected renderer or error
 */
KryonTargetResolution kryon_target_resolve(KryonTargetType target);

/**
 * @brief Parse target name string to enum
 *
 * @param name Target name ("sdl2", "raylib", "web")
 * @param out_target Output target type
 * @return true if parsed successfully, false if unknown
 */
bool kryon_target_parse(const char *name, KryonTargetType *out_target);

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
