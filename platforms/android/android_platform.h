#ifndef KRYON_ANDROID_PLATFORM_H
#define KRYON_ANDROID_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __ANDROID__
#include <jni.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/log.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Logging Support
// ============================================================================

#ifdef __ANDROID__
#define KRYON_ANDROID_LOG_TAG "KryonUI"
#define KRYON_ANDROID_LOG_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG, KRYON_ANDROID_LOG_TAG, __VA_ARGS__)
#define KRYON_ANDROID_LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, KRYON_ANDROID_LOG_TAG, __VA_ARGS__)
#define KRYON_ANDROID_LOG_WARN(...) __android_log_print(ANDROID_LOG_WARN, KRYON_ANDROID_LOG_TAG, __VA_ARGS__)
#define KRYON_ANDROID_LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, KRYON_ANDROID_LOG_TAG, __VA_ARGS__)
#else
#define KRYON_ANDROID_LOG_DEBUG(...) fprintf(stderr, "[DEBUG] " __VA_ARGS__)
#define KRYON_ANDROID_LOG_INFO(...) fprintf(stdout, "[INFO] " __VA_ARGS__)
#define KRYON_ANDROID_LOG_WARN(...) fprintf(stderr, "[WARN] " __VA_ARGS__)
#define KRYON_ANDROID_LOG_ERROR(...) fprintf(stderr, "[ERROR] " __VA_ARGS__)
#endif

// ============================================================================
// Lifecycle Management
// ============================================================================

typedef enum {
    KRYON_ANDROID_STATE_CREATED,
    KRYON_ANDROID_STATE_STARTED,
    KRYON_ANDROID_STATE_RESUMED,
    KRYON_ANDROID_STATE_PAUSED,
    KRYON_ANDROID_STATE_STOPPED,
    KRYON_ANDROID_STATE_DESTROYED
} kryon_android_lifecycle_state_t;

// Initialize platform with ANativeActivity
bool kryon_android_platform_init(void* activity);
void kryon_android_platform_shutdown(void);

// Lifecycle callbacks (called by Java Activity or NativeActivity)
void kryon_android_on_create(void);
void kryon_android_on_start(void);
void kryon_android_on_resume(void);
void kryon_android_on_pause(void);
void kryon_android_on_stop(void);
void kryon_android_on_destroy(void);

kryon_android_lifecycle_state_t kryon_android_get_lifecycle_state(void);

// ============================================================================
// Storage (SharedPreferences + Internal Storage)
// ============================================================================

// Storage limits
#define KRYON_ANDROID_MAX_KEY_LEN 128
#define KRYON_ANDROID_MAX_VALUE_SIZE 4096
#define KRYON_ANDROID_MAX_ENTRIES 128

// Key-value storage (via SharedPreferences JNI)
bool kryon_android_init_storage(void);
bool kryon_android_storage_set(const char* key, const void* data, size_t size);
bool kryon_android_storage_get(const char* key, void* buffer, size_t* size);
bool kryon_android_storage_has(const char* key);
bool kryon_android_storage_remove(const char* key);
void kryon_android_storage_clear(void);

// File storage (Internal storage: /data/data/<package>/files/)
bool kryon_android_file_write(const char* filename, const void* data, size_t size);
bool kryon_android_file_read(const char* filename, void** data, size_t* size);
bool kryon_android_file_exists(const char* filename);
bool kryon_android_file_delete(const char* filename);

const char* kryon_android_get_internal_storage_path(void);
const char* kryon_android_get_cache_path(void);
const char* kryon_android_get_external_storage_path(void);

// ============================================================================
// Input Handling (Touch + Multi-touch + IME)
// ============================================================================

#define KRYON_ANDROID_MAX_TOUCH_POINTS 10

typedef enum {
    KRYON_ANDROID_TOUCH_DOWN = 0,
    KRYON_ANDROID_TOUCH_MOVE = 1,
    KRYON_ANDROID_TOUCH_UP = 2,
    KRYON_ANDROID_TOUCH_CANCEL = 3
} kryon_android_touch_action_t;

