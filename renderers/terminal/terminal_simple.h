/**
 * Kryon Simple Terminal Renderer Backend - Public API
 *
 * Simplified terminal UI backend for Kryon UI framework.
 * Uses basic ANSI escape sequences for maximum compatibility.
 */

#ifndef KRYON_TERMINAL_SIMPLE_H
#define KRYON_TERMINAL_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>

#include "../../core/include/kryon.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Simple Terminal Renderer Creation
// ============================================================================

/**
 * Create a new simple terminal renderer instance
 * @return Pointer to renderer instance or NULL on failure
 */
kryon_renderer_t* kryon_terminal_simple_renderer_create(void);

/**
 * Destroy a simple terminal renderer instance
 * @param renderer Renderer instance to destroy
 */
void kryon_terminal_simple_renderer_destroy(kryon_renderer_t* renderer);

#ifdef __cplusplus
}
#endif

#endif // KRYON_TERMINAL_SIMPLE_H