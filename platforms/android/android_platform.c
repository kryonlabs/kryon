#define _POSIX_C_SOURCE 199309L

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
#include <sys/inotify.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/inotify.h>
#include <fcntl.h>
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

    // Hot reload state
    struct {
        bool initialized;
        int inotify_fd;
        kryon_android_reload_callback_t callback;
        void* callback_user_data;

        // Watched paths
        struct {
            char path[512];
            int watch_descriptor;
            bool recursive;
        } watched_paths[32];
        int watch_count;

        // Debouncing
        uint64_t last_change_time;
        uint64_t debounce_ms;

        // Files directory cache
        char files_dir[512];
        bool files_dir_cached;

        // Debug build cache
        bool is_debug;
        bool debug_cached;
    } hot_reload;

} kryon_android_platform_state_t;

static kryon_android_platform_state_t g_android_platform = {0};

// ============================================================================
// Internal Helper Functions
// ============================================================================

static void set_last_error(kryon_android_error_t error) {
    g_android_platform.last_error = error;
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
    // App can use kryon_android_storage_set() to persist state
    // Example: kryon_android_storage_set("app_state", state_data, size);
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
#ifdef __ANDROID__
    JNIEnv* env = attach_current_thread();
    if (!env || !g_android_platform.activity_object) {
        KRYON_ANDROID_LOG_WARN("Cannot set orientation: JNI not ready\n");
        return;
    }

    // Get Activity class
    jclass activity_class = (*env)->GetObjectClass(env, g_android_platform.activity_object);
    if (!activity_class) {
        KRYON_ANDROID_LOG_ERROR("Failed to get Activity class\n");
        return;
    }

    // Get setRequestedOrientation method
    jmethodID set_orientation = (*env)->GetMethodID(env, activity_class,
                                                     "setRequestedOrientation",
                                                     "(I)V");
    if (!set_orientation) {
        KRYON_ANDROID_LOG_ERROR("Failed to get setRequestedOrientation method\n");
        (*env)->DeleteLocalRef(env, activity_class);
        return;
    }

    // Map orientation to Android ActivityInfo constants
    jint android_orientation;
    switch (orientation) {
        case KRYON_ANDROID_ORIENTATION_PORTRAIT:
            android_orientation = 7;  // ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
            break;
        case KRYON_ANDROID_ORIENTATION_LANDSCAPE:
            android_orientation = 0;  // ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            break;
        case KRYON_ANDROID_ORIENTATION_REVERSE_PORTRAIT:
            android_orientation = 9;  // ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
            break;
        case KRYON_ANDROID_ORIENTATION_REVERSE_LANDSCAPE:
            android_orientation = 8;  // ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
            break;
        case KRYON_ANDROID_ORIENTATION_AUTO:
        default:
            android_orientation = 2;  // ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
            break;
    }

    (*env)->CallVoidMethod(env, g_android_platform.activity_object, set_orientation, android_orientation);
    (*env)->DeleteLocalRef(env, activity_class);
#else
    (void)orientation;
    KRYON_ANDROID_LOG_INFO("Orientation control not implemented on desktop\n");
#endif
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
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void kryon_android_vibrate(uint32_t duration_ms) {
    KRYON_ANDROID_LOG_INFO("Vibrate: %u ms\n", duration_ms);
#ifdef __ANDROID__
    JNIEnv* env = attach_current_thread();
    if (!env || !g_android_platform.activity_object) {
        KRYON_ANDROID_LOG_WARN("Cannot vibrate: JNI not ready\n");
        return;
    }

    // Get Vibrator service
    jclass context_class = (*env)->FindClass(env, "android/content/Context");
    if (!context_class) {
        KRYON_ANDROID_LOG_ERROR("Failed to find Context class\n");
        return;
    }

    jmethodID get_vibrator = (*env)->GetMethodID(env, context_class,
                                                   "getSystemService",
                                                   "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!get_vibrator) {
        KRYON_ANDROID_LOG_ERROR("Failed to get getSystemService method\n");
        (*env)->DeleteLocalRef(env, context_class);
        return;
    }

    jstring vibrator_service = (*env)->NewStringUTF(env, "vibrator");
    jobject vibrator = (*env)->CallObjectMethod(env, g_android_platform.activity_object,
                                                   get_vibrator, vibrator_service);
    (*env)->DeleteLocalRef(env, vibrator_service);

    if (!vibrator) {
        KRYON_ANDROID_LOG_ERROR("Failed to get Vibrator service\n");
        (*env)->DeleteLocalRef(env, context_class);
        return;
    }

    // Check API level for different vibrate methods
    if (g_android_platform.api_level >= 26) {
        // API 26+ (Android 8.0+) use Vibrator class
        jclass vibrator_class = (*env)->GetObjectClass(env, vibrator);

        // Create VibrationEffect
        jclass vibration_effect_class = (*env)->FindClass(env, "android/os/VibrationEffect");
        if (vibration_effect_class) {
            jmethodID create_one_shot = (*env)->GetStaticMethodID(env, vibration_effect_class,
                                                                     "createOneShot",
                                                                     "(J)Landroid/os/VibrationEffect;");
            if (create_one_shot) {
                jlong duration_ms_long = (jlong)duration_ms;
                jobject vibration_effect = (*env)->CallStaticObjectMethod(env, vibration_effect_class,
                                                                            create_one_shot, duration_ms_long);

                // Get vibrator method
                jmethodID vibrate_effect = (*env)->GetMethodID(env, vibrator_class,
                                                                "vibrate",
                                                                "(Landroid/os/VibrationEffect;)V");
                if (vibrate_effect) {
                    (*env)->CallVoidMethod(env, vibrator, vibrate_effect, vibration_effect);
                }
                (*env)->DeleteLocalRef(env, vibration_effect);
            }
            (*env)->DeleteLocalRef(env, vibration_effect_class);
        }
        (*env)->DeleteLocalRef(env, vibrator_class);
    } else {
        // Legacy vibrate method (deprecated but works)
        jclass vibrator_class = (*env)->GetObjectClass(env, vibrator);
        jmethodID vibrate = (*env)->GetMethodID(env, vibrator_class, "vibrate", "(J)V");
        if (vibrate) {
            jlong duration_ms_long = (jlong)duration_ms;
            (*env)->CallVoidMethod(env, vibrator, vibrate, duration_ms_long);
        }
        (*env)->DeleteLocalRef(env, vibrator_class);
    }

    (*env)->DeleteLocalRef(env, vibrator);
    (*env)->DeleteLocalRef(env, context_class);
#else
    (void)duration_ms;
    KRYON_ANDROID_LOG_INFO("Vibration not implemented on desktop\n");
#endif
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
#ifdef __ANDROID__
    JNIEnv* env = attach_current_thread();
    if (!env || !g_android_platform.activity_object) {
        KRYON_ANDROID_LOG_WARN("Cannot open URL: JNI not ready\n");
        return false;
    }

    // Get Intent class
    jclass intent_class = (*env)->FindClass(env, "android/content/Intent");
    if (!intent_class) {
        KRYON_ANDROID_LOG_ERROR("Failed to find Intent class\n");
        return false;
    }

    // Create Intent with ACTION_VIEW
    jfieldID action_view = (*env)->GetStaticFieldID(env, intent_class,
                                                    "ACTION_VIEW",
                                                    "Ljava/lang/String;");
    if (!action_view) {
        KRYON_ANDROID_LOG_ERROR("Failed to get ACTION_VIEW field\n");
        (*env)->DeleteLocalRef(env, intent_class);
        return false;
    }

    jobject action_view_string = (*env)->GetStaticObjectField(env, intent_class, action_view);

    // Create Uri from URL string
    jclass uri_class = (*env)->FindClass(env, "android/net/Uri");
    jmethodID uri_parse = (*env)->GetStaticMethodID(env, uri_class, "parse",
                                                       "(Ljava/lang/String;)Landroid/net/Uri;");
    if (!uri_parse) {
        KRYON_ANDROID_LOG_ERROR("Failed to get Uri.parse method\n");
        (*env)->DeleteLocalRef(env, intent_class);
        (*env)->DeleteLocalRef(env, action_view_string);
        return false;
    }

    jstring url_string = (*env)->NewStringUTF(env, url);
    jobject uri = (*env)->CallStaticObjectMethod(env, uri_class, uri_parse, url_string);

    // Create Intent
    jmethodID intent_init = (*env)->GetMethodID(env, intent_class, "<init>",
                                                      "(Ljava/lang/String;Landroid/net/Uri;)V");
    jobject intent = (*env)->AllocObject(env, intent_class);
    (*env)->CallVoidMethod(env, intent, intent_init, action_view_string, uri);

    // Get startActivity method from Activity class
    jclass activity_class = (*env)->GetObjectClass(env, g_android_platform.activity_object);
    jmethodID start_activity = (*env)->GetMethodID(env, activity_class,
                                                       "startActivity",
                                                       "(Landroid/content/Intent;)V");
    if (start_activity) {
        (*env)->CallVoidMethod(env, g_android_platform.activity_object, start_activity, intent);
        (*env)->DeleteLocalRef(env, intent);
        (*env)->DeleteLocalRef(env, activity_class);
        (*env)->DeleteLocalRef(env, intent_class);
        (*env)->DeleteLocalRef(env, uri_class);
        (*env)->DeleteLocalRef(env, action_view_string);
        (*env)->DeleteLocalRef(env, url_string);
        (*env)->DeleteLocalRef(env, uri);
        return true;
    }

    (*env)->DeleteLocalRef(env, intent);
    (*env)->DeleteLocalRef(env, activity_class);
    (*env)->DeleteLocalRef(env, intent_class);
    (*env)->DeleteLocalRef(env, uri_class);
    (*env)->DeleteLocalRef(env, action_view_string);
    (*env)->DeleteLocalRef(env, url_string);
    (*env)->DeleteLocalRef(env, uri);
    return false;
#else
    (void)url;
    KRYON_ANDROID_LOG_INFO("URL opening not implemented on desktop\n");
    return false;
#endif
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
// Hot Reload Support
// ============================================================================

#define HOT_RELOAD_EVENT_BUFFER_SIZE 4096

bool kryon_android_hot_reload_init(void) {
    if (g_android_platform.hot_reload.initialized) {
        return true;  // Already initialized
    }

    KRYON_ANDROID_LOG_INFO("Initializing hot reload system\n");

    // Initialize inotify
#ifdef __ANDROID__
    g_android_platform.hot_reload.inotify_fd = inotify_init1(IN_NONBLOCK);
    if (g_android_platform.hot_reload.inotify_fd < 0) {
        KRYON_ANDROID_LOG_ERROR("Failed to initialize inotify: %s\n", strerror(errno));
        return false;
    }
#else
    g_android_platform.hot_reload.inotify_fd = -1;
#endif

    g_android_platform.hot_reload.watch_count = 0;
    g_android_platform.hot_reload.callback = NULL;
    g_android_platform.hot_reload.callback_user_data = NULL;
    g_android_platform.hot_reload.last_change_time = 0;
    g_android_platform.hot_reload.debounce_ms = 100;  // 100ms default debounce
    g_android_platform.hot_reload.files_dir_cached = false;
    g_android_platform.hot_reload.debug_cached = false;
    g_android_platform.hot_reload.initialized = true;

    KRYON_ANDROID_LOG_INFO("Hot reload system initialized\n");
    return true;
}

void kryon_android_hot_reload_shutdown(void) {
    if (!g_android_platform.hot_reload.initialized) {
        return;
    }

    KRYON_ANDROID_LOG_INFO("Shutting down hot reload system\n");

    // Remove all watches
#ifdef __ANDROID__
    if (g_android_platform.hot_reload.inotify_fd >= 0) {
        for (int i = 0; i < g_android_platform.hot_reload.watch_count; i++) {
            inotify_rm_watch(g_android_platform.hot_reload.inotify_fd,
                            g_android_platform.hot_reload.watched_paths[i].watch_descriptor);
        }
        close(g_android_platform.hot_reload.inotify_fd);
        g_android_platform.hot_reload.inotify_fd = -1;
    }
#endif

    g_android_platform.hot_reload.watch_count = 0;
    g_android_platform.hot_reload.initialized = false;
}

bool kryon_android_watch_path(const char* path, bool recursive) {
    if (!g_android_platform.hot_reload.initialized) {
        KRYON_ANDROID_LOG_ERROR("Hot reload not initialized\n");
        return false;
    }

    if (!path || g_android_platform.hot_reload.watch_count >= 32) {
        KRYON_ANDROID_LOG_ERROR("Invalid path or too many watches\n");
        return false;
    }

#ifdef __ANDROID__
    // Check if path exists
    struct stat st;
    if (stat(path, &st) != 0) {
        KRYON_ANDROID_LOG_ERROR("Path does not exist: %s\n", path);
        return false;
    }

    // Watch for: modify, create, delete, move events
    uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;

    int wd = inotify_add_watch(g_android_platform.hot_reload.inotify_fd, path, mask);
    if (wd < 0) {
        KRYON_ANDROID_LOG_ERROR("Failed to add watch for %s: %s\n", path, strerror(errno));
        return false;
    }

    // Store watch info
    int idx = g_android_platform.hot_reload.watch_count;
    strncpy(g_android_platform.hot_reload.watched_paths[idx].path, path, 511);
    g_android_platform.hot_reload.watched_paths[idx].path[511] = '\0';
    g_android_platform.hot_reload.watched_paths[idx].watch_descriptor = wd;
    g_android_platform.hot_reload.watched_paths[idx].recursive = recursive;
    g_android_platform.hot_reload.watch_count++;

    KRYON_ANDROID_LOG_INFO("Added watch for: %s (wd=%d)\n", path, wd);
    return true;
#else
    (void)recursive;
    return false;
#endif
}

void kryon_android_set_reload_callback(kryon_android_reload_callback_t callback, void* user_data) {
    g_android_platform.hot_reload.callback = callback;
    g_android_platform.hot_reload.callback_user_data = user_data;
}

int kryon_android_hot_reload_poll(void) {
    if (!g_android_platform.hot_reload.initialized ||
        g_android_platform.hot_reload.inotify_fd < 0) {
        return 0;
    }

#ifdef __ANDROID__
    char buffer[HOT_RELOAD_EVENT_BUFFER_SIZE];
    ssize_t len = read(g_android_platform.hot_reload.inotify_fd, buffer, sizeof(buffer));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No events pending
        }
        return 0;
    }

    int event_count = 0;
    const struct inotify_event* event;

    for (char* ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + event->len) {
        event = (const struct inotify_event*)ptr;

        // Find the watched path for this event
        const char* base_path = NULL;
        for (int i = 0; i < g_android_platform.hot_reload.watch_count; i++) {
            if (g_android_platform.hot_reload.watched_paths[i].watch_descriptor == event->wd) {
                base_path = g_android_platform.hot_reload.watched_paths[i].path;
                break;
            }
        }

        if (!base_path) continue;

        // Build full path
        char full_path[512];
        if (event->len > 0) {
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, event->name);
        } else {
            strncpy(full_path, base_path, sizeof(full_path) - 1);
            full_path[sizeof(full_path) - 1] = '\0';
        }

        // Check debounce
        uint64_t now = (uint64_t)time(NULL) * 1000;
        if (now - g_android_platform.hot_reload.last_change_time < g_android_platform.hot_reload.debounce_ms) {
            continue;
        }

        // Filter for relevant file types
        const char* ext = strrchr(full_path, '.');
        if (ext) {
            bool is_relevant = (strcmp(ext, ".kry") == 0 ||
                               strcmp(ext, ".kir") == 0 ||
                               strcmp(ext, ".h") == 0 ||
                               strcmp(ext, ".c") == 0 ||
                               strcmp(ext, ".json") == 0);

            if (is_relevant && event->mask & (IN_MODIFY | IN_CREATE)) {
                KRYON_ANDROID_LOG_INFO("File changed: %s\n", full_path);

                // Trigger callback
                if (g_android_platform.hot_reload.callback) {
                    g_android_platform.hot_reload.callback(full_path,
                                                         g_android_platform.hot_reload.callback_user_data);
                }

                g_android_platform.hot_reload.last_change_time = now;
                event_count++;
            }
        }
    }

    return event_count;
