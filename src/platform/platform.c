/**
 * @file platform.c
 * @brief Platform Abstraction Layer - Main implementation
 * 
 * Cross-platform abstraction that automatically selects the appropriate
 * platform implementation at runtime based on the target system.
 */

#include "internal/platform.h"
#include "internal/memory.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// GLOBAL PLATFORM INSTANCE
// =============================================================================

static KryonPlatform *g_platform_instance = NULL;

// =============================================================================
// PLATFORM SELECTION
// =============================================================================

static const KryonPlatformInterface* select_platform_interface(void) {
#ifdef KRYON_PLATFORM_WINDOWS
    return &kryon_platform_windows_interface;
#elif defined(KRYON_PLATFORM_MACOS)
    return &kryon_platform_macos_interface;
#elif defined(KRYON_PLATFORM_LINUX)
    return &kryon_platform_linux_interface;
#elif defined(KRYON_PLATFORM_IOS)
    return &kryon_platform_ios_interface;
#elif defined(KRYON_PLATFORM_ANDROID)
    return &kryon_platform_android_interface;
#elif defined(KRYON_PLATFORM_WEB)
    return &kryon_platform_web_interface;
#else
    return NULL; // Unsupported platform
#endif
}

static KryonPlatformType get_platform_type(void) {
#if defined(KRYON_PLATFORM_WINDOWS) || defined(KRYON_PLATFORM_MACOS) || defined(KRYON_PLATFORM_LINUX)
    return KRYON_PLATFORM_TYPE_DESKTOP;
#elif defined(KRYON_PLATFORM_IOS) || defined(KRYON_PLATFORM_ANDROID)
    return KRYON_PLATFORM_TYPE_MOBILE;
#elif defined(KRYON_PLATFORM_WEB)
    return KRYON_PLATFORM_TYPE_WEB;
#else
    return KRYON_PLATFORM_TYPE_EMBEDDED;
#endif
}

// =============================================================================
// MAIN PLATFORM API
// =============================================================================

KryonPlatform *kryon_platform_init(void) {
    if (g_platform_instance) {
        return g_platform_instance; // Already initialized
    }
    
    // Select platform interface
    const KryonPlatformInterface *interface = select_platform_interface();
    if (!interface) {
        return NULL; // Unsupported platform
    }
    
    // Create platform instance
    g_platform_instance = kryon_alloc(sizeof(KryonPlatform));
    if (!g_platform_instance) {
        return NULL;
    }
    
    memset(g_platform_instance, 0, sizeof(KryonPlatform));
    
    // Initialize platform
    g_platform_instance->type = get_platform_type();
    g_platform_instance->api = *interface;
    
    // Set capabilities based on platform type
    switch (g_platform_instance->type) {
        case KRYON_PLATFORM_TYPE_DESKTOP:
            g_platform_instance->capabilities.has_window_system = true;
            g_platform_instance->capabilities.has_opengl = true;
            g_platform_instance->capabilities.has_vulkan = true;
            g_platform_instance->capabilities.has_touch_input = false;
            g_platform_instance->capabilities.has_gamepad = true;
            g_platform_instance->capabilities.has_file_system = true;
            g_platform_instance->capabilities.has_clipboard = true;
            g_platform_instance->capabilities.has_notifications = true;
            break;
            
        case KRYON_PLATFORM_TYPE_MOBILE:
            g_platform_instance->capabilities.has_window_system = true;
            g_platform_instance->capabilities.has_opengl = true;
            g_platform_instance->capabilities.has_vulkan = false; // Platform dependent
            g_platform_instance->capabilities.has_touch_input = true;
            g_platform_instance->capabilities.has_gamepad = false;
            g_platform_instance->capabilities.has_file_system = true;
            g_platform_instance->capabilities.has_clipboard = true;
            g_platform_instance->capabilities.has_notifications = true;
            break;
            
        case KRYON_PLATFORM_TYPE_WEB:
            g_platform_instance->capabilities.has_window_system = false; // Browser handles this
            g_platform_instance->capabilities.has_opengl = true; // WebGL
            g_platform_instance->capabilities.has_vulkan = false;
            g_platform_instance->capabilities.has_touch_input = true;
            g_platform_instance->capabilities.has_gamepad = true;
            g_platform_instance->capabilities.has_file_system = false; // Limited access
            g_platform_instance->capabilities.has_clipboard = true;
            g_platform_instance->capabilities.has_notifications = true;
            break;
            
        case KRYON_PLATFORM_TYPE_EMBEDDED:
            g_platform_instance->capabilities.has_window_system = false;
            g_platform_instance->capabilities.has_opengl = false;
            g_platform_instance->capabilities.has_vulkan = false;
            g_platform_instance->capabilities.has_touch_input = false;
            g_platform_instance->capabilities.has_gamepad = false;
            g_platform_instance->capabilities.has_file_system = true;
            g_platform_instance->capabilities.has_clipboard = false;
            g_platform_instance->capabilities.has_notifications = false;
            break;
    }
    
    // Initialize platform-specific implementation
    if (!g_platform_instance->api.init()) {
        kryon_free(g_platform_instance);
        g_platform_instance = NULL;
        return NULL;
    }
    
    return g_platform_instance;
}

