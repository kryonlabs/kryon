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
    // TODO: Resume rendering
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeOnPause(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeOnPause called\n");
    // TODO: Pause rendering
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
// Surface Callbacks
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeSurfaceCreated(JNIEnv* env, jobject thiz,
                                                   jlong handle, jobject surface) {
    LOGI("nativeSurfaceCreated called\n");

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // Get native window from Surface
    ctx->window = ANativeWindow_fromSurface(env, surface);
    if (!ctx->window) {
        LOGE("Failed to get native window from surface\n");
        return;
    }

    LOGI("Native window acquired: %p\n", ctx->window);

    // Create IR renderer (which creates its own AndroidRenderer internally)
    if (!ctx->ir_renderer) {
        AndroidIRRendererConfig ir_config = {
            .window_width = ANativeWindow_getWidth(ctx->window),
            .window_height = ANativeWindow_getHeight(ctx->window),
            .enable_animations = true,
            .enable_hot_reload = false,
            .hot_reload_watch_path = NULL
        };

        ctx->ir_renderer = android_ir_renderer_create(&ir_config);
        if (!ctx->ir_renderer) {
            LOGE("Failed to create IR renderer\n");
            return;
        }

        android_ir_renderer_initialize(ctx->ir_renderer, ctx->window);
        LOGI("IR Renderer initialized successfully\n");

        // Now initialize the AndroidRenderer that the IR renderer created
        if (ctx->ir_renderer->renderer) {
            if (!android_renderer_initialize(ctx->ir_renderer->renderer, ctx->window)) {
                LOGE("Failed to initialize AndroidRenderer\n");
                return;
            }
            LOGI("AndroidRenderer initialized successfully\n");
            ctx->renderer = ctx->ir_renderer->renderer;  // Keep reference for cleanup

            // Register Android system fonts directly with the renderer
            android_renderer_register_font(ctx->renderer, "Roboto", "/system/fonts/Roboto-Regular.ttf");
            android_renderer_register_font(ctx->renderer, "Roboto-Bold", "/system/fonts/Roboto-Bold.ttf");
            android_renderer_set_default_font(ctx->renderer, "Roboto", 16);
            LOGI("Fonts registered with renderer\n");
        } else {
            LOGE("IR renderer has no AndroidRenderer!\n");
            return;
        }
    }

    ctx->surface_ready = true;
}

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeSurfaceChanged(JNIEnv* env, jobject thiz,
                                                   jlong handle, jint width, jint height) {
    LOGI("nativeSurfaceChanged: %dx%d\n", width, height);

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

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

                // Set root dimensions to fill screen
                LOGI("Setting root dimensions to %dx%d\n", width, height);
                ir_set_width(g_ir_context->root, IR_DIMENSION_PX, (float)width);
                ir_set_height(g_ir_context->root, IR_DIMENSION_PX, (float)height);

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

JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeSurfaceDestroyed(JNIEnv* env, jobject thiz, jlong handle) {
    LOGI("nativeSurfaceDestroyed called\n");

    if (handle == 0) return;

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    ctx->surface_ready = false;

    // Shutdown renderer (but don't destroy - that's done in nativeShutdown)
    if (ctx->renderer) {
        android_renderer_shutdown(ctx->renderer);
        // Don't call android_renderer_destroy here - it will be called in nativeShutdown
        // via android_ir_renderer_destroy
    }

    // Release window
    if (ctx->window) {
        ANativeWindow_release(ctx->window);
        ctx->window = NULL;
    }
}

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
Java_com_kryon_KryonActivity_nativeRender(JNIEnv* env, jobject thiz, jlong handle) {
    static int jni_render_count = 0;
    jni_render_count++;

    if (jni_render_count % 60 == 0) {
        LOGI("JNI nativeRender called, count=%d", jni_render_count);
    }

    if (handle == 0) {
        LOGE("nativeRender: handle is 0");
        return;
    }

    KryonNativeContext* ctx = (KryonNativeContext*)handle;

    // Render the IR component tree
    if (ctx->ir_renderer) {
        android_ir_renderer_render(ctx->ir_renderer);
    } else {
        if (jni_render_count % 60 == 0) {
            LOGE("nativeRender: ctx->ir_renderer is NULL");
        }
    }
}

// ============================================================================
// Font Registration
// ============================================================================

JNIEXPORT void JNICALL
Java_com_kryon_temp_MainActivity_nativeRegisterFont(JNIEnv* env, jobject thiz,
                                                     jstring font_name, jstring font_path) {
    const char* name = (*env)->GetStringUTFChars(env, font_name, NULL);
    const char* path = (*env)->GetStringUTFChars(env, font_path, NULL);

    LOGI("Registering font: %s -> %s", name, path);

    // Get the global IR context to access the renderer
    extern IRContext* g_ir_context;

    if (g_ir_context) {
        // Try to get renderer from global IR context first
        AndroidRenderer* renderer = NULL;

        // The IR context doesn't directly have a renderer, we need to get it from somewhere
        // Let me check if we have access to it through the activity context
        // For now, we'll use a global renderer reference
        extern AndroidRenderer* g_android_renderer;

        if (g_android_renderer) {
            android_renderer_register_font(g_android_renderer, name, path);
            android_renderer_set_default_font(g_android_renderer, name, 16);
            LOGI("Font registered successfully: %s", name);
        } else {
            LOGE("Global renderer not available for font registration");
        }
    } else {
        LOGE("Global IR context not available for font registration");
    }

    (*env)->ReleaseStringUTFChars(env, font_name, name);
    (*env)->ReleaseStringUTFChars(env, font_path, path);
}