#else
    return 0;
#endif
}

bool kryon_android_hot_reload_available(void) {
#ifdef __ANDROID__
    // Only available in debug builds
    return kryon_android_is_debug_build();
#else
    return false;
#endif
}

void kryon_android_trigger_reload(void) {
    KRYON_ANDROID_LOG_INFO("Manual reload triggered\n");

    if (g_android_platform.hot_reload.callback) {
        g_android_platform.hot_reload.callback("<manual>",
                                             g_android_platform.hot_reload.callback_user_data);
    }
}

const char* kryon_android_get_files_dir(void) {
    if (!g_android_platform.hot_reload.files_dir_cached) {
#ifdef __ANDROID__
        JNIEnv* env = attach_current_thread();
        if (env && g_android_platform.activity_object) {
            // Get Context.getFilesDir()
            jclass activity_class = (*env)->GetObjectClass(env, g_android_platform.activity_object);
            jmethodID get_files_dir = (*env)->GetMethodID(env, activity_class, "getFilesDir",
                                                         "()Ljava/io/File;");
            if (get_files_dir) {
                jobject file_obj = (*env)->CallObjectMethod(env, g_android_platform.activity_object,
                                                           get_files_dir);
                if (file_obj) {
                    // Call File.getAbsolutePath()
                    jclass file_class = (*env)->FindClass(env, "java/io/File");
                    jmethodID get_abs_path = (*env)->GetMethodID(env, file_class, "getAbsolutePath",
                                                                "()Ljava/lang/String;");
                    if (get_abs_path) {
                        jstring path_str = (*env)->CallObjectMethod(env, file_obj, get_abs_path);
                        if (path_str) {
                            const char* path = (*env)->GetStringUTFChars(env, path_str, NULL);
                            strncpy(g_android_platform.hot_reload.files_dir, path, 511);
                            g_android_platform.hot_reload.files_dir[511] = '\0';
                            (*env)->ReleaseStringUTFChars(env, path_str, path);
                            (*env)->DeleteLocalRef(env, path_str);
                        }
                    }
                    (*env)->DeleteLocalRef(env, file_obj);
                }
            }
            (*env)->DeleteLocalRef(env, activity_class);
        }
        g_android_platform.hot_reload.files_dir_cached = true;
#endif
    }

    return g_android_platform.hot_reload.files_dir;
}

