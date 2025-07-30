/**
 * @file platform.h
 * @brief Kryon Platform Abstraction Layer
 * 
 * Cross-platform abstraction for window management, input handling, and
 * system integration across Windows, macOS, Linux, iOS, and Android.
 * 
 * @version 1.0.0  
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_PLATFORM_H
#define KRYON_INTERNAL_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "internal/runtime.h"

// =============================================================================
// PLATFORM DETECTION
// =============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define KRYON_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define KRYON_PLATFORM_IOS 1
    #else
        #define KRYON_PLATFORM_MACOS 1
    #endif
#elif defined(__ANDROID__)
    #define KRYON_PLATFORM_ANDROID 1
#elif defined(__linux__)
    #define KRYON_PLATFORM_LINUX 1
#elif defined(__EMSCRIPTEN__)
    #define KRYON_PLATFORM_WEB 1
#else
    #define KRYON_PLATFORM_UNKNOWN 1
#endif

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonPlatform KryonPlatform;
typedef struct KryonWindow KryonWindow;
typedef struct KryonWindowConfig KryonWindowConfig;
typedef struct KryonInputState KryonInputState;
typedef struct KryonDisplayInfo KryonDisplayInfo;
typedef struct KryonFileDialog KryonFileDialog;

// =============================================================================
// ENUMERATIONS
// =============================================================================

/**
 * @brief Platform types
 */
typedef enum {
    KRYON_PLATFORM_TYPE_DESKTOP = 0,
    KRYON_PLATFORM_TYPE_MOBILE,
    KRYON_PLATFORM_TYPE_WEB,
    KRYON_PLATFORM_TYPE_EMBEDDED
} KryonPlatformType;

/**
 * @brief Window modes
 */
typedef enum {
    KRYON_WINDOW_WINDOWED = 0,
    KRYON_WINDOW_FULLSCREEN,
    KRYON_WINDOW_BORDERLESS,
    KRYON_WINDOW_MAXIMIZED,
    KRYON_WINDOW_MINIMIZED
} KryonWindowMode;

/**
 * @brief Key codes (based on USB HID usage codes)
 */
typedef enum {
    KRYON_KEY_UNKNOWN = 0,
    
    // Letters
    KRYON_KEY_A = 4, KRYON_KEY_B, KRYON_KEY_C, KRYON_KEY_D, KRYON_KEY_E,
    KRYON_KEY_F, KRYON_KEY_G, KRYON_KEY_H, KRYON_KEY_I, KRYON_KEY_J,
    KRYON_KEY_K, KRYON_KEY_L, KRYON_KEY_M, KRYON_KEY_N, KRYON_KEY_O,
    KRYON_KEY_P, KRYON_KEY_Q, KRYON_KEY_R, KRYON_KEY_S, KRYON_KEY_T,
    KRYON_KEY_U, KRYON_KEY_V, KRYON_KEY_W, KRYON_KEY_X, KRYON_KEY_Y, KRYON_KEY_Z,
    
    // Numbers
    KRYON_KEY_1 = 30, KRYON_KEY_2, KRYON_KEY_3, KRYON_KEY_4, KRYON_KEY_5,
    KRYON_KEY_6, KRYON_KEY_7, KRYON_KEY_8, KRYON_KEY_9, KRYON_KEY_0,
    
    // Control keys
    KRYON_KEY_RETURN = 40,
    KRYON_KEY_ESCAPE = 41,
    KRYON_KEY_BACKSPACE = 42,
    KRYON_KEY_TAB = 43,
    KRYON_KEY_SPACE = 44,
    
    // Modifier keys
    KRYON_KEY_LCTRL = 224,
    KRYON_KEY_LSHIFT = 225,
    KRYON_KEY_LALT = 226,
    KRYON_KEY_LGUI = 227,
    KRYON_KEY_RCTRL = 228,
    KRYON_KEY_RSHIFT = 229,
    KRYON_KEY_RALT = 230,
    KRYON_KEY_RGUI = 231,
    
    // Arrow keys
    KRYON_KEY_RIGHT = 79,
    KRYON_KEY_LEFT = 80,
    KRYON_KEY_DOWN = 81,
    KRYON_KEY_UP = 82,
    
    // Function keys
    KRYON_KEY_F1 = 58, KRYON_KEY_F2, KRYON_KEY_F3, KRYON_KEY_F4,
    KRYON_KEY_F5, KRYON_KEY_F6, KRYON_KEY_F7, KRYON_KEY_F8,
    KRYON_KEY_F9, KRYON_KEY_F10, KRYON_KEY_F11, KRYON_KEY_F12
} KryonKeyCode;

