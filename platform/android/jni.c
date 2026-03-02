/*
 * Android Platform Layer - JNI Bridge
 * C89 compliant
 *
 * JNI bridge between Kryon C code and Android Java/Kotlin
 */

#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define LOG_TAG "Kryon"

/*
 * JNI_OnLoad - called when library is loaded
 */
JNIEXPORT jint JNI_OnLoad(Java_vm *vm, void *reserved)
{
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Kryon native library loaded");
    return JNI_VERSION_1_6;
}

/*
 * Initialize graphics (OpenGL ES)
 */
JNIEXPORT jlong JNICALL
Java_com_kryon_KryonActivity_nativeInit(JNIEnv *env, jobject thiz,
                                              jint width, jint height)
{
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                         "Initializing Kryon graphics: %dx%d", width, height);

    /* TODO: Initialize OpenGL ES context */
    /* TODO: Create framebuffer */
    /* TODO: Initialize Marrow */

    return 0;  /* Return graphics context pointer */
}

/*
 * Render frame
 */
JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeRender(JNIEnv *env, jobject thiz,
                                               jlong context_ptr)
{
    /* TODO: Render frame to OpenGL ES surface */
    /* TODO: Read from Marrow's /dev/screen */
    /* TODO: Upload as texture */
}

/*
 * Cleanup
 */
JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeCleanup(JNIEnv *env, jobject thiz,
                                                 jlong context_ptr)
{
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Cleaning up Kryon");
    /* TODO: Cleanup OpenGL ES resources */
    /* TODO: Shutdown Marrow */
}

/*
 * Handle touch event
 */
JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeTouch(JNIEnv *env, jobject thiz,
                                             jlong context_ptr,
                                             jfloat x, jfloat y, jint action)
{
    /* TODO: Convert touch to mouse event */
    /* TODO: Send to Marrow's /dev/mouse */
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,
                         "Touch: x=%.2f, y=%.2f, action=%d", x, y, action);
}

/*
 * Handle key event
 */
JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeKey(JNIEnv *env, jobject thiz,
                                          jlong context_ptr,
                                          jint keycode, jint action)
{
    /* TODO: Convert Android keycode to scancode */
    /* TODO: Send to Marrow's /dev/kbd */
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,
                         "Key: keycode=%d, action=%d", keycode, action);
}

/*
 * Start Marrow server
 */
JNIEXPORT jlong JNICALL
Java_com_kryon_KryonActivity_nativeStartMarrow(JNIEnv *env, jobject thiz,
                                                     jint port)
{
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                         "Starting Marrow server on port %d", port);

    /* TODO: Initialize Marrow subsystems */
    /* TODO: Start TCP server */

    return 0;  /* Return Marrow instance pointer */
}

/*
 * Stop Marrow server
 */
JNIEXPORT void JNICALL
Java_com_kryon_KryonActivity_nativeStopMarrow(JNIEnv *env, jobject thiz,
                                                    jlong marrow_ptr)
{
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Stopping Marrow server");
    /* TODO: Stop Marrow server */
}
