/**
 * @file krbview_renderer.h
 * @brief KRBView Renderer - TaijiOS emu integration
 *
 * This renderer integrates with the TaijiOS krbview module for rendering
 * Kryon applications within the Inferno emu environment.
 *
 * @note This is currently a stub implementation. Full integration requires
 *       investigation of the TaijiOS krbview module API.
 */

#ifndef KRYON_KRBVIEW_RENDERER_H
#define KRYON_KRBVIEW_RENDERER_H

#ifdef HAVE_RENDERER_KRBVIEW

#include "renderer_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a krbview renderer instance
 *
 * @param config Renderer configuration
 * @return Renderer instance or NULL on failure
 */
KryonRenderer* kryon_krbview_renderer_create(const KryonRendererConfig* config);

/**
 * @brief Check if krbview is available
 *
 * @return true if running in TaijiOS emu with krbview support
 */
bool kryon_krbview_is_available(void);

#ifdef __cplusplus
}
#endif

#endif // HAVE_RENDERER_KRBVIEW

#endif // KRYON_KRBVIEW_RENDERER_H
