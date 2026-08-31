#include "app_storage.h"
#include "kry_filesystem.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ANDROID_BUILD
#include <android_native_app_glue.h>
#include <jni.h>

extern struct android_app *GetAndroidApp(void);

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x10060000
#endif
#endif

static void
copy_text(char *dst, int dst_size, const char *src)
{
    size_t len;

    if(dst == NULL || dst_size <= 0)
        return;
    if(src == NULL)
        src = "";
    len = strlen(src);
    if(len >= (size_t)dst_size)
        len = (size_t)dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void
sanitize_token(char *dst, size_t dst_size, const char *src,
               const char *fallback)
{
    size_t written = 0;

    if(dst == NULL || dst_size == 0)
        return;
    if(src == NULL || src[0] == '\0')
        src = fallback != NULL ? fallback : "default";
    while(*src != '\0' && written + 1 < dst_size) {
        unsigned char ch = (unsigned char)*src++;

        if(isalnum(ch) || ch == '_' || ch == '-' || ch == '.')
            dst[written++] = (char)ch;
        else
            dst[written++] = '_';
    }
    if(written == 0 && dst_size > 1)
        dst[written++] = 'x';
    dst[written] = '\0';
}

#if ANDROID_BUILD
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
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK ||
           env == NULL)
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

static int
android_storage_has_key(const char *scope, const char *key)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jscope = NULL;
    jstring jkey = NULL;
    int attached;
    int found = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonAppStorageHasKey",
                                 "(Ljava/lang/String;Ljava/lang/String;)Z");
    if(method == NULL)
        goto done;
    jscope = (*env)->NewStringUTF(env, scope != NULL ? scope : "");
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    if(jscope == NULL || jkey == NULL)
        goto done;
    found = (*env)->CallBooleanMethod(env, activity, method, jscope, jkey) ? 1 : 0;

done:
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    if(jscope != NULL)
        (*env)->DeleteLocalRef(env, jscope);
    activity_env_done(env, jvm, attached);
    return found;
}

int
KryAppStorageGetString(const char *scope, const char *key,
                       const char *fallback, char *out, int out_size)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jscope = NULL;
    jstring jkey = NULL;
    jstring jfallback = NULL;
    jstring result = NULL;
    const char *chars;
    int attached;
    int found;

    copy_text(out, out_size, fallback);
    if(key == NULL || key[0] == '\0' || out == NULL || out_size <= 0)
        return 0;
    found = android_storage_has_key(scope, key);
    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonAppStorageGetString",
                                 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    if(method == NULL)
        goto done;
    jscope = (*env)->NewStringUTF(env, scope != NULL ? scope : "");
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    jfallback = (*env)->NewStringUTF(env, fallback != NULL ? fallback : "");
    if(jscope == NULL || jkey == NULL || jfallback == NULL)
        goto done;
    result = (jstring)(*env)->CallObjectMethod(env, activity, method,
                                               jscope, jkey, jfallback);
    if(result == NULL)
        goto done;
    chars = (*env)->GetStringUTFChars(env, result, NULL);
    if(chars != NULL) {
        copy_text(out, out_size, chars);
        (*env)->ReleaseStringUTFChars(env, result, chars);
    }

done:
    if(result != NULL)
        (*env)->DeleteLocalRef(env, result);
    if(jfallback != NULL)
        (*env)->DeleteLocalRef(env, jfallback);
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    if(jscope != NULL)
        (*env)->DeleteLocalRef(env, jscope);
    activity_env_done(env, jvm, attached);
    return found;
}

int
KryAppStorageSetString(const char *scope, const char *key, const char *value)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jscope = NULL;
    jstring jkey = NULL;
    jstring jvalue = NULL;
    int attached;
    int saved = 0;

    if(key == NULL || key[0] == '\0')
        return 0;
    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "kryonAppStorageSetString",
                                 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
    if(method == NULL)
        goto done;
    jscope = (*env)->NewStringUTF(env, scope != NULL ? scope : "");
    jkey = (*env)->NewStringUTF(env, key != NULL ? key : "");
    jvalue = (*env)->NewStringUTF(env, value != NULL ? value : "");
    if(jscope == NULL || jkey == NULL || jvalue == NULL)
        goto done;
    saved = (*env)->CallBooleanMethod(env, activity, method,
                                      jscope, jkey, jvalue) ? 1 : 0;

