/*
 * Linux Platform Integration Kit - Header
 *
 * Public interface for Linux-specific Kryon functionality
 */

#ifndef KRYON_LINUX_PLATFORM_H
#define KRYON_LINUX_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SDL3 Integration
// ============================================================================

#ifdef KRYON_HAS_SDL3
#include <SDL3/SDL.h>

// SDL3 initialization and management
bool kryon_linux_init_sdl3(void);
void kryon_linux_shutdown_sdl3(void);

// SDL3 access functions
SDL_Window* kryon_linux_get_sdl3_window(void);
SDL_Renderer* kryon_linux_get_sdl3_renderer(void);
SDL_Surface* kryon_linux_get_sdl3_surface(void);

// Event handling
bool kryon_linux_handle_sdl3_events(void* events, size_t* event_count);
void kryon_linux_swap_sdl3_buffers(void);

#endif // KRYON_HAS_SDL3

// ============================================================================
// Storage Implementation
// ============================================================================

bool kryon_linux_init_storage(void);

bool kryon_linux_storage_set(const char* key, const void* data, size_t size);
bool kryon_linux_storage_get(const char* key, void* buffer, size_t* size);
bool kryon_linux_storage_has(const char* key);
bool kryon_linux_storage_remove(const char* key);

// ============================================================================
// System Utilities
// ============================================================================

uint32_t kryon_linux_get_timestamp(void);
void kryon_linux_get_display_dimensions(uint16_t* width, uint16_t* height);
bool kryon_linux_open_url(const char* url);

// ============================================================================
// Platform Capabilities
// ============================================================================

typedef struct {
    bool has_sdl3;
    bool has_opengl;
    bool has_vulkan;
    bool has_wayland;
    bool has_x11;
    char desktop_environment[64];
} kryon_linux_capabilities_t;

kryon_linux_capabilities_t kryon_linux_get_capabilities(void);
void kryon_linux_print_info(void);

// ============================================================================
// Platform Lifecycle
// ============================================================================

bool kryon_linux_platform_init(void);
void kryon_linux_platform_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_LINUX_PLATFORM_H