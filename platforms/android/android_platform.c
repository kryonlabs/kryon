#include "android_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/configuration.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#endif

// ============================================================================
// Platform State
// ============================================================================

typedef struct {
    bool initialized;
    kryon_android_lifecycle_state_t lifecycle_state;
    kryon_android_error_t last_error;

#ifdef __ANDROID__
    ANativeActivity* activity;
    AAssetManager* asset_manager;
    JNIEnv* jni_env;
    jobject activity_object;
    ANativeWindow* window;
#else
    void* activity;
    void* asset_manager;
    void* jni_env;
    void* activity_object;
    void* window;
#endif

    // Display info cache
    kryon_android_display_info_t display_info;
    bool display_info_cached;

    // Storage paths
    char internal_storage_path[256];
    char cache_path[256];
    char external_storage_path[256];

    // Platform capabilities
    kryon_android_capabilities_t capabilities;
    bool capabilities_cached;

    // Device info
    char device_model[64];
    char android_version[32];
    char package_name[128];
    int api_level;

    // Keyboard state
    bool keyboard_visible;

    // Event queue
    kryon_android_event_t event_queue[32];
    size_t event_queue_head;
    size_t event_queue_tail;
    size_t event_queue_count;

} kryon_android_platform_state_t;

static kryon_android_platform_state_t g_android_platform = {0};

// ============================================================================
// Internal Helper Functions
// ============================================================================

static void set_last_error(kryon_android_error_t error) {
    g_android_platform.last_error = error;
}

static bool is_initialized(void) {
    if (!g_android_platform.initialized) {
        set_last_error(KRYON_ANDROID_ERROR_NOT_INITIALIZED);
        KRYON_ANDROID_LOG_ERROR("Platform not initialized\n");
        return false;
    }
    return true;
}

#ifdef __ANDROID__
static JNIEnv* attach_current_thread(void) {
    JNIEnv* env = NULL;
    JavaVM* vm = g_android_platform.activity->vm;

    if (!vm) {
        KRYON_ANDROID_LOG_ERROR("JavaVM is NULL\n");
        return NULL;
    }

    int status = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        status = (*vm)->AttachCurrentThread(vm, &env, NULL);
        if (status != JNI_OK) {
            KRYON_ANDROID_LOG_ERROR("Failed to attach thread to JVM\n");
            return NULL;
        }
    }

    return env;
}
#endif

static void query_device_info(void) {
#ifdef __ANDROID__
    // Get JNI environment
    JNIEnv* env = attach_current_thread();
    if (!env) return;

    // Get Build class
    jclass build_class = (*env)->FindClass(env, "android/os/Build");
    if (!build_class) {
        KRYON_ANDROID_LOG_ERROR("Failed to find Build class\n");
        return;
    }

    // Get Build.MODEL
    jfieldID model_field = (*env)->GetStaticFieldID(env, build_class, "MODEL", "Ljava/lang/String;");
    if (model_field) {
        jstring model_str = (jstring)(*env)->GetStaticObjectField(env, build_class, model_field);
        const char* model = (*env)->GetStringUTFChars(env, model_str, NULL);
        strncpy(g_android_platform.device_model, model, sizeof(g_android_platform.device_model) - 1);
        (*env)->ReleaseStringUTFChars(env, model_str, model);
    }

    // Get Build.VERSION.RELEASE
    jclass version_class = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (version_class) {
        jfieldID release_field = (*env)->GetStaticFieldID(env, version_class, "RELEASE", "Ljava/lang/String;");
        if (release_field) {
            jstring release_str = (jstring)(*env)->GetStaticObjectField(env, version_class, release_field);
            const char* release = (*env)->GetStringUTFChars(env, release_str, NULL);
            strncpy(g_android_platform.android_version, release, sizeof(g_android_platform.android_version) - 1);
            (*env)->ReleaseStringUTFChars(env, release_str, release);
        }

        // Get Build.VERSION.SDK_INT
        jfieldID sdk_field = (*env)->GetStaticFieldID(env, version_class, "SDK_INT", "I");
        if (sdk_field) {
            g_android_platform.api_level = (*env)->GetStaticIntField(env, version_class, sdk_field);
        }
    }

    KRYON_ANDROID_LOG_INFO("Device: %s, Android: %s (API %d)\n",
                           g_android_platform.device_model,
                           g_android_platform.android_version,
                           g_android_platform.api_level);
#else
    strncpy(g_android_platform.device_model, "Emulator", sizeof(g_android_platform.device_model) - 1);
    strncpy(g_android_platform.android_version, "Unknown", sizeof(g_android_platform.android_version) - 1);
    g_android_platform.api_level = 0;
#endif
}

