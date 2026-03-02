/*
 * WebAssembly Platform Layer - SDL2 Stubs
 * C89 compliant
 *
 * Stub implementations for SDL2 functions when targeting WebAssembly.
 * The actual rendering will be done via WebGL in the browser.
 */

#include <SDL2/SDL.h>

/*
 * Initialize SDL (stub)
 */
int SDL_Init(Uint32 flags)
{
    /* In WebAssembly, SDL is provided by the browser */
    return 0;
}

/*
 * Quit SDL (stub)
 */
void SDL_Quit(void)
{
    /* No-op in WebAssembly */
}

/*
 * Create window (stub - returns fake window)
 */
SDL_Window* SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    /* Return non-NULL to indicate success */
    return (SDL_Window*)0x1;
}

/*
 * Destroy window (stub)
 */
void SDL_DestroyWindow(SDL_Window* window)
{
    /* No-op */
}

/*
 * Get window surface (stub - creates fake surface)
 */
SDL_Surface* SDL_GetWindowSurface(SDL_Window* window)
{
    /* TODO: Return a surface backed by WebGL canvas */
    return NULL;
}

/*
 * Update window surface (stub)
 */
int SDL_UpdateWindowSurface(SDL_Window* window)
{
    /* Surface updates are handled by WebGL */
    return 0;
}

/*
 * Pump events (stub - events come from JavaScript)
 */
void SDL_PumpEvent(void)
{
    /* Events are handled by JavaScript event handlers */
}

/*
 * Poll event (stub - returns no events)
 */
int SDL_PollEvent(SDL_Event* event)
{
    /* Events are pushed via JavaScript */
    return 0;
}

/*
 * Delay (stub - no-op in async WebAssembly)
 */
void SDL_Delay(Uint32 ms)
{
    /* Use JavaScript setTimeout instead */
}

/*
 * Get error (stub)
 */
const char* SDL_GetError(void)
{
    return "WebAssembly stub - not implemented";
}
