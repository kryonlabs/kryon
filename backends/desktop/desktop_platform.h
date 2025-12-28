/**
 * Desktop Platform - Platform Abstraction Layer
 *
 * This file defines the core platform abstraction layer that sits between
 * the desktop renderer API and the specific backend implementations (SDL3, raylib, etc.)
 */

#ifndef DESKTOP_PLATFORM_H
#define DESKTOP_PLATFORM_H

#include "abstract/desktop_events.h"
#include "abstract/desktop_fonts.h"
#include "abstract/desktop_text.h"
#include "abstract/desktop_effects.h"
#include "../../ir/ir_core.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct DesktopIRRenderer DesktopIRRenderer;

// Backend types (from ir_desktop_renderer.h)
typedef enum {
    DESKTOP_BACKEND_SDL3,
    DESKTOP_BACKEND_RAYLIB,
    DESKTOP_BACKEND_GLFW,
    DESKTOP_BACKEND_WIN32,
    DESKTOP_BACKEND_COCOA,
    DESKTOP_BACKEND_X11
} DesktopBackendType;

/**
 * Backend Operations Table
 *
 * Each renderer implementation (SDL3, raylib, etc.) provides a table
 * of function pointers that implement the platform operations.
 */
typedef struct DesktopRendererOps {
    // ========================================================================
    // Lifecycle Operations
    // ========================================================================

    /**
     * Initialize the renderer backend
     * @param renderer Renderer instance
     * @return true on success, false on failure
     */
    bool (*initialize)(DesktopIRRenderer* renderer);

    /**
     * Shutdown the renderer backend and free resources
     * @param renderer Renderer instance
     */
    void (*shutdown)(DesktopIRRenderer* renderer);

    // ========================================================================
    // Event Handling
    // ========================================================================

    /**
     * Poll for input events
     * @param renderer Renderer instance
     */
    void (*poll_events)(DesktopIRRenderer* renderer);

    // ========================================================================
    // Frame Rendering
    // ========================================================================

    /**
     * Begin a new frame
     * @param renderer Renderer instance
     * @return true on success, false on failure
     */
    bool (*begin_frame)(DesktopIRRenderer* renderer);

    /**
     * Render a component tree
     * @param renderer Renderer instance
     * @param root Root component to render
     * @return true on success, false on failure
     */
    bool (*render_component)(DesktopIRRenderer* renderer, IRComponent* root);

    /**
     * End the current frame and present to screen
     * @param renderer Renderer instance
     */
    void (*end_frame)(DesktopIRRenderer* renderer);

    // ========================================================================
    // Resource Management
    // ========================================================================

    /**
     * Font operations for this backend
     */
    DesktopFontOps* font_ops;

    /**
     * Visual effects operations for this backend
     */
    DesktopEffectsOps* effects_ops;

    /**
     * Text measurement callback for layout system
     */
    DesktopTextMeasureCallback text_measure;

    // ========================================================================
    // Backend-Specific Data
    // ========================================================================

    /**
     * Renderer-specific data pointer
     * SDL3:   SDL3RendererData*
     * Raylib: RaylibRendererData*
     * etc.
     */
    void* backend_data;
} DesktopRendererOps;

// ============================================================================
// Backend Registry Functions
// ============================================================================

/**
 * Register a backend implementation
 * Called automatically by backends using __attribute__((constructor))
 *
 * @param type Backend type
 * @param ops Operations table for this backend
 */
void desktop_register_backend(DesktopBackendType type, DesktopRendererOps* ops);

/**
 * Get the operations table for a backend type
 *
 * @param type Backend type
 * @return Operations table, or NULL if backend not available
 */
DesktopRendererOps* desktop_get_backend_ops(DesktopBackendType type);

// ============================================================================
// Font Registry (Shared Across Backends)
// ============================================================================

/**
 * Registered font entry
 */
typedef struct {
    char name[64];
    char path[512];
} RegisteredFont;

/**
 * Register a font by name and path
 * @param name Font name (e.g., "Roboto", "Arial")
 * @param path Font file path
 */
void desktop_register_font(const char* name, const char* path);

/**
 * Find a font path by name
 * @param name Font name
 * @return Font path, or NULL if not found
 */
const char* desktop_find_font_path(const char* name);

/**
 * Get the count of registered fonts
 * @return Number of registered fonts
 */
int desktop_get_font_registry_count(void);

/**
 * Get a registered font by index
 * @param index Font index
 * @return Font entry, or NULL if index out of bounds
 */
const RegisteredFont* desktop_get_registered_font(int index);

// ============================================================================
// Platform Utilities
// ============================================================================

/**
 * Convert an IR dimension to pixels
 * @param dimension IR dimension value
 * @param container_size Container size for percentage calculations
 * @return Dimension in pixels
 */
float desktop_dimension_to_pixels(IRDimension dimension, float container_size);

/**
 * Convert IR color to RGBA32
 * @param color IR color value
 * @return RGBA color (0xRRGGBBAA)
 */
uint32_t desktop_color_to_rgba32(IRColor color);

#ifdef __cplusplus
}
#endif

#endif // DESKTOP_PLATFORM_H