void kryon_platform_shutdown(KryonPlatform *platform) {
    if (!platform || platform != g_platform_instance) {
        return;
    }
    
    // Destroy all windows
    for (size_t i = 0; i < platform->window_count; i++) {
        if (platform->windows[i]) {
            platform->api.window_destroy(platform->windows[i]);
        }
    }
    
    kryon_free(platform->windows);
    
    // Shutdown platform implementation
    if (platform->api.shutdown) {
        platform->api.shutdown();
    }
    
    kryon_free(platform);
    g_platform_instance = NULL;
}

KryonPlatformType kryon_platform_get_type(const KryonPlatform *platform) {
    if (!platform) {
        return KRYON_PLATFORM_TYPE_EMBEDDED;
    }
    
    return platform->type;
}

bool kryon_platform_has_capability(const KryonPlatform *platform, const char *capability) {
    if (!platform || !capability) {
        return false;
    }
    
    if (strcmp(capability, "window_system") == 0) {
        return platform->capabilities.has_window_system;
    } else if (strcmp(capability, "opengl") == 0) {
        return platform->capabilities.has_opengl;
    } else if (strcmp(capability, "vulkan") == 0) {
        return platform->capabilities.has_vulkan;
    } else if (strcmp(capability, "touch_input") == 0) {
        return platform->capabilities.has_touch_input;
    } else if (strcmp(capability, "gamepad") == 0) {
        return platform->capabilities.has_gamepad;
    } else if (strcmp(capability, "file_system") == 0) {
        return platform->capabilities.has_file_system;
    } else if (strcmp(capability, "clipboard") == 0) {
        return platform->capabilities.has_clipboard;
    } else if (strcmp(capability, "notifications") == 0) {
        return platform->capabilities.has_notifications;
    }
    
    return false;
}

// =============================================================================
// WINDOW MANAGEMENT
// =============================================================================

KryonWindow *kryon_platform_create_window(KryonPlatform *platform, 
                                         const KryonWindowConfig *config) {
    if (!platform || !config) {
        return NULL;
    }
    
    KryonWindow *window = platform->api.window_create(config);
    if (!window) {
        return NULL;
    }
    
    // Add to window list
    if (platform->window_count >= platform->window_capacity) {
        size_t new_capacity = platform->window_capacity == 0 ? 4 : platform->window_capacity * 2;
        KryonWindow **new_windows = kryon_alloc(new_capacity * sizeof(KryonWindow*));
        if (!new_windows) {
            platform->api.window_destroy(window);
            return NULL;
        }
        
        if (platform->windows) {
            memcpy(new_windows, platform->windows, platform->window_count * sizeof(KryonWindow*));
            kryon_free(platform->windows);
        }
        
        platform->windows = new_windows;
        platform->window_capacity = new_capacity;
    }
    
    platform->windows[platform->window_count++] = window;
    return window;
}

void kryon_platform_destroy_window(KryonPlatform *platform, KryonWindow *window) {
    if (!platform || !window) {
        return;
    }
    
    // Remove from window list
    for (size_t i = 0; i < platform->window_count; i++) {
        if (platform->windows[i] == window) {
            // Shift remaining windows
            for (size_t j = i; j < platform->window_count - 1; j++) {
                platform->windows[j] = platform->windows[j + 1];
            }
            platform->window_count--;
            break;
        }
    }
    
    platform->api.window_destroy(window);
}

KryonWindowConfig kryon_platform_default_window_config(void) {
    KryonWindowConfig config;
    memset(&config, 0, sizeof(config));
    
    config.title = "Kryon Application";
    config.width = 800;
    config.height = 600;
    config.x = -1; // Center
    config.y = -1; // Center
    config.mode = KRYON_WINDOW_WINDOWED;
    config.resizable = true;
    config.visible = true;
    config.decorated = true;
    config.always_on_top = false;
    config.vsync = true;
    config.min_width = 0;
    config.min_height = 0;
    config.max_width = 0;
    config.max_height = 0;
    config.icon_path = NULL;
    
    return config;
}

