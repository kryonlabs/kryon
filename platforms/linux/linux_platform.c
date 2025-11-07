/*
 * Linux Platform Integration Kit
 *
 * Provides Linux-specific implementations for Kryon framework
 * - SDL3 window management and input handling
 * - Filesystem-based storage
 * - System integration utilities
 */

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

// Include Kryon core header
#include "../../core/include/kryon.h"

// ============================================================================
// SDL3 Integration
// ============================================================================

#ifdef KRYON_HAS_SDL3
#include <SDL3/SDL.h>

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* surface;
    uint32_t window_id;
    bool initialized;
} kryon_linux_sdl3_state_t;

static kryon_linux_sdl3_state_t g_sdl3_state = {0};

bool kryon_linux_init_sdl3() {
    if (g_sdl3_state.initialized) {
        return true;
    }

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        printf("SDL3 initialization failed: %s\n", SDL_GetError());
        return false;
    }

    // Create window
    g_sdl3_state.window = SDL_CreateWindow("Kryon Application",
                                         800, 600,
                                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!g_sdl3_state.window) {
        printf("SDL3 window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // Create renderer
    g_sdl3_state.renderer = SDL_CreateRenderer(g_sdl3_state.window, NULL,
                                              SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_sdl3_state.renderer) {
        printf("SDL3 renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_sdl3_state.window);
        SDL_Quit();
        return false;
    }

    // Get window surface for framebuffer rendering
    g_sdl3_state.surface = SDL_GetWindowSurface(g_sdl3_state.window);

    g_sdl3_state.window_id = SDL_GetWindowID(g_sdl3_state.window);
    g_sdl3_state.initialized = true;

    printf("SDL3 initialized successfully\n");
    return true;
}

void kryon_linux_shutdown_sdl3() {
    if (!g_sdl3_state.initialized) {
        return;
    }

    if (g_sdl3_state.renderer) {
        SDL_DestroyRenderer(g_sdl3_state.renderer);
        g_sdl3_state.renderer = NULL;
    }

    if (g_sdl3_state.window) {
        SDL_DestroyWindow(g_sdl3_state.window);
        g_sdl3_state.window = NULL;
    }

    SDL_Quit();
    g_sdl3_state.initialized = false;
    printf("SDL3 shutdown complete\n");
}

SDL_Window* kryon_linux_get_sdl3_window() {
    return g_sdl3_state.window;
}

SDL_Renderer* kryon_linux_get_sdl3_renderer() {
    return g_sdl3_state.renderer;
}

SDL_Surface* kryon_linux_get_sdl3_surface() {
    return g_sdl3_state.surface;
}

bool kryon_linux_handle_sdl3_events(kryon_event_t* events, size_t* event_count) {
    if (!g_sdl3_state.initialized || !events || !event_count) {
        return false;
    }

    *event_count = 0;
    SDL_Event sdl_event;

    while (SDL_PollEvent(&sdl_event) && *event_count < 16) {
        kryon_event_t* kryon_event = &events[*event_count];

        switch (sdl_event.type) {
            case SDL_EVENT_QUIT:
                kryon_event->eventType = KRYON_EVT_CUSTOM;
                kryon_event->param = 0; // Quit event
                (*event_count)++;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (sdl_event.button.button == SDL_BUTTON_LEFT) {
                    kryon_event->eventType = KRYON_EVT_CLICK;
                    kryon_event->x = (int16_t)sdl_event.button.x;
                    kryon_event->y = (int16_t)sdl_event.button.y;
                    kryon_event->param = 1; // Left button
                    (*event_count)++;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                kryon_event->eventType = KRYON_EVT_TOUCH;
                kryon_event->x = (int16_t)sdl_event.motion.x;
                kryon_event->y = (int16_t)sdl_event.motion.y;
                kryon_event->param = 0; // Mouse ID
                (*event_count)++;
                break;

            case SDL_EVENT_KEY_DOWN:
                kryon_event->eventType = KRYON_EVT_KEY;
                kryon_event->param = (uint32_t)sdl_event.key.key;
                (*event_count)++;
                break;

            default:
                break;
        }
    }

    return *event_count > 0;
}

void kryon_linux_swap_sdl3_buffers() {
    if (g_sdl3_state.initialized && g_sdl3_state.surface) {
        // Update surface
        SDL_UpdateWindowSurface(g_sdl3_state.window);
    } else if (g_sdl3_state.initialized && g_sdl3_state.renderer) {
        // Present renderer
        SDL_RenderPresent(g_sdl3_state.renderer);
    }
}

#endif // KRYON_HAS_SDL3

// ============================================================================
// Linux Filesystem Storage Implementation
// ============================================================================

#define KRYON_LINUX_STORAGE_DIR ".kryon_storage"
#define KRYON_LINUX_MAX_KEY_LEN 256
#define KRYON_LINUX_MAX_PATH_LEN 512

static char g_storage_dir[KRYON_LINUX_MAX_PATH_LEN] = {0};

bool kryon_linux_init_storage() {
    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        home = "/tmp"; // Fallback to temp directory
    }

    // Create storage directory path
    snprintf(g_storage_dir, sizeof(g_storage_dir), "%s/%s", home, KRYON_LINUX_STORAGE_DIR);

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(g_storage_dir, &st) == -1) {
        if (mkdir(g_storage_dir, 0755) == -1) {
            printf("Failed to create storage directory: %s\n", strerror(errno));
            return false;
        }
    }

    printf("Linux storage initialized: %s\n", g_storage_dir);
    return true;
}

bool kryon_linux_storage_set(const char* key, const void* data, size_t size) {
    if (!key || !data || size == 0 || strlen(g_storage_dir) == 0) {
        return false;
    }

    if (strlen(key) > KRYON_LINUX_MAX_KEY_LEN) {
        printf("Storage key too long: %s\n", key);
        return false;
    }

    // Create file path
    char filepath[KRYON_LINUX_MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_storage_dir, key);

    // Write data to file
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        printf("Failed to open storage file for writing: %s\n", filepath);
        return false;
    }

    size_t written = fwrite(data, 1, size, file);
    fclose(file);

    return written == size;
}

