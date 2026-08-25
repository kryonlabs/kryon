#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "kry_uri.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
#include <emscripten.h>
#elif defined(__ANDROID__) || defined(PLATFORM_ANDROID) || defined(ANDROID)
#include <android_native_app_glue.h>
#include <jni.h>
extern struct android_app *GetAndroidApp(void);
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

#if defined(__ANDROID__) || defined(PLATFORM_ANDROID) || defined(ANDROID)
#define KRY_OPEN_URI_ANDROID 1
#else
#define KRY_OPEN_URI_ANDROID 0
#endif

static int
kry_uri_valid(const char *uri)
{
    return uri != NULL && uri[0] != '\0';
}

static int
kry_open_uri_test_capture(const char *uri)
{
#if !KRY_OPEN_URI_ANDROID && !defined(__EMSCRIPTEN__) && !defined(PLATFORM_WEB)
    const char *path = getenv("KRYON_TEST_OPEN_URI_CAPTURE");
    FILE *f;

    if(path == NULL || path[0] == '\0')
        return 0;
    f = fopen(path, "wb");
    if(f == NULL)
        return 0;
    fputs(uri, f);
    fclose(f);
    return 1;
#else
    (void)uri;
    return 0;
#endif
}

#if KRY_OPEN_URI_ANDROID

static int
kry_android_get_env(struct android_app **out_app, JNIEnv **out_env, int *attached)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *vm;

    if(out_app != NULL)
        *out_app = app;
    if(out_env != NULL)
        *out_env = NULL;
    if(attached != NULL)
        *attached = 0;
    if(app == NULL || app->activity == NULL || app->activity->vm == NULL)
        return 0;
    vm = app->activity->vm;
    if((*vm)->GetEnv(vm, (void **)out_env, JNI_VERSION_1_6) == JNI_OK)
        return *out_env != NULL;
    if((*vm)->AttachCurrentThread(vm, out_env, NULL) == JNI_OK && *out_env != NULL) {
        if(attached != NULL)
            *attached = 1;
        return 1;
    }
    return 0;
}