/**
 * @brief Mouse buttons
 */
typedef enum {
    KRYON_MOUSE_LEFT = 0,
    KRYON_MOUSE_RIGHT = 1,
    KRYON_MOUSE_MIDDLE = 2,
    KRYON_MOUSE_X1 = 3,
    KRYON_MOUSE_X2 = 4
} KryonMouseButton;

/**
 * @brief Input modifiers
 */
typedef enum {
    KRYON_MOD_NONE = 0,
    KRYON_MOD_SHIFT = 1 << 0,
    KRYON_MOD_CTRL = 1 << 1,
    KRYON_MOD_ALT = 1 << 2,
    KRYON_MOD_GUI = 1 << 3  // Windows/Cmd key
} KryonInputModifiers;

// =============================================================================
// STRUCTURES
// =============================================================================

/**
 * @brief Window configuration
 */
struct KryonWindowConfig {
    const char *title;               // Window title
    int width;                       // Window width
    int height;                      // Window height
    int x;                          // Window X position (-1 for center)
    int y;                          // Window Y position (-1 for center)
    KryonWindowMode mode;           // Window mode
    bool resizable;                 // Is window resizable?
    bool visible;                   // Is window initially visible?
    bool decorated;                 // Has title bar and borders?
    bool always_on_top;             // Always on top?
    bool vsync;                     // Enable vertical sync?
    int min_width;                  // Minimum width (0 for no limit)
    int min_height;                 // Minimum height (0 for no limit)
    int max_width;                  // Maximum width (0 for no limit)
    int max_height;                 // Maximum height (0 for no limit)
    const char *icon_path;          // Path to window icon
};

/**
 * @brief Display information
 */
struct KryonDisplayInfo {
    int width;                      // Display width in pixels
    int height;                     // Display height in pixels
    int refresh_rate;               // Refresh rate in Hz
    float dpi_scale;                // DPI scaling factor
    const char *name;               // Display name
    bool is_primary;                // Is primary display?
};

/**
 * @brief Input state
 */
struct KryonInputState {
    // Mouse state
    float mouse_x;                  // Mouse X position
    float mouse_y;                  // Mouse Y position
    float mouse_delta_x;            // Mouse movement delta X
    float mouse_delta_y;            // Mouse movement delta Y
    float scroll_x;                 // Scroll wheel X delta
    float scroll_y;                 // Scroll wheel Y delta
    bool mouse_buttons[8];          // Mouse button states
    
    // Keyboard state
    bool keys[256];                 // Key states (indexed by KryonKeyCode)
    KryonInputModifiers modifiers;  // Current modifier keys
    
    // Touch state (mobile)
    struct {
        bool active;                // Is touch active?
        float x, y;                 // Touch position
        uint32_t id;                // Touch ID
    } touches[10];                  // Up to 10 simultaneous touches
    
    // Gamepad state
    struct {
        bool connected;             // Is gamepad connected?
        float axes[6];              // Analog axes (-1.0 to 1.0)
        bool buttons[16];           // Button states
    } gamepad;
};

/**
 * @brief File dialog configuration
 */
struct KryonFileDialog {
    const char *title;              // Dialog title
    const char *default_path;       // Default directory
    const char **filters;           // File type filters
    size_t filter_count;            // Number of filters
    bool multiple_selection;        // Allow multiple files?
    bool save_dialog;               // Is save dialog (vs open)?
};

// =============================================================================
// PLATFORM INTERFACE
// =============================================================================

/**
 * @brief Platform interface - function pointers for platform-specific implementations
 */