bool kryon_android_is_debug_build(void) {
    if (!g_android_platform.hot_reload.debug_cached) {
#ifdef __ANDROID__
        // Check debug flag via ApplicationInfo.flags
        JNIEnv* env = attach_current_thread();
        if (env && g_android_platform.activity_object) {
            jclass activity_class = (*env)->GetObjectClass(env, g_android_platform.activity_object);

            // Get ApplicationInfo via getApplicationInfo()
            jmethodID get_app_info = (*env)->GetMethodID(env, activity_class, "getApplicationInfo",
                                                        "()Landroid/content/pm/ApplicationInfo;");
            if (get_app_info) {
                jobject app_info_obj = (*env)->CallObjectMethod(env, g_android_platform.activity_object,
                                                               get_app_info);
                if (app_info_obj) {
                    jclass app_info_class = (*env)->FindClass(env, "android/content/pm/ApplicationInfo");
                    jfieldID flags_field = (*env)->GetFieldID(env, app_info_class, "flags", "I");
                    if (flags_field) {
                        int flags = (*env)->GetIntField(env, app_info_obj, flags_field);
                        // DEBUGgable = 0x2 (ApplicationInfo.FLAG_DEBUGGABLE)
                        g_android_platform.hot_reload.is_debug = (flags & 0x2) != 0;
                    }
                    (*env)->DeleteLocalRef(env, app_info_obj);
                }
            }
            (*env)->DeleteLocalRef(env, activity_class);
        }
        g_android_platform.hot_reload.debug_cached = true;
#endif
    }

    return g_android_platform.hot_reload.is_debug;
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