static void query_storage_paths(void) {
#ifdef __ANDROID__
    // Get internal storage path from activity
    if (g_android_platform.activity) {
        const char* internal_path = g_android_platform.activity->internalDataPath;
        if (internal_path) {
            strncpy(g_android_platform.internal_storage_path, internal_path,
                    sizeof(g_android_platform.internal_storage_path) - 1);
        }

        // Cache path is typically <internal>/cache
        snprintf(g_android_platform.cache_path, sizeof(g_android_platform.cache_path),
                 "%s/cache", internal_path ? internal_path : "/data/local/tmp");

        // External storage path
        const char* external_path = g_android_platform.activity->externalDataPath;
        if (external_path) {
            strncpy(g_android_platform.external_storage_path, external_path,
                    sizeof(g_android_platform.external_storage_path) - 1);
        }
    }
#else
    // Mock paths for non-Android builds
    strncpy(g_android_platform.internal_storage_path, "/tmp/kryon_internal",
            sizeof(g_android_platform.internal_storage_path) - 1);
    strncpy(g_android_platform.cache_path, "/tmp/kryon_cache",
            sizeof(g_android_platform.cache_path) - 1);
    strncpy(g_android_platform.external_storage_path, "/tmp/kryon_external",
            sizeof(g_android_platform.external_storage_path) - 1);
#endif

    KRYON_ANDROID_LOG_INFO("Internal storage: %s\n", g_android_platform.internal_storage_path);
    KRYON_ANDROID_LOG_INFO("Cache path: %s\n", g_android_platform.cache_path);
    KRYON_ANDROID_LOG_INFO("External storage: %s\n", g_android_platform.external_storage_path);
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool kryon_android_platform_init(void* activity) {
    if (g_android_platform.initialized) {
        KRYON_ANDROID_LOG_WARN("Platform already initialized\n");
        return true;
    }

    memset(&g_android_platform, 0, sizeof(g_android_platform));

#ifdef __ANDROID__
    g_android_platform.activity = (ANativeActivity*)activity;
    if (!g_android_platform.activity) {
        KRYON_ANDROID_LOG_ERROR("ANativeActivity is NULL\n");
        set_last_error(KRYON_ANDROID_ERROR_INVALID_PARAM);
        return false;
    }

    // Get JNI environment
    g_android_platform.jni_env = attach_current_thread();
    if (!g_android_platform.jni_env) {
        set_last_error(KRYON_ANDROID_ERROR_JNI_FAILED);
        return false;
    }

    // Get activity object
    g_android_platform.activity_object = g_android_platform.activity->clazz;

    // Get asset manager
    g_android_platform.asset_manager = g_android_platform.activity->assetManager;
#else
    g_android_platform.activity = activity;
    KRYON_ANDROID_LOG_INFO("Running in non-Android mode (stub)\n");
#endif

    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_CREATED;
    g_android_platform.initialized = true;

    // Query device information
    query_device_info();

    // Query storage paths
    query_storage_paths();

    // Initialize storage subsystem
    if (!kryon_android_init_storage()) {
        KRYON_ANDROID_LOG_ERROR("Failed to initialize storage\n");
        return false;
    }

    KRYON_ANDROID_LOG_INFO("Platform initialized successfully\n");
    return true;
}

void kryon_android_platform_shutdown(void) {
    if (!g_android_platform.initialized) {
        return;
    }

    KRYON_ANDROID_LOG_INFO("Shutting down platform\n");

    // Clear any cached data
    g_android_platform.display_info_cached = false;
    g_android_platform.capabilities_cached = false;

    g_android_platform.initialized = false;
    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_DESTROYED;
}

// ============================================================================
// Lifecycle Management
// ============================================================================

void kryon_android_on_create(void) {
    KRYON_ANDROID_LOG_INFO("Lifecycle: onCreate\n");
    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_CREATED;
}

void kryon_android_on_start(void) {
    KRYON_ANDROID_LOG_INFO("Lifecycle: onStart\n");
    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_STARTED;
}

void kryon_android_on_resume(void) {
    KRYON_ANDROID_LOG_INFO("Lifecycle: onResume\n");
    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_RESUMED;
}

void kryon_android_on_pause(void) {
    KRYON_ANDROID_LOG_INFO("Lifecycle: onPause\n");
    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_PAUSED;

    // Save any pending state
    // TODO: Implement state saving
}

void kryon_android_on_stop(void) {
    KRYON_ANDROID_LOG_INFO("Lifecycle: onStop\n");
    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_STOPPED;
}

void kryon_android_on_destroy(void) {
    KRYON_ANDROID_LOG_INFO("Lifecycle: onDestroy\n");
    g_android_platform.lifecycle_state = KRYON_ANDROID_STATE_DESTROYED;
}

kryon_android_lifecycle_state_t kryon_android_get_lifecycle_state(void) {
    return g_android_platform.lifecycle_state;
}

// ============================================================================
// Display & Window Management
// ============================================================================

kryon_android_display_info_t kryon_android_get_display_info(void) {
    if (g_android_platform.display_info_cached) {
        return g_android_platform.display_info;
    }

    kryon_android_display_info_t info = {0};

#ifdef __ANDROID__
    if (g_android_platform.window) {
        info.width = ANativeWindow_getWidth(g_android_platform.window);
        info.height = ANativeWindow_getHeight(g_android_platform.window);
    }

    // Get display metrics via JNI
    JNIEnv* env = attach_current_thread();
    if (env && g_android_platform.activity_object) {
        // Get DisplayMetrics from Resources
        jclass activity_class = (*env)->GetObjectClass(env, g_android_platform.activity_object);
        jmethodID get_resources = (*env)->GetMethodID(env, activity_class, "getResources", "()Landroid/content/res/Resources;");
        if (get_resources) {
            jobject resources = (*env)->CallObjectMethod(env, g_android_platform.activity_object, get_resources);
            jclass resources_class = (*env)->GetObjectClass(env, resources);
            jmethodID get_display_metrics = (*env)->GetMethodID(env, resources_class, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
            if (get_display_metrics) {
                jobject metrics = (*env)->CallObjectMethod(env, resources, get_display_metrics);
                jclass metrics_class = (*env)->GetObjectClass(env, metrics);

                jfieldID density_dpi_field = (*env)->GetFieldID(env, metrics_class, "densityDpi", "I");
                jfieldID density_field = (*env)->GetFieldID(env, metrics_class, "density", "F");

                if (density_dpi_field) {
                    info.density_dpi = (*env)->GetIntField(env, metrics, density_dpi_field);
                }
                if (density_field) {
                    info.density_scale = (*env)->GetFloatField(env, metrics, density_field);
                }
            }
        }
    }
#else
    // Stub values for non-Android
    info.width = 1080;
    info.height = 1920;
    info.density_dpi = 480;
    info.density_scale = 3.0f;
#endif

    info.orientation = KRYON_ANDROID_ORIENTATION_PORTRAIT;

    g_android_platform.display_info = info;
    g_android_platform.display_info_cached = true;

    return info;
}

void kryon_android_get_display_dimensions(uint16_t* width, uint16_t* height) {
    kryon_android_display_info_t info = kryon_android_get_display_info();
    if (width) *width = (uint16_t)info.width;
    if (height) *height = (uint16_t)info.height;
}

#ifdef __ANDROID__
ANativeWindow* kryon_android_get_native_window(void) {
    return g_android_platform.window;
}
#else
void* kryon_android_get_native_window(void) {
    return g_android_platform.window;
}
#endif

void kryon_android_set_orientation(kryon_android_orientation_t orientation) {
    KRYON_ANDROID_LOG_INFO("Setting orientation: %d\n", orientation);
    // TODO: Implement via JNI call to setRequestedOrientation
    (void)orientation;
}

// ============================================================================
// System Utilities
// ============================================================================

uint64_t kryon_android_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void kryon_android_delay_ms(uint32_t ms) {
    usleep(ms * 1000);
}

void kryon_android_vibrate(uint32_t duration_ms) {
    KRYON_ANDROID_LOG_INFO("Vibrate: %u ms\n", duration_ms);
    // TODO: Implement via JNI call to Vibrator service
    (void)duration_ms;
}

const char* kryon_android_get_package_name(void) {
    return g_android_platform.package_name;
}

const char* kryon_android_get_app_data_dir(void) {
    return g_android_platform.internal_storage_path;
}

const char* kryon_android_get_device_model(void) {
    return g_android_platform.device_model;
}

const char* kryon_android_get_android_version(void) {
    return g_android_platform.android_version;
}

int kryon_android_get_api_level(void) {
    return g_android_platform.api_level;
}

const char* kryon_android_get_internal_storage_path(void) {
    return g_android_platform.internal_storage_path;
}

const char* kryon_android_get_cache_path(void) {
    return g_android_platform.cache_path;
}

const char* kryon_android_get_external_storage_path(void) {
    return g_android_platform.external_storage_path;
}

bool kryon_android_open_url(const char* url) {
    if (!url) return false;
    KRYON_ANDROID_LOG_INFO("Opening URL: %s\n", url);
    // TODO: Implement via JNI Intent.ACTION_VIEW
    return false;
}

// ============================================================================
// JNI Environment Access
// ============================================================================

#ifdef __ANDROID__
JNIEnv* kryon_android_get_jni_env(void) {
    return g_android_platform.jni_env;
}

jobject kryon_android_get_activity_object(void) {
    return g_android_platform.activity_object;
}

ANativeActivity* kryon_android_get_native_activity(void) {
    return g_android_platform.activity;
}
#else
void* kryon_android_get_jni_env(void) {
    return g_android_platform.jni_env;
}

void* kryon_android_get_activity_object(void) {
    return g_android_platform.activity_object;
}

void* kryon_android_get_native_activity(void) {
    return g_android_platform.activity;
}
#endif

// ============================================================================
// Platform Capabilities
// ============================================================================

kryon_android_capabilities_t kryon_android_get_capabilities(void) {
    if (g_android_platform.capabilities_cached) {
        return g_android_platform.capabilities;
    }

    kryon_android_capabilities_t caps = {0};

#ifdef __ANDROID__
    // Query OpenGL ES support
    caps.has_opengl_es2 = (g_android_platform.api_level >= 8);   // API 8+
    caps.has_opengl_es3 = (g_android_platform.api_level >= 18);  // API 18+
    caps.has_vulkan = (g_android_platform.api_level >= 24);      // API 24+

    // Multitouch support (assume 10 points for modern devices)
    caps.has_multitouch = true;
    caps.max_touch_points = 10;

    // Sensors (assume available on most devices)
    caps.has_accelerometer = true;
    caps.has_gyroscope = true;
    caps.has_gps = true;
    caps.has_nfc = false;  // Not all devices have NFC
    caps.has_bluetooth = true;

    caps.api_level = g_android_platform.api_level;
#else
    // Stub values for non-Android
    caps.has_opengl_es3 = true;
    caps.has_multitouch = true;
    caps.max_touch_points = 10;
    caps.api_level = 0;
#endif

    caps.device_model = g_android_platform.device_model;
    caps.android_version = g_android_platform.android_version;

    g_android_platform.capabilities = caps;
    g_android_platform.capabilities_cached = true;

    return caps;
}

void kryon_android_print_info(void) {
    kryon_android_capabilities_t caps = kryon_android_get_capabilities();
    kryon_android_display_info_t display = kryon_android_get_display_info();

    printf("=== Kryon Android Platform Info ===\n");
    printf("Device Model: %s\n", caps.device_model);
    printf("Android Version: %s (API %d)\n", caps.android_version, caps.api_level);
    printf("Display: %dx%d @ %d DPI (scale: %.2f)\n",
           display.width, display.height, display.density_dpi, display.density_scale);
    printf("OpenGL ES 2.0: %s\n", caps.has_opengl_es2 ? "Yes" : "No");
    printf("OpenGL ES 3.0: %s\n", caps.has_opengl_es3 ? "Yes" : "No");
    printf("Vulkan: %s\n", caps.has_vulkan ? "Yes" : "No");
    printf("Multi-touch: %s (%d points)\n",
           caps.has_multitouch ? "Yes" : "No", caps.max_touch_points);
    printf("Accelerometer: %s\n", caps.has_accelerometer ? "Yes" : "No");
    printf("Gyroscope: %s\n", caps.has_gyroscope ? "Yes" : "No");
    printf("GPS: %s\n", caps.has_gps ? "Yes" : "No");
    printf("===================================\n");
}

// ============================================================================
// Error Handling
// ============================================================================

kryon_android_error_t kryon_android_get_last_error(void) {
    return g_android_platform.last_error;
}

const char* kryon_android_get_error_string(kryon_android_error_t error) {
    switch (error) {
        case KRYON_ANDROID_ERROR_NONE: return "No error";
        case KRYON_ANDROID_ERROR_NOT_INITIALIZED: return "Platform not initialized";
        case KRYON_ANDROID_ERROR_JNI_FAILED: return "JNI operation failed";
        case KRYON_ANDROID_ERROR_PERMISSION_DENIED: return "Permission denied";
        case KRYON_ANDROID_ERROR_IO_FAILED: return "I/O operation failed";
        case KRYON_ANDROID_ERROR_INVALID_PARAM: return "Invalid parameter";
        case KRYON_ANDROID_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case KRYON_ANDROID_ERROR_NOT_SUPPORTED: return "Operation not supported";
        default: return "Unknown error";
    }
}
