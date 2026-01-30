/**
 * Kryon DSL Runtime for Desktop Target
 *
 * This runtime bridges the C DSL macros to the desktop IRRenderer.
 * It provides the KRYON_RUN implementation for desktop applications.
 */

#ifndef KRYON_DSL_RUNTIME_H
#define KRYON_DSL_RUNTIME_H

#include "../../ir/include/ir_core.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run the desktop application
 * This replaces the KRYON_RUN macro implementation for desktop builds
 * Returns 0 on success, non-zero on error
 */
int kryon_run_desktop(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_DSL_RUNTIME_H
