#include "android_host.h"

#include "android_surface.h"
#include "platform.h"
#include "theme.h"
#include "ui_controls.h"
#include "ui_dpi.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if ANDROID_BUILD
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>

extern struct android_app *GetAndroidApp(void);

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x10060000
#endif

#define LOG_TAG "KRYON_ANDROID_HOST"

static KryMutex g_android_host_mutex = KRY_MUTEX_INIT;
static float g_device_density;

static int
scaled_inset(int java_px, float density)
{
    if(density <= 0.0f)
        density = 1.0f;
    return (int)(java_px / density + 0.5f);
}

static float
current_device_density(void)
{
    float density;

    KryMutexLock(&g_android_host_mutex);
    density = g_device_density;
    KryMutexUnlock(&g_android_host_mutex);
    return density;
}

static int
activity_env(JNIEnv **env_out, JavaVM **jvm_out, jobject *activity_out)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    int attached = 0;

    if(env_out == NULL || jvm_out == NULL || activity_out == NULL ||
       app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL)
        return -1;

    jvm = app->activity->vm;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return -1;
        attached = 1;
    }
    *env_out = env;
    *jvm_out = jvm;
    *activity_out = app->activity->clazz;
    return attached;
}

static void
activity_env_done(JNIEnv *env, JavaVM *jvm, int attached)
{
    if(env != NULL && (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached && jvm != NULL)
        (*jvm)->DetachCurrentThread(jvm);
}

static Color
color_from_argb(jint argb)
{
    Color color;

    color.a = (unsigned char)((argb >> 24) & 0xff);
    color.r = (unsigned char)((argb >> 16) & 0xff);
    color.g = (unsigned char)((argb >> 8) & 0xff);
    color.b = (unsigned char)(argb & 0xff);
    if(color.a == 0)
        color.a = 0xff;
    return color;
}

static int
call_boolean_method(const char *name)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;
    int result = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, name, "()Z");
    if(method == NULL)
        goto done;
    result = (*env)->CallBooleanMethod(env, activity, method) ? 1 : 0;

done:
    activity_env_done(env, jvm, attached);
    return result;
}

static int
call_key_boolean_method(const char *name, const char *key)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jkey = NULL;
    int attached;
    int result = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, name, "(Ljava/lang/String;)Z");
    if(method == NULL)
        goto done;
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    if(jkey == NULL)
        goto done;
    result = (*env)->CallBooleanMethod(env, activity, method, jkey) ? 1 : 0;

done:
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    activity_env_done(env, jvm, attached);
    return result;
}

static void
call_key_void_method(const char *name, const char *key)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jkey = NULL;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, name, "(Ljava/lang/String;)V");
    if(method == NULL)
        goto done;
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    if(jkey == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method, jkey);

done:
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    activity_env_done(env, jvm, attached);
}

void
AndroidHostInit(void)
{
    KryMutexLock(&g_android_host_mutex);
    g_device_density = 0.0f;
    KryMutexUnlock(&g_android_host_mutex);
}

void
AndroidHostApplySystemTheme(void)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jintArray array;
    jint values[9];
    jsize len;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;

    memset(values, 0, sizeof(values));
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonSystemThemeColors", "()[I");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG,
                            "kryonSystemThemeColors not found");
        goto done;
    }
    array = (jintArray)(*env)->CallObjectMethod(env, activity, method);
    if(array == NULL)
        goto done;
    len = (*env)->GetArrayLength(env, array);
    if(len < 9)
        goto done;
    (*env)->GetIntArrayRegion(env, array, 0, 9, values);
    SetSystemThemePalette("Android",
                          color_from_argb(values[1]),
                          color_from_argb(values[2]),
                          color_from_argb(values[3]),
                          color_from_argb(values[4]),
                          color_from_argb(values[5]),
                          color_from_argb(values[6]),
                          color_from_argb(values[7]),
                          color_from_argb(values[8]),
                          values[0] != 0,
                          1);

done:
    activity_env_done(env, jvm, attached);
}

void
AndroidHostSetSoftKeyboardVisible(int visible)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "soft keyboard %d: no activity env", visible);
        return;
    }

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class,
                                 "kryonSetSoftKeyboardVisible", "(Z)V");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG,
                            "kryonSetSoftKeyboardVisible not found");
        goto done;
    }

    (*env)->CallVoidMethod(env, activity, method, visible ? JNI_TRUE : JNI_FALSE);

