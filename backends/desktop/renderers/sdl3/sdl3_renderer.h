/**
 * SDL3 Renderer - SDL3-Specific Renderer Implementation
 *
 * This file defines the SDL3 backend implementation for the desktop renderer.
 * It implements the DesktopRendererOps interface for SDL3.
 */

#ifndef SDL3_RENDERER_H
#define SDL3_RENDERER_H

#include "../../desktop_platform.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <kryon.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SDL3-specific renderer data
 * Stored in DesktopIRRenderer->ops->backend_data
 */
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* default_font;
    SDL_Cursor* cursor_default;
    SDL_Cursor* cursor_hand;
    SDL_Cursor* current_cursor;
    SDL_Texture* white_texture;  // 1x1 white texture for vertex coloring
    bool blend_mode_set;         // Track if blend mode already set this frame

    // Kryon renderer wrapper for command buffer execution
    kryon_renderer_t* kryon_renderer;
} SDL3RendererData;

/**
 * Register SDL3 backend (call once at startup before creating renderer)
 */
void sdl3_backend_register(void);

/**
 * Get the SDL3 operations table
 * Called by desktop_register_backend() during registration
 *
 * @return Operations table for SDL3 renderer
 */
DesktopRendererOps* desktop_sdl3_get_ops(void);

/**
 * Get SDL3 renderer data from generic renderer
 * Helper for accessing SDL3-specific fields
 */
SDL3RendererData* sdl3_get_data(DesktopIRRenderer* renderer);

#ifdef __cplusplus
}
#endif

#endif // SDL3_RENDERER_H
