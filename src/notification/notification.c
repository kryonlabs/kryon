/*
 * Cross-platform user notifications. One file, one backend per platform:
 * Android via pure JNI (no Java-side code required), web via the browser
 * Notification API, desktop Linux via org.freedesktop.Notifications on the
 * session bus. Everything else compiles to an honest "unsupported" stub.
 */
#include "notification.h"

#include <stdio.h>
#include <string.h>
#if defined(__ANDROID__)
#include <stdint.h>
#endif

#if defined(__ANDROID__)

#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>

#define KRYON_NOTIFY_LOG_TAG "KRYON_NOTIFY"
#define KRYON_CHANNEL_DEFAULT "kryon.default"
#define KRYON_CHANNEL_LOW     "kryon.low"
#define KRYON_CHANNEL_HIGH    "kryon.high"

/* raylib's PLATFORM_ANDROID entry point (see raylib.h); declared here to
 * keep this file free of the full kryon umbrella. */
struct android_app *GetAndroidApp(void);

static struct android_app *notify_app(void)
{
    return GetAndroidApp();
}

static int jni_env(struct android_app *app, JNIEnv **out)
{
    JavaVM *vm = app->activity->vm;

    *out = NULL;
    if(vm == NULL)
        return 0;
    if((*vm)->GetEnv(vm, (void **)out, JNI_VERSION_1_6) != JNI_OK) {
        if((*vm)->AttachCurrentThread(vm, out, NULL) != JNI_OK || *out == NULL)
            return 0;
        return 2;   /* caller must detach */
    }
    return 1;
}

static void jni_done(JNIEnv *env, int attached)
{
    JavaVM *vm;

    if(env != NULL && (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached == 2 && env != NULL) {
        vm = NULL;
        (*env)->GetJavaVM(env, &vm);
        if(vm != NULL)
            (*vm)->DetachCurrentThread(vm);
    }
}

static int android_sdk_int(JNIEnv *env, jobject context)
{
    jclass version_class;
    jfieldID sdk_field;
    int attached = 0;
    struct android_app *app = notify_app();

    if(app == NULL)
        return 0;
    attached = jni_env(app, &env);
    if(attached == 0)
        return 0;
    version_class = (*env)->FindClass(env, "android/os/Build$VERSION");
    if(version_class == NULL) {
        jni_done(env, attached);
        return 0;
    }
    sdk_field = (*env)->GetStaticFieldID(env, version_class, "SDK_INT", "I");
    if(sdk_field == NULL) {
        jni_done(env, attached);
        return 0;
    }
    {
        int sdk = (*env)->GetStaticIntField(env, version_class, sdk_field);

        jni_done(env, attached);
        return sdk;
    }
}

static jobject notification_manager(JNIEnv *env, jobject context)
{
    jclass context_class;
    jmethodID get_service;
    jstring name;
    jobject manager;

    context_class = (*env)->GetObjectClass(env, context);
    if(context_class == NULL)
        return NULL;
    get_service = (*env)->GetMethodID(env, context_class, "getSystemService",
                                      "(Ljava/lang/String;)Ljava/lang/Object;");
    if(get_service == NULL)
        return NULL;
    name = (*env)->NewStringUTF(env, "notification");
    if(name == NULL)
        return NULL;
    manager = (*env)->CallObjectMethod(env, context, get_service, name);
    (*env)->DeleteLocalRef(env, name);
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return NULL;
    }
    return manager;    /* local ref, valid for the enclosing JNI scope */
}