typedef struct {
    // Initialization
    bool (*init)(void);
    void (*shutdown)(void);
    
    // Window management
    KryonWindow* (*window_create)(const KryonWindowConfig *config);
    void (*window_destroy)(KryonWindow *window);
    void (*window_show)(KryonWindow *window);
    void (*window_hide)(KryonWindow *window);
    void (*window_set_title)(KryonWindow *window, const char *title);
    void (*window_set_size)(KryonWindow *window, int width, int height);
    void (*window_set_position)(KryonWindow *window, int x, int y);
    void (*window_get_size)(KryonWindow *window, int *width, int *height);
    void (*window_get_position)(KryonWindow *window, int *x, int *y);
    void (*window_get_framebuffer_size)(KryonWindow *window, int *width, int *height);
    bool (*window_should_close)(KryonWindow *window);
    void (*window_swap_buffers)(KryonWindow *window);
    
    // Event handling
    void (*poll_events)(void);
    void (*wait_events)(void);
    void (*post_empty_event)(void);
    
    // Input
    void (*get_input_state)(KryonInputState *state);
    void (*set_cursor_mode)(KryonWindow *window, int mode);
    void (*set_clipboard_text)(const char *text);
    const char* (*get_clipboard_text)(void);
    
    // System
    double (*get_time)(void);
    void (*sleep)(double seconds);
    int (*get_display_count)(void);
    void (*get_display_info)(int display_index, KryonDisplayInfo *info);
    
    // File system
    char* (*show_file_dialog)(const KryonFileDialog *config);
    bool (*file_exists)(const char *path);
    char* (*get_app_data_path)(void);
    char* (*get_user_documents_path)(void);
    
    // Graphics context
    void* (*create_graphics_context)(KryonWindow *window);
    void (*destroy_graphics_context)(void *context);
    void (*make_context_current)(void *context);
    
} KryonPlatformInterface;

/**
 * @brief Main platform structure
 */
struct KryonPlatform {
    KryonPlatformType type;         // Platform type
    KryonPlatformInterface api;     // Platform API implementation
    void *platform_data;           // Platform-specific data
    
    // Capabilities
    struct {
        bool has_window_system;     // Has windowing system?
        bool has_opengl;            // Has OpenGL support?
        bool has_vulkan;            // Has Vulkan support?
        bool has_touch_input;       // Has touch input?
        bool has_gamepad;           // Has gamepad support?
        bool has_file_system;       // Has file system access?
        bool has_clipboard;         // Has clipboard access?
        bool has_notifications;     // Has system notifications?
    } capabilities;
    
    // Runtime state
    KryonWindow **windows;          // Array of created windows
    size_t window_count;            // Number of windows
    size_t window_capacity;         // Window array capacity
    KryonInputState input_state;    // Current input state
    
    // Event callbacks
    void (*event_callback)(const KryonEvent *event, void *user_data);
    void *event_user_data;
};

// =============================================================================
// MAIN PLATFORM API
// =============================================================================

/**
 * @brief Initialize platform system
 * @return Platform instance or NULL on failure
 */
KryonPlatform *kryon_platform_init(void);

/**
 * @brief Shutdown platform system
 * @param platform Platform instance
 */
void kryon_platform_shutdown(KryonPlatform *platform);

/**
 * @brief Get platform type
 * @param platform Platform instance
 * @return Platform type
 */
KryonPlatformType kryon_platform_get_type(const KryonPlatform *platform);

/**
 * @brief Check platform capability
 * @param platform Platform instance
 * @param capability Capability name (string)
 * @return true if supported
 */
bool kryon_platform_has_capability(const KryonPlatform *platform, const char *capability);

// =============================================================================
// WINDOW MANAGEMENT
// =============================================================================

/**
 * @brief Create window
 * @param platform Platform instance
 * @param config Window configuration
 * @return Window handle or NULL on failure
 */
KryonWindow *kryon_platform_create_window(KryonPlatform *platform, 
                                         const KryonWindowConfig *config);

/**
 * @brief Destroy window
 * @param platform Platform instance  
 * @param window Window to destroy
 */
void kryon_platform_destroy_window(KryonPlatform *platform, KryonWindow *window);

/**
 * @brief Get default window configuration
 * @return Default window configuration
 */
KryonWindowConfig kryon_platform_default_window_config(void);

// =============================================================================
// EVENT HANDLING
// =============================================================================

/**
 * @brief Set event callback
 * @param platform Platform instance
 * @param callback Event callback function
 * @param user_data User data for callback
 */