typedef struct {
    int32_t pointer_id;
    float x, y;
    float pressure;
    float size;  // Touch area size
    kryon_android_touch_action_t action;
    uint64_t timestamp;
} kryon_android_touch_event_t;

typedef enum {
    KRYON_ANDROID_KEY_DOWN = 0,
    KRYON_ANDROID_KEY_UP = 1,
    KRYON_ANDROID_KEY_REPEAT = 2
} kryon_android_key_action_t;

typedef struct {
    int32_t key_code;
    kryon_android_key_action_t action;
    uint32_t meta_state;  // Shift, Ctrl, Alt modifiers
    uint64_t timestamp;
} kryon_android_key_event_t;

// Event structure for unified event handling
typedef enum {
    KRYON_ANDROID_EVENT_TOUCH,
    KRYON_ANDROID_EVENT_KEY,
    KRYON_ANDROID_EVENT_LIFECYCLE,
    KRYON_ANDROID_EVENT_WINDOW_RESIZE,
    KRYON_ANDROID_EVENT_BACK_PRESSED
} kryon_android_event_type_t;

typedef struct {
    kryon_android_event_type_t type;
    union {
        kryon_android_touch_event_t touch;
        kryon_android_key_event_t key;
        struct {
            kryon_android_lifecycle_state_t state;
        } lifecycle;
        struct {
            int32_t width;
            int32_t height;
        } resize;
    } data;
} kryon_android_event_t;

// Process Android MotionEvents and KeyEvents → Kryon events
bool kryon_android_poll_events(kryon_android_event_t* events, size_t* count, size_t max_events);

// Soft keyboard (IME) control
void kryon_android_show_keyboard(void);
void kryon_android_hide_keyboard(void);
bool kryon_android_is_keyboard_visible(void);

// ============================================================================
// Display & Window Management
// ============================================================================

typedef struct {
    int32_t width;
    int32_t height;
    int32_t density_dpi;
    float density_scale;
    int32_t orientation;  // 0=portrait, 1=landscape, 2=reverse_portrait, 3=reverse_landscape
} kryon_android_display_info_t;

kryon_android_display_info_t kryon_android_get_display_info(void);
void kryon_android_get_display_dimensions(uint16_t* width, uint16_t* height);

#ifdef __ANDROID__
ANativeWindow* kryon_android_get_native_window(void);
#else
void* kryon_android_get_native_window(void);
#endif

// Orientation control
typedef enum {
    KRYON_ANDROID_ORIENTATION_PORTRAIT = 0,
    KRYON_ANDROID_ORIENTATION_LANDSCAPE = 1,
    KRYON_ANDROID_ORIENTATION_REVERSE_PORTRAIT = 2,
    KRYON_ANDROID_ORIENTATION_REVERSE_LANDSCAPE = 3,
    KRYON_ANDROID_ORIENTATION_AUTO = 4
} kryon_android_orientation_t;

void kryon_android_set_orientation(kryon_android_orientation_t orientation);

// ============================================================================
// Permissions Management
// ============================================================================

typedef enum {
    KRYON_ANDROID_PERM_STORAGE,
    KRYON_ANDROID_PERM_CAMERA,
    KRYON_ANDROID_PERM_LOCATION,
    KRYON_ANDROID_PERM_NETWORK,
    KRYON_ANDROID_PERM_MICROPHONE,
    KRYON_ANDROID_PERM_CONTACTS,
    KRYON_ANDROID_PERM_PHONE
} kryon_android_permission_t;

bool kryon_android_request_permission(kryon_android_permission_t permission);
bool kryon_android_has_permission(kryon_android_permission_t permission);

// ============================================================================
// System Utilities
// ============================================================================

// Timing
uint64_t kryon_android_get_timestamp(void);
void kryon_android_delay_ms(uint32_t ms);