done:
    activity_env_done(env, jvm, attached);
}

int
AndroidHostLeftReserved(void)
{
    KrySafeArea safe_area = GetAndroidSafeArea();

    if(!GetAndroidWindowInsets(NULL))
        return 0;
    return scaled_inset(safe_area.left, current_device_density());
}

int
AndroidHostTopReserved(void)
{
    KrySafeArea safe_area = GetAndroidSafeArea();

    if(!GetAndroidWindowInsets(NULL))
        return 28;
    return scaled_inset(safe_area.top, current_device_density());
}

int
AndroidHostRightReserved(void)
{
    KrySafeArea safe_area = GetAndroidSafeArea();

    if(!GetAndroidWindowInsets(NULL))
        return 0;
    return scaled_inset(safe_area.right, current_device_density());
}

int
AndroidHostBottomReserved(void)
{
    AndroidWindowInsets insets;
    KrySafeArea safe_area = GetAndroidSafeArea();
    int ready = GetAndroidWindowInsets(&insets);
    int bottom = safe_area.bottom;

    if(!ready)
        return 48;
    if(insets.ime_bottom > bottom)
        bottom = insets.ime_bottom;
    return scaled_inset(bottom, current_device_density());
}

int
AndroidSecureStoreBiometricAvailable(void)
{
    return call_boolean_method("kryonSecureStoreBiometricAvailable");
}

int
AndroidSecureStoreBiometricSetupRequired(void)
{
    return call_boolean_method("kryonSecureStoreBiometricSetupRequired");
}

int
AndroidSecureStoreHasSecret(const char *key)
{
    return call_key_boolean_method("kryonSecureStoreHasSecret", key);
}

int
AndroidSecureStoreSecretUsesBiometric(const char *key)
{
    return call_key_boolean_method("kryonSecureStoreSecretUsesBiometric", key);
}

void
AndroidSecureStoreSaveSecret(const char *key, const char *secret,
                             int require_biometric, const char *label)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jkey = NULL;
    jstring jsecret = NULL;
    jstring jlabel = NULL;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonSecureStoreSaveSecret",
                                 "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;)V");
    if(method == NULL)
        goto done;
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    jsecret = (*env)->NewStringUTF(env, secret != NULL ? secret : "");
    jlabel = (*env)->NewStringUTF(env, label != NULL ? label : "");
    if(jkey == NULL || jsecret == NULL || jlabel == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method, jkey, jsecret,
                           require_biometric ? JNI_TRUE : JNI_FALSE, jlabel);

done:
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    if(jsecret != NULL)
        (*env)->DeleteLocalRef(env, jsecret);
    if(jlabel != NULL)
        (*env)->DeleteLocalRef(env, jlabel);
    activity_env_done(env, jvm, attached);
}

void
AndroidSecureStoreUnlockSecret(const char *key, const char *label)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jkey = NULL;
    jstring jlabel = NULL;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonSecureStoreUnlockSecret",
                                 "(Ljava/lang/String;Ljava/lang/String;)V");
    if(method == NULL)
        goto done;
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    jlabel = (*env)->NewStringUTF(env, label != NULL ? label : "");
    if(jkey == NULL || jlabel == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method, jkey, jlabel);

done:
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    if(jlabel != NULL)
        (*env)->DeleteLocalRef(env, jlabel);
    activity_env_done(env, jvm, attached);
}

void
AndroidSecureStoreClearSecret(const char *key)
{
    call_key_void_method("kryonSecureStoreClearSecret", key);
}

int
AndroidSecureStoreStatus(const char *key)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jkey = NULL;
    int attached;
    int result = ANDROID_SECURE_STORE_IDLE;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return ANDROID_SECURE_STORE_IDLE;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonSecureStoreStatus",
                                 "(Ljava/lang/String;)I");
    if(method == NULL)
        goto done;
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    if(jkey == NULL)
        goto done;
    result = (int)(*env)->CallIntMethod(env, activity, method, jkey);

done:
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    activity_env_done(env, jvm, attached);
    return result;
}