bool kryon_linux_storage_get(const char* key, void* buffer, size_t* size) {
    if (!key || !buffer || !size || strlen(g_storage_dir) == 0) {
        return false;
    }

    if (strlen(key) > KRYON_LINUX_MAX_KEY_LEN) {
        return false;
    }

    // Create file path
    char filepath[KRYON_LINUX_MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_storage_dir, key);

    // Open file for reading
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        // File doesn't exist
        *size = 0;
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        return false;
    }

    size_t data_size = (size_t)file_size;

    // Check buffer size
    if (data_size > *size) {
        *size = data_size;
        fclose(file);
        return false; // Buffer too small
    }

    // Read data
    size_t read = fread(buffer, 1, data_size, file);
    fclose(file);

    if (read == data_size) {
        *size = read;
        return true;
    }

    return false;
}

bool kryon_linux_storage_has(const char* key) {
    if (!key || strlen(g_storage_dir) == 0) {
        return false;
    }

    char filepath[KRYON_LINUX_MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_storage_dir, key);

    // Check if file exists
    return access(filepath, F_OK) == 0;
}

bool kryon_linux_storage_remove(const char* key) {
    if (!key || strlen(g_storage_dir) == 0) {
        return false;
    }

    char filepath[KRYON_LINUX_MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_storage_dir, key);

    return unlink(filepath) == 0;
}

// ============================================================================
// Linux System Utilities
// ============================================================================