static void ensure_channel(JNIEnv *env, jobject manager,
                           const char *channel_id, const char *name, int importance)
{
    jclass channel_class;
    jmethodID ctor;
    jmethodID create_ret;      /* API 30+: returns the channel */
    jmethodID create_void;     /* API 26-29: void */
    jstring jid;
    jstring jname;
    jobject channel;

    channel_class = (*env)->FindClass(env, "android/app/NotificationChannel");
    if(channel_class == NULL) {
        (*env)->ExceptionClear(env);
        return;
    }
    ctor = (*env)->GetMethodID(env, channel_class, "<init>",
                               "(Ljava/lang/String;Ljava/lang/CharSequence;I)V");
    create_ret = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, manager),
            "createNotificationChannel",
            "(Landroid/app/NotificationChannel;)Landroid/app/NotificationChannel;");
    if(create_ret == NULL) {
        (*env)->ExceptionClear(env);
        create_void = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, manager),
                "createNotificationChannel",
                "(Landroid/app/NotificationChannel;)V");
    } else {
        create_void = NULL;
    }
    if(ctor == NULL || (create_ret == NULL && create_void == NULL))
        return;
    jid = (*env)->NewStringUTF(env, channel_id);
    jname = (*env)->NewStringUTF(env, name);
    if(jid == NULL || jname == NULL)
        return;
    channel = (*env)->NewObject(env, channel_class, ctor, jid, jname,
                                (jint)importance);
    if(channel != NULL) {
        if(create_ret != NULL) {
            jobject made = (*env)->CallObjectMethod(env, manager, create_ret, channel);

            (*env)->DeleteLocalRef(env, made);
        } else {
            (*env)->CallVoidMethod(env, manager, create_void, channel);
        }
    }
    if((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
}

static const char *channel_for_priority(int priority)
{
    if(priority == NOTIFICATION_PRIORITY_LOW)
        return KRYON_CHANNEL_LOW;
    if(priority == NOTIFICATION_PRIORITY_HIGH)
        return KRYON_CHANNEL_HIGH;
    return KRYON_CHANNEL_DEFAULT;
}

int IsNotificationSupported(void)
{
    return notify_app() != NULL;
}

int IsNotificationPermissionGranted(void)
{
    struct android_app *app = notify_app();
    JNIEnv *env;
    int attached;
    jobject manager;
    jmethodID enabled;
    int granted = 1;

    if(app == NULL || app->activity == NULL)
        return 0;
    attached = jni_env(app, &env);
    if(attached == 0)
        return 0;
    if(android_sdk_int(env, app->activity->clazz) < 24)
        goto done;   /* pre-24 has no toggle: granted */
    manager = notification_manager(env, app->activity->clazz);
    if(manager == NULL)
        goto done;
    enabled = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, manager),
                                  "areNotificationsEnabled", "()Z");
    if(enabled == NULL) {
        (*env)->ExceptionClear(env);
        goto done;
    }
    granted = (*env)->CallBooleanMethod(env, manager, enabled) == JNI_TRUE;

done:
    jni_done(env, attached);
    return granted;
}