static void
kry_android_done(struct android_app *app, int attached)
{
    if(attached && app != NULL && app->activity != NULL && app->activity->vm != NULL)
        (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
}

static int
kry_android_clear_exception(JNIEnv *env)
{
    if(env != NULL && (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return 1;
    }
    return 0;
}

static jobject
kry_android_new_uri_intent(JNIEnv *env, const char *uri)
{
    jclass uri_class = NULL;
    jclass intent_class = NULL;
    jstring uri_text = NULL;
    jstring action_view = NULL;
    jstring category_browsable = NULL;
    jobject uri_obj = NULL;
    jobject intent = NULL;
    jmethodID parse = NULL;
    jmethodID ctor = NULL;
    jmethodID add_category = NULL;
    jmethodID add_flags = NULL;
    jfieldID action_view_id = NULL;
    jfieldID category_browsable_id = NULL;
    jfieldID new_task_id = NULL;
    jint new_task = 0;

    uri_text = (*env)->NewStringUTF(env, uri);
    uri_class = (*env)->FindClass(env, "android/net/Uri");
    if(uri_text == NULL || uri_class == NULL || kry_android_clear_exception(env))
        return NULL;
    parse = (*env)->GetStaticMethodID(env, uri_class, "parse",
                                      "(Ljava/lang/String;)Landroid/net/Uri;");
    if(parse == NULL || kry_android_clear_exception(env))
        return NULL;
    uri_obj = (*env)->CallStaticObjectMethod(env, uri_class, parse, uri_text);
    if(uri_obj == NULL || kry_android_clear_exception(env))
        return NULL;

    intent_class = (*env)->FindClass(env, "android/content/Intent");
    if(intent_class == NULL || kry_android_clear_exception(env))
        return NULL;
    action_view_id = (*env)->GetStaticFieldID(env, intent_class, "ACTION_VIEW",
                                              "Ljava/lang/String;");
    category_browsable_id = (*env)->GetStaticFieldID(env, intent_class,
                                                     "CATEGORY_BROWSABLE",
                                                     "Ljava/lang/String;");
    new_task_id = (*env)->GetStaticFieldID(env, intent_class,
                                           "FLAG_ACTIVITY_NEW_TASK", "I");
    ctor = (*env)->GetMethodID(env, intent_class, "<init>",
                               "(Ljava/lang/String;Landroid/net/Uri;)V");
    add_category = (*env)->GetMethodID(env, intent_class, "addCategory",
                                       "(Ljava/lang/String;)Landroid/content/Intent;");
    add_flags = (*env)->GetMethodID(env, intent_class, "addFlags",
                                    "(I)Landroid/content/Intent;");
    if(action_view_id == NULL || category_browsable_id == NULL || new_task_id == NULL ||
       ctor == NULL || add_category == NULL || add_flags == NULL ||
       kry_android_clear_exception(env))
        return NULL;

    action_view = (jstring)(*env)->GetStaticObjectField(env, intent_class,
                                                        action_view_id);
    category_browsable = (jstring)(*env)->GetStaticObjectField(
        env, intent_class, category_browsable_id);
    new_task = (*env)->GetStaticIntField(env, intent_class, new_task_id);
    if(action_view == NULL || category_browsable == NULL ||
       kry_android_clear_exception(env))
        return NULL;

    intent = (*env)->NewObject(env, intent_class, ctor, action_view, uri_obj);
    if(intent == NULL || kry_android_clear_exception(env))
        return NULL;
    (*env)->CallObjectMethod(env, intent, add_category, category_browsable);
    if(kry_android_clear_exception(env))
        return NULL;
    (*env)->CallObjectMethod(env, intent, add_flags, new_task);
    if(kry_android_clear_exception(env))
        return NULL;
    return intent;
}

static int
kry_android_can_open_uri(JNIEnv *env, jobject activity, jobject intent)
{
    jclass context_class = NULL;
    jclass package_manager_class = NULL;
    jobject package_manager = NULL;
    jobject resolved = NULL;
    jmethodID get_package_manager = NULL;
    jmethodID resolve_activity = NULL;

    context_class = (*env)->FindClass(env, "android/content/Context");
    if(context_class == NULL || kry_android_clear_exception(env))
        return 0;
    get_package_manager = (*env)->GetMethodID(
        env, context_class, "getPackageManager",
        "()Landroid/content/pm/PackageManager;");
    if(get_package_manager == NULL || kry_android_clear_exception(env))
        return 0;
    package_manager = (*env)->CallObjectMethod(env, activity, get_package_manager);
    if(package_manager == NULL || kry_android_clear_exception(env))
        return 0;

    package_manager_class = (*env)->FindClass(env,
                                              "android/content/pm/PackageManager");
    if(package_manager_class == NULL || kry_android_clear_exception(env))
        return 0;
    resolve_activity = (*env)->GetMethodID(
        env, package_manager_class, "resolveActivity",
        "(Landroid/content/Intent;I)Landroid/content/pm/ResolveInfo;");
    if(resolve_activity == NULL || kry_android_clear_exception(env))
        return 0;
    resolved = (*env)->CallObjectMethod(env, package_manager, resolve_activity,
                                        intent, 0);
    if(kry_android_clear_exception(env))
        return 0;
    return resolved != NULL;
}

#endif

int
CanOpenURI(const char *uri)
{
    if(!kry_uri_valid(uri))
        return 0;

#if KRY_OPEN_URI_ANDROID
    struct android_app *app = NULL;
    JNIEnv *env = NULL;
    jobject intent = NULL;
    int attached = 0;
    int ok = 0;

    if(!kry_android_get_env(&app, &env, &attached))
        return 0;
    intent = kry_android_new_uri_intent(env, uri);
    if(intent != NULL)
        ok = kry_android_can_open_uri(env, app->activity->clazz, intent);
    kry_android_done(app, attached);
    return ok;
#elif defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
    return strncmp(uri, "http://", 7) == 0 || strncmp(uri, "https://", 8) == 0;
#else
    return 1;
#endif
}

int
OpenURI(const char *uri)
{
    if(!kry_uri_valid(uri))
        return 0;
    if(kry_open_uri_test_capture(uri))
        return 1;

#if KRY_OPEN_URI_ANDROID
    struct android_app *app = NULL;
    JNIEnv *env = NULL;
    jobject intent = NULL;
    jclass activity_class = NULL;
    jmethodID start_activity = NULL;
    int attached = 0;
    int ok = 0;

    if(!kry_android_get_env(&app, &env, &attached))
        return 0;
    intent = kry_android_new_uri_intent(env, uri);
    if(intent != NULL && kry_android_can_open_uri(env, app->activity->clazz, intent)) {
        activity_class = (*env)->FindClass(env, "android/app/Activity");
        if(activity_class != NULL && !kry_android_clear_exception(env)) {
            start_activity = (*env)->GetMethodID(env, activity_class,
                                                 "startActivity",
                                                 "(Landroid/content/Intent;)V");
            if(start_activity != NULL && !kry_android_clear_exception(env)) {
                (*env)->CallVoidMethod(env, app->activity->clazz,
                                       start_activity, intent);
                ok = !kry_android_clear_exception(env);
            }
        }
    }
    kry_android_done(app, attached);
    return ok;
#elif defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
    return EM_ASM_INT({
        var uri = UTF8ToString($0);
        var http = 'http:' + '/' + '/';
        var https = 'https:' + '/' + '/';
        if (uri.indexOf(http) !== 0 && uri.indexOf(https) !== 0) return 0;
        try {
            var link = document.createElement('a');
            link.href = uri;
            link.target = '_blank';
            link.rel = 'noopener';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            return 1;
        } catch (e) {
            return 0;
        }
    }, uri);
#elif defined(_WIN32)
    return (INT_PTR)ShellExecuteA(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL) > 32;
#elif defined(KRYON_NATIVE_PLAN9)
    switch(rfork(RFPROC|RFFDG|RFENVG|RFNOTEG|RFNOWAIT)) {
    case -1:
        return 0;
    case 0:
        execl("/bin/plumb", "plumb", (char *)uri, nil);
        exits("exec");
    default:
        return 1;
    }
#elif defined(__APPLE__)
    pid_t pid = fork();
    if(pid < 0)
        return 0;
    if(pid == 0) {
        execlp("open", "open", uri, (char *)NULL);
        _exit(127);
    }
    return 1;
#else
    pid_t pid = fork();
    if(pid < 0)
        return 0;
    if(pid == 0) {
        execlp("xdg-open", "xdg-open", uri, (char *)NULL);
        _exit(127);
    }
    return 1;
#endif
}

#ifndef PLATFORM_ANDROID
void OpenURL(const char *url)
{
    (void)OpenURI(url);
}
#endif