uint32_t kryon_linux_get_timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void kryon_linux_get_display_dimensions(uint16_t* width, uint16_t* height) {
    if (width) *width = 1920;  // Default values
    if (height) *height = 1080;

#ifdef KRYON_HAS_SDL3
    if (g_sdl3_state.window) {
        int w, h;
        SDL_GetWindowSize(g_sdl3_state.window, &w, &h);
        if (width) *width = (uint16_t)w;
        if (height) *height = (uint16_t)h;
    }
#endif
}

bool kryon_linux_open_url(const char* url) {
    if (!url) return false;

    char command[512];
    snprintf(command, sizeof(command), "xdg-open '%s'", url);

    int result = system(command);
    return result == 0;
}

// ============================================================================
// Platform Detection and Capabilities
// ============================================================================

typedef struct {
    bool has_sdl3;
    bool has_opengl;
    bool has_vulkan;
    bool has_wayland;
    bool has_x11;
    char desktop_environment[64];
} kryon_linux_capabilities_t;

kryon_linux_capabilities_t kryon_linux_get_capabilities() {
    kryon_linux_capabilities_t caps = {0};

    // Check for SDL3
#ifdef KRYON_HAS_SDL3
    caps.has_sdl3 = true;
#endif

    // Check for OpenGL
    if (access("/usr/lib/x86_64-linux-gnu/libGL.so.1", F_OK) == 0 ||
        access("/usr/lib/libGL.so.1", F_OK) == 0) {
        caps.has_opengl = true;
    }

    // Check for Vulkan
    if (access("/usr/lib/x86_64-linux-gnu/libvulkan.so.1", F_OK) == 0 ||
        access("/usr/lib/libvulkan.so.1", F_OK) == 0) {
        caps.has_vulkan = true;
    }

    // Check for Wayland
    caps.has_wayland = (getenv("WAYLAND_DISPLAY") != NULL);

    // Check for X11
    caps.has_x11 = (getenv("DISPLAY") != NULL);

    // Detect desktop environment
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if (desktop) {
        strncpy(caps.desktop_environment, desktop, sizeof(caps.desktop_environment) - 1);
    } else if (getenv("KDE_FULL_SESSION")) {
        strcpy(caps.desktop_environment, "KDE");
    } else if (getenv("GNOME_DESKTOP_SESSION_ID")) {
        strcpy(caps.desktop_environment, "GNOME");
    } else {
        strcpy(caps.desktop_environment, "Unknown");
    }

    return caps;
}

void kryon_linux_print_info() {
    printf("=== Linux Platform Information ===\n");

    kryon_linux_capabilities_t caps = kryon_linux_get_capabilities();

    printf("Desktop Environment: %s\n", caps.desktop_environment);
    printf("Display Server: ");
    if (caps.has_wayland) {
        printf("Wayland");
    } else if (caps.has_x11) {
        printf("X11");
    } else {
        printf("Unknown");
    }
    printf("\n");

    printf("Graphics Support:\n");
    printf("  SDL3: %s\n", caps.has_sdl3 ? "Yes" : "No");
    printf("  OpenGL: %s\n", caps.has_opengl ? "Yes" : "No");
    printf("  Vulkan: %s\n", caps.has_vulkan ? "Yes" : "No");

    printf("Storage Directory: %s\n", g_storage_dir);
    printf("===================================\n");
}

// ============================================================================
// Initialization and Cleanup
// ============================================================================

bool kryon_linux_platform_init() {
    printf("Initializing Linux platform...\n");

    // Initialize storage
    if (!kryon_linux_init_storage()) {
        return false;
    }

#ifdef KRYON_HAS_SDL3
    // Initialize SDL3 if available
    if (!kryon_linux_init_sdl3()) {
        printf("Warning: SDL3 initialization failed, continuing without graphics\n");
    }
#endif

    printf("Linux platform initialization complete\n");
    return true;
}

void kryon_linux_platform_shutdown() {
    printf("Shutting down Linux platform...\n");

#ifdef KRYON_HAS_SDL3
    kryon_linux_shutdown_sdl3();
#endif

    printf("Linux platform shutdown complete\n");
}