int RequestNotificationPermission(void)
{
    struct android_app *app = notify_app();
    JNIEnv *env;
    int attached;
    jclass activity_class;
    jmethodID request;
    jclass string_class;
    jobjectArray permissions;
    jstring permission;
    int sdk;

    if(app == NULL || app->activity == NULL)
        return 0;
    if(IsNotificationPermissionGranted())
        return 1;
    attached = jni_env(app, &env);
    if(attached == 0)
        return 0;
    sdk = android_sdk_int(env, app->activity->clazz);
    if(sdk < 33) {
        jni_done(env, attached);
        return 0;   /* toggled in system settings, not requestable */
    }
    activity_class = (*env)->GetObjectClass(env, app->activity->clazz);
    request = (*env)->GetMethodID(env, activity_class, "requestPermissions",
                                  "([Ljava/lang/String;I)V");
    if(request == NULL) {
        jni_done(env, attached);
        return 0;
    }
    string_class = (*env)->FindClass(env, "java/lang/String");
    permission = (*env)->NewStringUTF(env, "android.permission.POST_NOTIFICATIONS");
    permissions = (*env)->NewObjectArray(env, 1, string_class, permission);
    if(permissions != NULL) {
        (*env)->CallVoidMethod(env, app->activity->clazz, request, permissions,
                               (jint)0x6b72);
        if((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
    }
    jni_done(env, attached);
    return 0;   /* result arrives via the activity callback */
}

int SendNotificationEx(const char *title, const char *body,
                       const char *tag, int id, int priority)
{
    struct android_app *app = notify_app();
    JNIEnv *env;
    int attached;
    jobject context, manager;
    jclass builder_class;
    jmethodID ctor, set_title, set_text, set_auto_cancel, build;
    jmethodID notify_id, notify_tag;
    jobject builder, notification;
    jstring jtitle, jbody, jtag, jchannel;
    int sdk;
    int delivered = 0;

    if(app == NULL || app->activity == NULL ||
       app->activity->vm == NULL || app->activity->clazz == NULL)
        return 0;
    if(title == NULL || body == NULL)
        return 0;
    attached = jni_env(app, &env);
    if(attached == 0)
        return 0;
    context = app->activity->clazz;
    sdk = android_sdk_int(env, context);
    manager = notification_manager(env, context);
    if(manager == NULL)
        goto done;

    if(sdk >= 26) {
        ensure_channel(env, manager, KRYON_CHANNEL_DEFAULT,
                       "Notifications", 3 /* IMPORTANCE_DEFAULT */);
        ensure_channel(env, manager, KRYON_CHANNEL_LOW,
                       "Silent notifications", 2 /* IMPORTANCE_LOW */);
        ensure_channel(env, manager, KRYON_CHANNEL_HIGH,
                       "Alerts", 4 /* IMPORTANCE_HIGH */);
    }

    builder_class = (*env)->FindClass(env, "android/app/Notification$Builder");
    if(builder_class == NULL) {
        (*env)->ExceptionClear(env);
        goto done;
    }
    jchannel = (*env)->NewStringUTF(env, channel_for_priority(priority));
    if(sdk >= 26) {
        ctor = (*env)->GetMethodID(env, builder_class, "<init>",
                "(Landroid/content/Context;Ljava/lang/String;)V");
        builder = (*env)->NewObject(env, builder_class, ctor, context, jchannel);
    } else {
        ctor = (*env)->GetMethodID(env, builder_class, "<init>",
                                   "(Landroid/content/Context;)V");
        builder = (*env)->NewObject(env, builder_class, ctor, context);
    }
    if(builder == NULL || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        goto done;
    }
    if(sdk < 26) {
        /* legacy priorities: LOW -1, DEFAULT 0, HIGH 1 */
        jmethodID set_prio = (*env)->GetMethodID(env, builder_class,
                "setPriority", "(I)Landroid/app/Notification$Builder;");

        if(set_prio != NULL)
            (*env)->CallObjectMethod(env, builder, set_prio,
                    (jint)(priority == NOTIFICATION_PRIORITY_LOW ? -1 :
                           priority == NOTIFICATION_PRIORITY_HIGH ? 1 : 0));
        if((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
    }

    set_title = (*env)->GetMethodID(env, builder_class, "setContentTitle",
            "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
    set_text = (*env)->GetMethodID(env, builder_class, "setContentText",
            "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
    set_auto_cancel = (*env)->GetMethodID(env, builder_class, "setAutoCancel",
            "(Z)Landroid/app/Notification$Builder;");
    build = (*env)->GetMethodID(env, builder_class, "build",
                                "()Landroid/app/Notification;");
    jtitle = (*env)->NewStringUTF(env, title);
    jbody = (*env)->NewStringUTF(env, body);
    if(set_title != NULL)
        (*env)->CallObjectMethod(env, builder, set_title, jtitle);
    if(set_text != NULL)
        (*env)->CallObjectMethod(env, builder, set_text, jbody);
    if(set_auto_cancel != NULL)
        (*env)->CallObjectMethod(env, builder, set_auto_cancel, JNI_TRUE);
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        goto done;
    }
    notification = (*env)->CallObjectMethod(env, builder, build);
    if(notification == NULL || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        goto done;
    }

    {
        jclass manager_class = (*env)->GetObjectClass(env, manager);

        if(id <= 0)
            id = (int)(intptr_t)notification;   /* stable-ish auto id */
        notify_id = (*env)->GetMethodID(env, manager_class, "notify",
                "(ILandroid/app/Notification;)V");
        notify_tag = (*env)->GetMethodID(env, manager_class, "notify",
                "(Ljava/lang/String;ILandroid/app/Notification;)V");
        if(tag != NULL && tag[0] != '\0' && notify_tag != NULL) {
            jtag = (*env)->NewStringUTF(env, tag);
            (*env)->CallVoidMethod(env, manager, notify_tag, jtag, (jint)id,
                                   notification);
        } else if(notify_id != NULL) {
            (*env)->CallVoidMethod(env, manager, notify_id, (jint)id, notification);
        }
        if((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            __android_log_write(ANDROID_LOG_ERROR, KRYON_NOTIFY_LOG_TAG,
                                "notify() failed");
        } else {
            delivered = 1;
        }
    }

done:
    jni_done(env, attached);
    return delivered;
}

void CancelNotification(const char *tag, int id)
{
    struct android_app *app = notify_app();
    JNIEnv *env;
    int attached;
    jobject manager;
    jmethodID cancel;

    if(app == NULL || app->activity == NULL || id <= 0)
        return;
    attached = jni_env(app, &env);
    if(attached == 0)
        return;
    manager = notification_manager(env, app->activity->clazz);
    if(manager == NULL)
        goto done;
    if(tag != NULL && tag[0] != '\0') {
        jmethodID cancel_tag = (*env)->GetMethodID(env,
                (*env)->GetObjectClass(env, manager), "cancel",
                "(Ljava/lang/String;I)V");

        if(cancel_tag != NULL) {
            jstring jtag = (*env)->NewStringUTF(env, tag);

            (*env)->CallVoidMethod(env, manager, cancel_tag, jtag, (jint)id);
            if((*env)->ExceptionCheck(env))
                (*env)->ExceptionClear(env);
        }
    } else {
        cancel = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, manager),
                                     "cancel", "(I)V");
        if(cancel != NULL) {
            (*env)->CallVoidMethod(env, manager, cancel, (jint)id);
            if((*env)->ExceptionCheck(env))
                (*env)->ExceptionClear(env);
        }
    }

done:
    jni_done(env, attached);
}

void CancelAllNotifications(void)
{
    struct android_app *app = notify_app();
    JNIEnv *env;
    int attached;
    jobject manager;
    jmethodID cancel_all;

    if(app == NULL || app->activity == NULL)
        return;
    attached = jni_env(app, &env);
    if(attached == 0)
        return;
    manager = notification_manager(env, app->activity->clazz);
    if(manager == NULL)
        goto done;
    cancel_all = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, manager),
                                     "cancelAll", "()V");
    if(cancel_all != NULL) {
        (*env)->CallVoidMethod(env, manager, cancel_all);
        if((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
    }

done:
    jni_done(env, attached);
}

void SetNotificationAppName(const char *name)
{
    (void)name;   /* Android shows the app itself; no daemon-side name */
}

#elif defined(PLATFORM_WEB)

#include <emscripten.h>

EM_JS(int, kryon_web_notification_supported, (void), {
    return (typeof Notification !== 'undefined') ? 1 : 0;
});

EM_JS(int, kryon_web_permission_granted, (void), {
    if(typeof Notification === 'undefined')
        return 0;
    return Notification.permission === 'granted' ? 1 : 0;
});

EM_JS(int, kryon_web_request_permission, (void), {
    if(typeof Notification === 'undefined')
        return 0;
    if(Notification.permission === 'granted')
        return 1;
    if(Notification.permission === 'default')
        Notification.requestPermission().catch(function() {});
    return 0;
});

EM_JS(int, kryon_web_send, (const char *title, const char *body, const char *tag, int id), {
    if(typeof Notification === 'undefined' || Notification.permission !== 'granted')
        return 0;
    var t = UTF8ToString(title);
    var b = UTF8ToString(body);
    var g = UTF8ToString(tag);
    var key = (g ? g + ':' : '') + id;
    if(!Module.kryonNotifications)
        Module.kryonNotifications = {};
    try {
        var n = new Notification(t, { body: b, tag: g || undefined });
        Module.kryonNotifications[key] = n;
        var drop = function() { delete Module.kryonNotifications[key]; };
        n.onclick = function() { n.close(); drop(); };
        n.onclose = drop;
        n.onerror = drop;
        return 1;
    } catch(e) {
        return 0;
    }
});

EM_JS(void, kryon_web_cancel, (const char *tag, int id), {
    var g = UTF8ToString(tag);
    var key = (g ? g + ':' : '') + id;
    if(!Module.kryonNotifications)
        return;
    var n = Module.kryonNotifications[key];
    if(n) {
        delete Module.kryonNotifications[key];
        try { n.close(); } catch(e) {}
    }
});

EM_JS(void, kryon_web_cancel_all, (void), {
    if(!Module.kryonNotifications)
        return;
    for(var key in Module.kryonNotifications) {
        var n = Module.kryonNotifications[key];
        delete Module.kryonNotifications[key];
        try { n.close(); } catch(e) {}
    }
});

int IsNotificationSupported(void)
{
    return kryon_web_notification_supported();
}

int IsNotificationPermissionGranted(void)
{
    return kryon_web_permission_granted();
}

int RequestNotificationPermission(void)
{
    return kryon_web_request_permission();
}

int SendNotificationEx(const char *title, const char *body,
                       const char *tag, int id, int priority)
{
    (void)priority;   /* browsers have no priority concept */
    if(title == NULL || body == NULL)
        return 0;
    return kryon_web_send(title, body, tag != NULL ? tag : "", id);
}

void CancelNotification(const char *tag, int id)
{
    kryon_web_cancel(tag != NULL ? tag : "", id);
}

void CancelAllNotifications(void)
{
    kryon_web_cancel_all();
}

void SetNotificationAppName(const char *name)
{
    (void)name;   /* the browser labels notifications with the origin */
}

#elif defined(KRYON_NOTIFICATION_GDBUS)

#include <gio/gio.h>
#include <stdint.h>

#define KRYON_NOTIFICATIONS_BUS    "org.freedesktop.Notifications"
#define KRYON_NOTIFICATIONS_PATH   "/org/freedesktop/Notifications"
#define KRYON_NOTIFICATIONS_IFACE  "org.freedesktop.Notifications"
#define KRYON_NOTIFIED_MAX 64

static char g_notification_app_name[64] = "kryon";
/* Sent ids remembered so CancelAll can close them; the spec has no global
 * cancel. Plain ring, oldest entries fall off. */
static unsigned int g_notified[KRYON_NOTIFIED_MAX];
static int g_notified_count;

void SetNotificationAppName(const char *name)
{
    if(name == NULL || name[0] == '\0')
        return;
    snprintf(g_notification_app_name, sizeof(g_notification_app_name), "%s",
             name);
}

static GDBusConnection *notification_bus(void)
{
    static GDBusConnection *bus;

    if(bus == NULL)
        bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    return bus;
}

int IsNotificationSupported(void)
{
    return notification_bus() != NULL;
}

int IsNotificationPermissionGranted(void)
{
    return 1;   /* the desktop has no permission gate */
}

int RequestNotificationPermission(void)
{
    return 1;
}

int SendNotificationEx(const char *title, const char *body,
                       const char *tag, int id, int priority)
{
    GDBusConnection *bus = notification_bus();
    GVariant *hints;
    GVariant *result;
    GError *error = NULL;
    unsigned int replaces_id = id > 0 ? (unsigned int)id : 0;
    unsigned int urgency = priority == NOTIFICATION_PRIORITY_LOW ? 0 :
                           priority == NOTIFICATION_PRIORITY_HIGH ? 2 : 1;
    const char *tag_key = (tag != NULL && tag[0] != '\0') ? tag : "";
    int delivered = 0;

    (void)tag_key;
    if(bus == NULL || title == NULL || body == NULL)
        return 0;
    hints = g_variant_new_parsed("{"
            "'urgency': <%u>, "
            "'desktop-entry': <%s>"
            "}", urgency, g_notification_app_name);
    result = g_dbus_connection_call_sync(bus,
            KRYON_NOTIFICATIONS_BUS, KRYON_NOTIFICATIONS_PATH,
            KRYON_NOTIFICATIONS_IFACE, "Notify",
            g_variant_new("(susssasa{sv}i)",
                    g_notification_app_name,
                    replaces_id,
                    "",              /* app_icon: the desktop-entry wins */
                    title,
                    body,
                    NULL,            /* no actions */
                    NULL,
                    hints,
                    -1),             /* default expiry */
            G_VARIANT_TYPE("(u)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1, NULL, &error);
    if(result != NULL) {
        unsigned int daemon_id = 0;

        g_variant_get(result, "(u)", &daemon_id);
        g_variant_unref(result);
        delivered = 1;
        /* Remember the daemon id so Cancel can close it; when the caller
         * supplied its own id the daemon replaced that notification. */
        if(daemon_id != 0) {
            g_notified[g_notified_count % KRYON_NOTIFIED_MAX] = daemon_id;
            g_notified_count++;
        }
    } else {
        g_error_free(error);
    }
    return delivered;
}

void CancelNotification(const char *tag, int id)
{
    GDBusConnection *bus = notification_bus();

    (void)tag;
    if(bus == NULL || id <= 0)
        return;
    /* The daemon addresses notifications by the id it returned, which the
     * caller does not know when using NOTIFICATION_ID_AUTO; cancel works
     * for notifications whose id the caller picked and passed through. */
    g_dbus_connection_call_sync(bus,
            KRYON_NOTIFICATIONS_BUS, KRYON_NOTIFICATIONS_PATH,
            KRYON_NOTIFICATIONS_IFACE, "CloseNotification",
            g_variant_new("(u)", (unsigned int)id), NULL,
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
}

void CancelAllNotifications(void)
{
    GDBusConnection *bus = notification_bus();
    int i;
    int count = g_notified_count < KRYON_NOTIFIED_MAX ? g_notified_count
                                                      : KRYON_NOTIFIED_MAX;

    if(bus == NULL)
        return;
    for(i = 0; i < count; i++) {
        g_dbus_connection_call_sync(bus,
                KRYON_NOTIFICATIONS_BUS, KRYON_NOTIFICATIONS_PATH,
                KRYON_NOTIFICATIONS_IFACE, "CloseNotification",
                g_variant_new("(u)", g_notified[i]), NULL,
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
    }
    g_notified_count = 0;
}

#else /* stub backend */

static char g_notification_app_name[64] = "kryon";

void SetNotificationAppName(const char *name)
{
    if(name == NULL || name[0] == '\0')
        return;
    snprintf(g_notification_app_name, sizeof(g_notification_app_name), "%s",
             name);
}

int IsNotificationSupported(void)
{
    return 0;
}

int IsNotificationPermissionGranted(void)
{
    return 1;
}

int RequestNotificationPermission(void)
{
    return 1;
}

int SendNotificationEx(const char *title, const char *body,
                       const char *tag, int id, int priority)
{
    (void)title; (void)body; (void)tag; (void)id; (void)priority;
    return 0;
}

void CancelNotification(const char *tag, int id)
{
    (void)tag; (void)id;
}

void CancelAllNotifications(void)
{
}

#endif

int SendNotification(const char *title, const char *body)
{
    return SendNotificationEx(title, body, NULL, NOTIFICATION_ID_AUTO,
                              NOTIFICATION_PRIORITY_DEFAULT);
}
