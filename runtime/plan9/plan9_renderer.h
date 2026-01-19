/**
 * Plan 9 Backend - Public API
 *
 * Public interface for the Plan 9/9front rendering backend
 * Implements DesktopRendererOps for use with Kryon desktop renderer
 */

#ifndef PLAN9_RENDERER_H
#define PLAN9_RENDERER_H

#include "../desktop/desktop_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get Plan 9 backend operations table
 * @return Pointer to DesktopRendererOps structure
 */
DesktopRendererOps* desktop_plan9_get_ops(void);

/**
 * Register Plan 9 backend with desktop renderer system
 * Call this during renderer initialization to make Plan 9 backend available
 */
void plan9_backend_register(void);

#ifdef __cplusplus
}
#endif

#endif /* PLAN9_RENDERER_H */