int
AndroidSecureStoreTakeResult(const char *key, char *out, int out_size)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jkey = NULL;
    jstring result = NULL;
    const char *chars;
    int attached;
    int status;

    if(out != NULL && out_size > 0)
        out[0] = '\0';
    status = AndroidSecureStoreStatus(key);
    if(status != ANDROID_SECURE_STORE_OK &&
       status != ANDROID_SECURE_STORE_ERROR)
        return status;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return ANDROID_SECURE_STORE_IDLE;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonSecureStoreTakeResult",
                                 "(Ljava/lang/String;)Ljava/lang/String;");
    if(method == NULL)
        goto done;
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    if(jkey == NULL)
        goto done;
    result = (jstring)(*env)->CallObjectMethod(env, activity, method, jkey);
    if(result == NULL)
        goto done;
    chars = (*env)->GetStringUTFChars(env, result, NULL);
    if(chars != NULL) {
        if(out != NULL && out_size > 0)
            snprintf(out, (size_t)out_size, "%s", chars);
        (*env)->ReleaseStringUTFChars(env, result, chars);
    }

done:
    if(result != NULL)
        (*env)->DeleteLocalRef(env, result);
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    activity_env_done(env, jvm, attached);
    return status;
}

JNIEXPORT void JNICALL
Java_com_kryonlabs_kryon_KryonActivity_nativeSetInsets(JNIEnv *env, jobject thiz,
                                                       jint system_left, jint system_top,
                                                       jint system_right, jint system_bottom,
                                                       jint ime_bottom,
                                                       jint cutout_left, jint cutout_top,
                                                       jint cutout_right, jint cutout_bottom)
{
    (void)env;
    (void)thiz;

    SetAndroidWindowInsets(system_left, system_top, system_right, system_bottom,
                           ime_bottom,
                           cutout_left, cutout_top, cutout_right, cutout_bottom);
}

JNIEXPORT void JNICALL
Java_com_kryonlabs_kryon_KryonActivity_nativeSetDeviceDensity(JNIEnv *env,
                                                              jobject thiz,
                                                              jfloat density)
{
    (void)env;
    (void)thiz;

    if(density <= 0.0f)
        return;
    KryMutexLock(&g_android_host_mutex);
    g_device_density = density;
    KryMutexUnlock(&g_android_host_mutex);
    SetUIDeviceDensity(density);
}

JNIEXPORT void JNICALL
Java_com_kryonlabs_kryon_KryonActivity_nativeTextInputCommit(JNIEnv *env,
                                                             jobject thiz,
                                                             jint codepoint)
{
    (void)env;
    (void)thiz;
    QueueTextInputCodepoint((int)codepoint);
}

JNIEXPORT void JNICALL
Java_com_kryonlabs_kryon_KryonActivity_nativeTextInputBackspace(JNIEnv *env,
                                                                jobject thiz)
{
    (void)env;
    (void)thiz;
    QueueTextInputBackspace();
}

JNIEXPORT void JNICALL
Java_com_kryonlabs_kryon_KryonActivity_nativeTextInputEnter(JNIEnv *env,
                                                            jobject thiz)
{
    (void)env;
    (void)thiz;
    QueueTextInputEnter();
}

#else /* !ANDROID_BUILD */

void AndroidHostInit(void) {}
void AndroidHostApplySystemTheme(void) {}
void AndroidHostSetSoftKeyboardVisible(int visible) { (void)visible; }
int AndroidHostLeftReserved(void) { return 0; }
int AndroidHostTopReserved(void) { return 0; }
int AndroidHostRightReserved(void) { return 0; }
int AndroidHostBottomReserved(void) { return 0; }
int AndroidSecureStoreBiometricAvailable(void) { return 0; }
int AndroidSecureStoreBiometricSetupRequired(void) { return 0; }
int AndroidSecureStoreHasSecret(const char *key) { (void)key; return 0; }
int AndroidSecureStoreSecretUsesBiometric(const char *key) { (void)key; return 0; }
void AndroidSecureStoreSaveSecret(const char *key, const char *secret,
                                  int require_biometric, const char *label)
{
    (void)key;
    (void)secret;
    (void)require_biometric;
    (void)label;
}
void AndroidSecureStoreUnlockSecret(const char *key, const char *label)
{
    (void)key;
    (void)label;
}
void AndroidSecureStoreClearSecret(const char *key) { (void)key; }
int AndroidSecureStoreStatus(const char *key)
{
    (void)key;
    return ANDROID_SECURE_STORE_IDLE;
}
int AndroidSecureStoreTakeResult(const char *key, char *out, int out_size)
{
    (void)key;
    if(out != NULL && out_size > 0)
        out[0] = '\0';
    return ANDROID_SECURE_STORE_IDLE;
}

#endif