// =============================================================================
// EVENT HANDLING
// =============================================================================

void kryon_platform_set_event_callback(KryonPlatform *platform, 
                                      void (*callback)(const KryonEvent *event, void *user_data),
                                      void *user_data) {
    if (!platform) {
        return;
    }
    
    platform->event_callback = callback;
    platform->event_user_data = user_data;
}

void kryon_platform_poll_events(KryonPlatform *platform) {
    if (!platform) {
        return;
    }
    
    // Update input state first
    if (platform->api.get_input_state) {
        platform->api.get_input_state(&platform->input_state);
    }
    
    // Poll platform events
    if (platform->api.poll_events) {
        platform->api.poll_events();
    }
}

void kryon_platform_wait_events(KryonPlatform *platform) {
    if (!platform) {
        return;
    }
    
    if (platform->api.wait_events) {
        platform->api.wait_events();
    }
}

// =============================================================================
// INPUT HANDLING
// =============================================================================

const KryonInputState *kryon_platform_get_input_state(KryonPlatform *platform) {
    if (!platform) {
        return NULL;
    }
    
    return &platform->input_state;
}

bool kryon_platform_is_key_pressed(KryonPlatform *platform, KryonKeyCode key) {
    if (!platform || key >= 256) {
        return false;
    }
    
    return platform->input_state.keys[key];
}

bool kryon_platform_is_mouse_pressed(KryonPlatform *platform, KryonMouseButton button) {
    if (!platform || button >= 8) {
        return false;
    }
    
    return platform->input_state.mouse_buttons[button];
}

void kryon_platform_get_mouse_position(KryonPlatform *platform, float *x, float *y) {
    if (!platform) {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        return;
    }
    
    if (x) *x = platform->input_state.mouse_x;
    if (y) *y = platform->input_state.mouse_y;
}

// =============================================================================
// SYSTEM UTILITIES
// =============================================================================

double kryon_platform_get_time(KryonPlatform *platform) {
    if (!platform || !platform->api.get_time) {
        return 0.0;
    }
    
    return platform->api.get_time();
}

void kryon_platform_sleep(KryonPlatform *platform, double seconds) {
    if (!platform || !platform->api.sleep) {
        return;
    }
    
    platform->api.sleep(seconds);
}

bool kryon_platform_get_display_info(KryonPlatform *platform, int display_index, 
                                    KryonDisplayInfo *info) {
    if (!platform || !info || !platform->api.get_display_info) {
        return false;
    }
    
    int display_count = platform->api.get_display_count ? platform->api.get_display_count() : 1;
    if (display_index < 0 || display_index >= display_count) {
        return false;
    }
    
    platform->api.get_display_info(display_index, info);
    return true;
}

// =============================================================================
// FILE SYSTEM
// =============================================================================

char *kryon_platform_show_file_dialog(KryonPlatform *platform, 
                                     const KryonFileDialog *config) {
    if (!platform || !config || !platform->api.show_file_dialog) {
        return NULL;
    }
    
    return platform->api.show_file_dialog(config);
}

bool kryon_platform_file_exists(KryonPlatform *platform, const char *path) {
    if (!platform || !path || !platform->api.file_exists) {
        return false;
    }
    
    return platform->api.file_exists(path);
}

char *kryon_platform_get_app_data_path(KryonPlatform *platform) {
    if (!platform || !platform->api.get_app_data_path) {
        return NULL;
    }
    
    return platform->api.get_app_data_path();
}

// =============================================================================
// CLIPBOARD
// =============================================================================

void kryon_platform_set_clipboard_text(KryonPlatform *platform, const char *text) {
    if (!platform || !text || !platform->api.set_clipboard_text) {
        return;
    }
    
    platform->api.set_clipboard_text(text);
}

char *kryon_platform_get_clipboard_text(KryonPlatform *platform) {
    if (!platform || !platform->api.get_clipboard_text) {
        return NULL;
    }
    
    const char *text = platform->api.get_clipboard_text();
    if (!text) {
        return NULL;
    }
    
    // Return a copy that the caller owns
    size_t len = strlen(text) + 1;
    char *copy = kryon_alloc(len);
    if (copy) {
        memcpy(copy, text, len);
    }
    
    return copy;
}