// Haptics
void kryon_android_vibrate(uint32_t duration_ms);

// System information
const char* kryon_android_get_package_name(void);
const char* kryon_android_get_app_data_dir(void);
const char* kryon_android_get_device_model(void);
const char* kryon_android_get_android_version(void);
int kryon_android_get_api_level(void);

// Network
bool kryon_android_is_network_available(void);
bool kryon_android_is_wifi_connected(void);

// URL opening
bool kryon_android_open_url(const char* url);

// Share intent
bool kryon_android_share_text(const char* text, const char* subject);

// JNI Environment access (for advanced use)
#ifdef __ANDROID__
JNIEnv* kryon_android_get_jni_env(void);
jobject kryon_android_get_activity_object(void);
ANativeActivity* kryon_android_get_native_activity(void);
#else
void* kryon_android_get_jni_env(void);
void* kryon_android_get_activity_object(void);
void* kryon_android_get_native_activity(void);
#endif

// ============================================================================
// Platform Capabilities
// ============================================================================

typedef struct {
    bool has_opengl_es2;
    bool has_opengl_es3;
    bool has_vulkan;
    bool has_multitouch;
    bool has_accelerometer;
    bool has_gyroscope;
    bool has_gps;
    bool has_nfc;
    bool has_bluetooth;
    int max_touch_points;
    const char* device_model;
    const char* android_version;
    int api_level;
    int32_t total_memory_mb;
    int32_t available_memory_mb;
} kryon_android_capabilities_t;

kryon_android_capabilities_t kryon_android_get_capabilities(void);
void kryon_android_print_info(void);

// ============================================================================
// Hot Reload Support
// ============================================================================

// Hot reload callback type (called when files change)
typedef void (*kryon_android_reload_callback_t)(const char* changed_path, void* user_data);

/**
 * Initialize hot reload system
 * Sets up file watching for development builds
 */
bool kryon_android_hot_reload_init(void);

/**
 * Shutdown hot reload system
 */
void kryon_android_hot_reload_shutdown(void);

/**
 * Add path to watch for changes
 * Can watch directories in the app's data directory
 */
bool kryon_android_watch_path(const char* path, bool recursive);

/**
 * Set callback for file change notifications
 */
void kryon_android_set_reload_callback(kryon_android_reload_callback_t callback, void* user_data);

/**
 * Poll for file changes (call from main loop)
 * Returns number of changes detected
 */
int kryon_android_hot_reload_poll(void);

/**
 * Check if hot reload is available
 * Only available in debug/dev builds, not release
 */
bool kryon_android_hot_reload_available(void);

/**
 * Trigger manual reload (e.g., from dev menu)
 */
void kryon_android_trigger_reload(void);

/**
 * Get app's files directory for watching source files
 */
const char* kryon_android_get_files_dir(void);

/**
 * Check if running in debug/dev mode
 */
bool kryon_android_is_debug_build(void);

// ============================================================================
// Error Handling
// ============================================================================

typedef enum {
    KRYON_ANDROID_ERROR_NONE = 0,
    KRYON_ANDROID_ERROR_NOT_INITIALIZED = 1,
    KRYON_ANDROID_ERROR_JNI_FAILED = 2,
    KRYON_ANDROID_ERROR_PERMISSION_DENIED = 3,
    KRYON_ANDROID_ERROR_IO_FAILED = 4,
    KRYON_ANDROID_ERROR_INVALID_PARAM = 5,
    KRYON_ANDROID_ERROR_OUT_OF_MEMORY = 6,
    KRYON_ANDROID_ERROR_NOT_SUPPORTED = 7
} kryon_android_error_t;

kryon_android_error_t kryon_android_get_last_error(void);
const char* kryon_android_get_error_string(kryon_android_error_t error);

#ifdef __cplusplus
}
#endif

#endif // KRYON_ANDROID_PLATFORM_H