void kryon_platform_set_event_callback(KryonPlatform *platform, 
                                      void (*callback)(const KryonEvent *event, void *user_data),
                                      void *user_data);

/**
 * @brief Poll for events
 * @param platform Platform instance
 */
void kryon_platform_poll_events(KryonPlatform *platform);

/**
 * @brief Wait for events
 * @param platform Platform instance
 */
void kryon_platform_wait_events(KryonPlatform *platform);

// =============================================================================
// INPUT HANDLING
// =============================================================================

/**
 * @brief Get current input state
 * @param platform Platform instance
 * @return Pointer to input state
 */
const KryonInputState *kryon_platform_get_input_state(KryonPlatform *platform);

/**
 * @brief Check if key is pressed
 * @param platform Platform instance
 * @param key Key code
 * @return true if pressed
 */
bool kryon_platform_is_key_pressed(KryonPlatform *platform, KryonKeyCode key);

/**
 * @brief Check if mouse button is pressed
 * @param platform Platform instance
 * @param button Mouse button
 * @return true if pressed
 */
bool kryon_platform_is_mouse_pressed(KryonPlatform *platform, KryonMouseButton button);

/**
 * @brief Get mouse position
 * @param platform Platform instance
 * @param x Output for X position
 * @param y Output for Y position
 */
void kryon_platform_get_mouse_position(KryonPlatform *platform, float *x, float *y);

// =============================================================================
// SYSTEM UTILITIES
// =============================================================================

/**
 * @brief Get current time in seconds
 * @param platform Platform instance
 * @return Time in seconds since platform init
 */
double kryon_platform_get_time(KryonPlatform *platform);

/**
 * @brief Sleep for given duration
 * @param platform Platform instance
 * @param seconds Sleep duration in seconds
 */
void kryon_platform_sleep(KryonPlatform *platform, double seconds);

/**
 * @brief Get display information
 * @param platform Platform instance
 * @param display_index Display index (0 for primary)
 * @param info Output for display info
 * @return true if display exists
 */
bool kryon_platform_get_display_info(KryonPlatform *platform, int display_index, 
                                    KryonDisplayInfo *info);

// =============================================================================
// FILE SYSTEM
// =============================================================================

/**
 * @brief Show file dialog
 * @param platform Platform instance
 * @param config Dialog configuration
 * @return Selected file path (caller must free) or NULL
 */
char *kryon_platform_show_file_dialog(KryonPlatform *platform, 
                                     const KryonFileDialog *config);

/**
 * @brief Check if file exists
 * @param platform Platform instance
 * @param path File path
 * @return true if file exists
 */
bool kryon_platform_file_exists(KryonPlatform *platform, const char *path);

/**
 * @brief Get application data directory
 * @param platform Platform instance
 * @return Application data path (caller must free)
 */
char *kryon_platform_get_app_data_path(KryonPlatform *platform);

// =============================================================================
// CLIPBOARD
// =============================================================================

/**
 * @brief Set clipboard text
 * @param platform Platform instance
 * @param text Text to set
 */
void kryon_platform_set_clipboard_text(KryonPlatform *platform, const char *text);

/**
 * @brief Get clipboard text
 * @param platform Platform instance
 * @return Clipboard text (caller must free) or NULL
 */
char *kryon_platform_get_clipboard_text(KryonPlatform *platform);

// =============================================================================
// PLATFORM-SPECIFIC IMPLEMENTATIONS
// =============================================================================

#ifdef KRYON_PLATFORM_WINDOWS
extern const KryonPlatformInterface kryon_platform_windows_interface;
#endif

#ifdef KRYON_PLATFORM_MACOS
extern const KryonPlatformInterface kryon_platform_macos_interface;
#endif

#ifdef KRYON_PLATFORM_LINUX
extern const KryonPlatformInterface kryon_platform_linux_interface;
#endif

#ifdef KRYON_PLATFORM_IOS
extern const KryonPlatformInterface kryon_platform_ios_interface;
#endif

#ifdef KRYON_PLATFORM_ANDROID
extern const KryonPlatformInterface kryon_platform_android_interface;
#endif

#ifdef KRYON_PLATFORM_WEB
extern const KryonPlatformInterface kryon_platform_web_interface;
#endif

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_PLATFORM_H