#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <string.h>

// Include Kryon headers
#include "android_platform.h"
#include "android_renderer.h"
#include "android_internal.h"

#define LOG_TAG "KryonJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// ============================================================================
// Native Handle Structure
// ============================================================================

// Forward declaration of DSLBuildContext (defined in kryon_dsl_jni.c)
typedef struct DSLBuildContext DSLBuildContext;

typedef struct {
    jobject activity_ref;  // Global reference to Activity
    JavaVM* jvm;
    ANativeWindow* window;
    AndroidRenderer* renderer;
    AndroidIRRenderer* ir_renderer;
    bool initialized;
    bool surface_ready;
    DSLBuildContext* dsl_context;  // DSL build context for setContent()
} KryonNativeContext;

// ============================================================================
// JNI Helper Functions
// ============================================================================

static JNIEnv* get_jni_env(JavaVM* jvm) {
    JNIEnv* env = NULL;
    int status = (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        status = (*jvm)->AttachCurrentThread(jvm, &env, NULL);
        if (status != JNI_OK) {
            LOGE("Failed to attach thread to JVM\n");
            return NULL;
        }
    }

    return env;
}

// ============================================================================
// Lifecycle Methods
// ============================================================================

JNIEXPORT jlong JNICALL
Java_com_kryon_KryonActivity_nativeInit(JNIEnv* env, jobject thiz, jobject activity) {
    LOGI("nativeInit called\n");

    // Create native context
    KryonNativeContext* ctx = (KryonNativeContext*)calloc(1, sizeof(KryonNativeContext));
    if (!ctx) {
        LOGE("Failed to allocate native context\n");
        return 0;
    }

    // Get JavaVM
    if ((*env)->GetJavaVM(env, &ctx->jvm) != JNI_OK) {
        LOGE("Failed to get JavaVM\n");
        free(ctx);
        return 0;
    }

    // Create global reference to activity
    ctx->activity_ref = (*env)->NewGlobalRef(env, activity);
    if (!ctx->activity_ref) {
        LOGE("Failed to create global reference to activity\n");
        free(ctx);
        return 0;
    }

    // Initialize platform (stub for now, as we don't have ANativeActivity)
    // TODO: Adapt android_platform to work with regular Activity
    LOGI("Platform initialized (stub)\n");

    ctx->initialized = true;
    ctx->dsl_context = NULL;  // DSL context created on demand

    LOGI("Native context created: %p\n", ctx);
    return (jlong)ctx;
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeShutdown(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeShutdown called\n");

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // Shutdown IR renderer
    if (ctx->ir_renderer) {
        android_ir_renderer_destroy(ctx->ir_renderer);
        ctx->ir_renderer = NULL;
    }

    // Shutdown renderer
    if (ctx->renderer) {
        android_renderer_shutdown(ctx->renderer);
        android_renderer_destroy(ctx->renderer);
        ctx->renderer = NULL;
    }

    // Release window
    if (ctx->window) {
        ANativeWindow_release(ctx->window);
        ctx->window = NULL;
    }

    // Delete global reference
    if (ctx->activity_ref) {
        (*env)->DeleteGlobalRef(env, ctx->activity_ref);
        ctx->activity_ref = NULL;
    }

    free(ctx);

    LOGI("Native context destroyed\n");
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeOnCreate(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeOnCreate called\n");
    // TODO: Initialize platform
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeOnStart(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeOnStart called\n");
    // TODO: Call platform lifecycle
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeOnResume(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeOnResume called\n");

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // GLSurfaceView.onResume() handles render loop restart
    // Just mark that we're resumed if needed for state tracking
    // (Currently no state tracking needed)
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeOnPause(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeOnPause called\n");

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // GLSurfaceView.onPause() handles render loop stop and context preservation
    // Just mark that we're paused if needed for state tracking
    // (Currently no state tracking needed)
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeOnStop(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeOnStop called\n");
    // TODO: Stop rendering
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeOnDestroy(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeOnDestroy called\n");
    // Cleanup is done in nativeShutdown
}

// ============================================================================
// GLSurfaceView.Renderer Callbacks
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeGLSurfaceCreated(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeGLSurfaceCreated called\n");

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // EGL context is already created by GLSurfaceView - we're on GL thread now
    // Just initialize GL resources (shaders, VAO, VBO, etc.)

    // Create IR renderer if first time
    if (!ctx->ir_renderer) {
        AndroidIRRendererConfig ir_config = {
            .window_width = 800,  // Will be updated in onSurfaceChanged
            .window_height = 600,
            .enable_animations = true,
            .enable_hot_reload = false,
            .hot_reload_watch_path = NULL
        };

        ctx->ir_renderer = android_ir_renderer_create(&ir_config);
        if (!ctx->ir_renderer) {
            LOGE("Failed to create IR renderer\n");
            return;
        }

        // Initialize the IR renderer (creates the AndroidRenderer)
        if (!android_ir_renderer_initialize(ctx->ir_renderer, ctx)) {
            LOGE("Failed to initialize IR renderer\n");
            return;
        }

        ctx->renderer = ctx->ir_renderer->renderer;  // Keep reference
        LOGI("IR Renderer created and initialized successfully\n");
    }

    // Initialize GL resources (no EGL setup - GLSurfaceView owns that)
    if (ctx->renderer) {
        if (!android_renderer_initialize_gl_only(ctx->renderer)) {
            LOGE("Failed to initialize GL resources\n");
            return;
        }
        LOGI("GL resources initialized successfully\n");

        // Register Android system fonts
        android_renderer_register_font(ctx->renderer, "Roboto", "/system/fonts/Roboto-Regular.ttf");
        android_renderer_register_font(ctx->renderer, "Roboto-Bold", "/system/fonts/Roboto-Bold.ttf");
        android_renderer_set_default_font(ctx->renderer, "Roboto", 16);
        LOGI("Fonts registered with renderer\n");
    }

    ctx->surface_ready = true;
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeGLSurfaceChanged(JNIEnv* env, jobject thiz,
                                                     jlong handle, jint width, jint height) {
    LOGI("nativeGLSurfaceChanged: %dx%d\n", width, height);

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // Get display density from Activity's Resources
    if (ctx->renderer && ctx->activity_ref) {
        // Get Resources from Activity
        jclass activity_class = (*env)->GetObjectClass(env, ctx->activity_ref);
        jmethodID get_resources = (*env)->GetMethodID(env, activity_class,
            "getResources", "()Landroid/content/res/Resources;");
        jobject resources = (*env)->CallObjectMethod(env, ctx->activity_ref, get_resources);

        if (resources) {
            // Get DisplayMetrics from Resources
            jclass resources_class = (*env)->GetObjectClass(env, resources);
            jmethodID get_metrics = (*env)->GetMethodID(env, resources_class,
                "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
            jobject metrics = (*env)->CallObjectMethod(env, resources, get_metrics);

            if (metrics) {
                // Get density field from DisplayMetrics
                jclass metrics_class = (*env)->GetObjectClass(env, metrics);
                jfieldID density_field = (*env)->GetFieldID(env, metrics_class, "density", "F");
                float density = (*env)->GetFloatField(env, metrics, density_field);

                LOGI("Setting renderer density: %.2f\n", density);
                android_renderer_set_density(ctx->renderer, density);

                (*env)->DeleteLocalRef(env, metrics_class);
                (*env)->DeleteLocalRef(env, metrics);
            }
            (*env)->DeleteLocalRef(env, resources_class);
            (*env)->DeleteLocalRef(env, resources);
        }
        (*env)->DeleteLocalRef(env, activity_class);
    }

    if (ctx->renderer) {
        android_renderer_resize(ctx->renderer, width, height);
    }

    // Trigger initial render if we have an IR renderer and component tree
    if (ctx->ir_renderer) {
        // Get the root component from global IR context
        extern IRContext* g_ir_context;
        LOGI("Checking IR context: g_ir_context=%p\n", g_ir_context);
        if (g_ir_context) {
            LOGI("IR context root: %p\n", g_ir_context->root);
            if (g_ir_context->root) {
                LOGI("Rendering component tree from IR context (type=%d, child_count=%d)\n",
                     g_ir_context->root->type, g_ir_context->root->child_count);

                // Get density for conversion (physical -> logical pixels)
                float density = android_renderer_get_density(ctx->renderer);

                // Set root dimensions in LOGICAL pixels (dp)
                // Rendering will convert back to physical by multiplying by density
                float logical_width = (float)width / density;
                float logical_height = (float)height / density;
                LOGI("Setting root dimensions: physical=%dx%d, density=%.2f, logical=%.1fx%.1f\n",
                     width, height, density, logical_width, logical_height);
                ir_set_width(g_ir_context->root, IR_DIMENSION_PX, logical_width);
                ir_set_height(g_ir_context->root, IR_DIMENSION_PX, logical_height);

                // Mark tree as needing re-layout
                android_ir_renderer_set_root(ctx->ir_renderer, g_ir_context->root);

                // Force re-layout with new dimensions
                AndroidIRRenderer* ir_renderer = (AndroidIRRenderer*)ctx->ir_renderer;
                ir_renderer->needs_relayout = true;

                android_ir_renderer_render(ctx->ir_renderer);
            } else {
                LOGI("IR context root is NULL\n");
            }
        } else {
            LOGI("Global IR context is NULL\n");
        }
    } else {
        LOGI("IR renderer is NULL\n");
    }
}

// Note: No nativeSurfaceDestroyed needed - GLSurfaceView handles context cleanup
// The GL context will be destroyed automatically when needed, and onSurfaceCreated
// will be called again to rebuild resources

// ============================================================================
// Input Events
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_com_kryon_KryonActivity_nativeTouchEvent(JNIEnv* env, jobject thiz,
                                               jlong handle, jobject motion_event) {
    if (handle == 0) return JNI_FALSE;

    // TODO: Convert MotionEvent to Kryon touch events
    // For now, just log
    LOGD("Touch event received\n");

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_kryon_KryonActivity_nativeKeyEvent(JNIEnv* env, jobject thiz,
                                             jlong handle, jobject key_event) {
    if (handle == 0) return JNI_FALSE;

    // TODO: Convert KeyEvent to Kryon key events
    LOGD("Key event received\n");

    return JNI_FALSE;
}

// ============================================================================
// File Loading
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_com_kryon_KryonActivity_nativeLoadFile(JNIEnv* env, jobject thiz,
                                             jlong handle, jstring path) {
    if (handle == 0) return JNI_FALSE;

    const char* path_str = (*env)->GetStringUTFChars(env, path, NULL);
    LOGI("Load file: %s\n", path_str);

    KryonNativeContext* ctx = (KryonNativeContext*)handle;
    jboolean result = android_ir_renderer_load_kir(ctx->ir_renderer, path_str) ? JNI_TRUE : JNI_FALSE;

    if (result) {
        LOGI("KIR file loaded successfully: %s\n", path_str);
    } else {
        LOGE("Failed to load KIR file: %s\n", path_str);
    }

    (*env)->ReleaseStringUTFChars(env, path, path_str);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_kryon_KryonActivity_nativeLoadSource(JNIEnv* env, jobject thiz,
                                               jlong handle, jstring path) {
    if (handle == 0) return JNI_FALSE;

    const char* path_str = (*env)->GetStringUTFChars(env, path, NULL);
    LOGI("Load source: %s\n", path_str);

    // TODO: Compile and execute .kry file
    jboolean result = JNI_FALSE;

    (*env)->ReleaseStringUTFChars(env, path, path_str);
    return result;
}

// ============================================================================
// State Management
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_com_kryon_KryonActivity_nativeSetState(JNIEnv* env, jobject thiz,
                                             jlong handle, jstring key, jstring value) {
    if (handle == 0) return JNI_FALSE;

    const char* key_str = (*env)->GetStringUTFChars(env, key, NULL);
    const char* value_str = (*env)->GetStringUTFChars(env, value, NULL);

    LOGD("Set state: %s = %s\n", key_str, value_str);

    // TODO: Set state in Kryon runtime

    (*env)->ReleaseStringUTFChars(env, key, key_str);
    (*env)->ReleaseStringUTFChars(env, value, value_str);

    return JNI_TRUE;
}

JNIEXPORT jstring JNICALL
Java_com_kryon_KryonActivity_nativeGetState(JNIEnv* env, jobject thiz,
                                             jlong handle, jstring key) {
    if (handle == 0) return NULL;

    const char* key_str = (*env)->GetStringUTFChars(env, key, NULL);

    LOGD("Get state: %s\n", key_str);

    // TODO: Get state from Kryon runtime
    jstring result = NULL;

    (*env)->ReleaseStringUTFChars(env, key, key_str);

    return result;
}

// ============================================================================
// Performance
// ============================================================================

JNIEXPORT jfloat JNICALL
Java_com_kryon_KryonActivity_nativeGetFPS(JNIEnv* env, jobject thiz, jlong handle) {
    if (handle == 0) return 0.0f;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    if (ctx->renderer) {
        return android_renderer_get_fps(ctx->renderer);
    }

    return 0.0f;
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeGLRender(JNIEnv* env, jobject thiz, jlong handle) {
    static int jni_render_count = 0;
    jni_render_count++;

    if (jni_render_count % 60 == 0) {
        LOGI("JNI nativeGLRender called, count=%d", jni_render_count);
    }

    if (handle == 0) {
        LOGE("nativeGLRender: handle is 0");
        return;
    }

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // Render the IR component tree
    if (ctx->ir_renderer) {
        android_ir_renderer_render(ctx->ir_renderer);
    } else {
        if (jni_render_count % 60 == 0) {
            LOGE("nativeGLRender: ctx->ir_renderer is NULL");
        }
    }
}