done:
    if(jvalue != NULL)
        (*env)->DeleteLocalRef(env, jvalue);
    if(jkey != NULL)
        (*env)->DeleteLocalRef(env, jkey);
    if(jscope != NULL)
        (*env)->DeleteLocalRef(env, jscope);
    activity_env_done(env, jvm, attached);
    return saved;
}
#else
static void
storage_path(char *dst, size_t dst_size, const char *scope, const char *key)
{
    char safe_scope[64];
    char safe_key[128];

    sanitize_token(safe_scope, sizeof(safe_scope), scope, "default");
    sanitize_token(safe_key, sizeof(safe_key), key, "value");
    snprintf(dst, dst_size, ".kryon_%s_%s.txt", safe_scope, safe_key);
}
#endif

const char *
KryAppDataRoot(const char *app_id)
{
    static char root[512];
    const char *home;
    const char *xdg;
    char safe[64];
    size_t i;

    if(root[0] != '\0')
        return root;
    sanitize_token(safe, sizeof(safe), app_id, "app");
#if defined(_WIN32)
    {
        const char *local = getenv("LOCALAPPDATA");

        if(local != NULL && local[0] != '\0')
            snprintf(root, sizeof(root), "%s/%s", local, safe);
        else
            snprintf(root, sizeof(root), "%s", safe);
    }
#elif defined(KRYON_PLATFORM_PLAN9)
    home = getenv("home");
    if(home == NULL || home[0] == '\0')
        home = getenv("HOME");
    if(home != NULL && home[0] != '\0')
        snprintf(root, sizeof(root), "%s/.local/share/%s", home, safe);
    else
        snprintf(root, sizeof(root), ".local/%s", safe);
#else
    xdg = getenv("XDG_DATA_HOME");
    home = getenv("HOME");
    if(xdg != NULL && xdg[0] != '\0')
        snprintf(root, sizeof(root), "%s/%s", xdg, safe);
    else if(home != NULL && home[0] != '\0')
        snprintf(root, sizeof(root), "%s/.local/share/%s", home, safe);
    else
        snprintf(root, sizeof(root), ".local/%s", safe);
#endif
    for(i = 0; root[i] != '\0'; i++) {
        if(root[i] == '\\')
            root[i] = '/';
    }
    if(kry_fs_mkdir_p(root) != 0)
        root[0] = '\0';
    return root;
}

int
KryAppStorageGetString(const char *scope, const char *key,
                       const char *fallback, char *out, int out_size)
{
    char path[256];
    FILE *f;
    size_t len;

    copy_text(out, out_size, fallback);
    if(key == NULL || key[0] == '\0' || out == NULL || out_size <= 0)
        return 0;
    storage_path(path, sizeof(path), scope, key);
    f = fopen(path, "rb");
    if(f == NULL)
        return 0;
    len = fread(out, 1, (size_t)out_size - 1, f);
    if(ferror(f)) {
        fclose(f);
        copy_text(out, out_size, fallback);
        return 0;
    }
    fclose(f);
    out[len] = '\0';
    return 1;
}

int
KryAppStorageSetString(const char *scope, const char *key, const char *value)
{
    char path[256];
    FILE *f;

    if(key == NULL || key[0] == '\0')
        return 0;
    storage_path(path, sizeof(path), scope, key);
    f = fopen(path, "wb");
    if(f == NULL)
        return 0;
    if(value != NULL && value[0] != '\0' &&
       fwrite(value, 1, strlen(value), f) != strlen(value)) {
        fclose(f);
        return 0;
    }
    return fclose(f) == 0;
}

int
KryAppStorageGetInt(const char *scope, const char *key, int fallback, int *out)
{
    char text[64];
    char *end;
    long value;
    int found;

    if(out == NULL)
        return 0;
    *out = fallback;
    found = KryAppStorageGetString(scope, key, "", text, sizeof(text));
    if(!found || text[0] == '\0')
        return 0;
    value = strtol(text, &end, 10);
    if(end == text || *end != '\0')
        return 0;
    *out = (int)value;
    return 1;
}

int
KryAppStorageSetInt(const char *scope, const char *key, int value)
{
    char text[64];

    snprintf(text, sizeof(text), "%d", value);
    return KryAppStorageSetString(scope, key, text);
}
