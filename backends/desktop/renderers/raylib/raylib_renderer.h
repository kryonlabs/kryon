/**
 * Raylib Renderer Backend Header
 *
 * Provides raylib renderer backend operations for the desktop platform.
 */

#ifndef RAYLIB_RENDERER_H
#define RAYLIB_RENDERER_H

#include "../../desktop_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get raylib renderer operations table
 */
DesktopRendererOps* desktop_raylib_get_ops(void);

#ifdef __cplusplus
}
#endif

#endif // RAYLIB_RENDERER_H
