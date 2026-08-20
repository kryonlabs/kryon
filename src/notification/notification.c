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
                       const char *tag, int id, NotificationPriority priority)
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

int SendNotificationAction(const char *title, const char *body,
                           const char *icon, int expire_ms,
                           int action, const char *action_label,
                           const char *action_url)
{
    /* No click plumbing yet: degrade to the plain notification. */
    (void)icon; (void)expire_ms; (void)action; (void)action_label;
    (void)action_url;
    return SendNotificationEx(title, body, NULL, NOTIFICATION_ID_AUTO,
                              NOTIFICATION_PRIORITY_DEFAULT);
}

int PollNotificationAction(char *url_buf, int url_buf_size)
{
    (void)url_buf; (void)url_buf_size;
    return 0;
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
                       const char *tag, int id, NotificationPriority priority)
{
    (void)priority;   /* browsers have no priority concept */
    if(title == NULL || body == NULL)
        return 0;
    return kryon_web_send(title, body, tag != NULL ? tag : "", id);
}

int SendNotificationAction(const char *title, const char *body,
                           const char *icon, int expire_ms,
                           int action, const char *action_label,
                           const char *action_url)
{
    /* The web backend closes notifications on click but has no poll path
     * for the app: degrade to the plain notification. */
    (void)icon; (void)expire_ms; (void)action; (void)action_label;
    (void)action_url;
    if(title == NULL || body == NULL)
        return 0;
    return kryon_web_send(title, body, "", 0);
}

int PollNotificationAction(char *url_buf, int url_buf_size)
{
    (void)url_buf; (void)url_buf_size;
    return 0;
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

#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#define KRY_MSG (WM_APP + 75)
#define KRY_SLOTS 64
typedef struct { UINT id; int action, active; char url[384]; } WinNote;
static char g_notification_app_name[64] = "kryon";
static HWND g_note_window;
static UINT g_next_note = 1;
static WinNote g_notes[KRY_SLOTS];
static int g_pending_action;
static char g_pending_url[384];
static void wide(const char *s, WCHAR *d, int n) {
    if(!d || n < 1) return; d[0] = 0; if(!s) return;
    if(!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, d, n))
        MultiByteToWideChar(CP_ACP, 0, s, -1, d, n);
    d[n - 1] = 0;
}
static WinNote *slot(UINT id, int add) {
    WinNote *free_slot = NULL; int i;
    for(i = 0; i < KRY_SLOTS; i++) {
        if(g_notes[i].active && g_notes[i].id == id) return &g_notes[i];
        if(!g_notes[i].active && !free_slot) free_slot = &g_notes[i];
    }
    if(!add) return NULL;
    if(!free_slot) free_slot = &g_notes[id % KRY_SLOTS];
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->id = id; free_slot->active = 1; return free_slot;
}
static void remove_note(UINT id) {
    NOTIFYICONDATAW n; WinNote *s;
    if(!g_note_window) return; memset(&n, 0, sizeof(n)); n.cbSize = sizeof(n);
    n.hWnd = g_note_window; n.uID = id; Shell_NotifyIconW(NIM_DELETE, &n);
    s = slot(id, 0); if(s) memset(s, 0, sizeof(*s));
}
static LRESULT CALLBACK note_proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    UINT event, id; WinNote *s;
    if(m != KRY_MSG) return DefWindowProcW(w, m, wp, lp);
    event = (UINT)lp; id = (UINT)wp; s = slot(id, 0);
    if(event == NIN_BALLOONUSERCLICK || event == WM_LBUTTONUP) {
        if(s && s->action) { g_pending_action = s->action;
            snprintf(g_pending_url, sizeof(g_pending_url), "%s", s->url); }
        remove_note(id);
    } else if(event == NIN_BALLOONHIDE || event == NIN_BALLOONTIMEOUT)
        remove_note(id);
    return 0;
}
static HWND note_window(void) {
    static const WCHAR name[] = L"KryonNotificationWindow";
    WNDCLASSEXW c; HINSTANCE h;
    if(g_note_window) return g_note_window; h = GetModuleHandleW(NULL);
    memset(&c, 0, sizeof(c)); c.cbSize = sizeof(c); c.lpfnWndProc = note_proc;
    c.hInstance = h; c.lpszClassName = name; RegisterClassExW(&c);
    g_note_window = CreateWindowExW(WS_EX_TOOLWINDOW, name, L"", WS_POPUP,
        0, 0, 0, 0, NULL, NULL, h, NULL); return g_note_window;
}
static UINT note_id(const char *tag, int id) {
    UINT h = 2166136261u; const unsigned char *p;
    if(id <= 0) return g_next_note++;
    for(p = (const unsigned char *)(tag ? tag : ""); *p; p++)
        { h ^= *p; h *= 16777619u; }
    h ^= (UINT)id; h *= 16777619u; return h ? h : 1;
}
static int win_send(const char *title, const char *body, const char *tag,
                    int id, NotificationPriority priority, int action,
                    const char *url) {
    NOTIFYICONDATAW n; WinNote *s; UINT native_id; HINSTANCE h;
    if(!title || !body || !note_window()) return 0;
    native_id = note_id(tag, id); s = slot(native_id, 1); s->action = action;
    snprintf(s->url, sizeof(s->url), "%s", url ? url : "");
    memset(&n, 0, sizeof(n)); n.cbSize = sizeof(n); n.hWnd = g_note_window;
    n.uID = native_id; n.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
    n.uCallbackMessage = KRY_MSG; h = GetModuleHandleW(NULL);
    n.hIcon = LoadIconW(h, IDI_APPLICATION);
    if(!n.hIcon) n.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wide(g_notification_app_name, n.szTip, ARRAYSIZE(n.szTip));
    wide(title, n.szInfoTitle, ARRAYSIZE(n.szInfoTitle));
    wide(body, n.szInfo, ARRAYSIZE(n.szInfo)); n.dwInfoFlags = NIIF_USER;
    if(priority == NOTIFICATION_PRIORITY_LOW) n.dwInfoFlags |= NIIF_NOSOUND;
    if(!Shell_NotifyIconW(NIM_ADD, &n)) { memset(s, 0, sizeof(*s)); return 0; }
    n.uVersion = NOTIFYICON_VERSION; Shell_NotifyIconW(NIM_SETVERSION, &n);
    return 1;
}
void SetNotificationAppName(const char *name) { if(name && *name)
    snprintf(g_notification_app_name, sizeof(g_notification_app_name), "%s", name); }
int IsNotificationSupported(void) { return note_window() != NULL; }
int IsNotificationPermissionGranted(void) { return IsNotificationSupported(); }
int RequestNotificationPermission(void) { return IsNotificationSupported(); }
int SendNotificationEx(const char *t, const char *b, const char *tag, int id,
                       NotificationPriority p) { return win_send(t,b,tag,id,p,0,NULL); }
int SendNotificationAction(const char *t, const char *b, const char *icon,
    int expire, int action, const char *label, const char *url) {
    (void)icon; (void)expire; (void)label;
    return win_send(t,b,NULL,NOTIFICATION_ID_AUTO,NOTIFICATION_PRIORITY_DEFAULT,action,url);
}
int PollNotificationAction(char *url, int size) {
    MSG m; int action;
    while(PeekMessageW(&m, g_note_window, 0, 0, PM_REMOVE))
        { TranslateMessage(&m); DispatchMessageW(&m); }
    action = g_pending_action; g_pending_action = 0;
    if(action && url && size > 0) { snprintf(url, (size_t)size, "%s", g_pending_url);
        url[size - 1] = 0; } return action;
}
void CancelNotification(const char *tag, int id) { if(id > 0) remove_note(note_id(tag,id)); }
void CancelAllNotifications(void) { int i; for(i=0;i<KRY_SLOTS;i++)
    if(g_notes[i].active) remove_note(g_notes[i].id); }

#elif defined(KRYON_NOTIFICATION_GDBUS)

#include <gio/gio.h>
#include <stdint.h>

#define KRYON_NOTIFICATIONS_BUS    "org.freedesktop.Notifications"
#define KRYON_NOTIFICATIONS_PATH   "/org/freedesktop/Notifications"
#define KRYON_NOTIFICATIONS_IFACE  "org.freedesktop.Notifications"
#define KRYON_NOTIFIED_MAX 64
#define KRYON_NOTIFICATION_ACTION_KEY "open"
#define KRYON_NOTIFICATION_ACTION_SLOTS 16

static char g_notification_app_name[64] = "kryon";
/* Sent ids remembered so CancelAll can close them; the spec has no global
 * cancel. Plain ring, oldest entries fall off. */
static unsigned int g_notified[KRYON_NOTIFIED_MAX];
static int g_notified_count;

/* Action-capable notifications: Notify's returned daemon id is remembered
 * with its URL so the ActionInvoked signal can resolve the click, and the
 * resolved action waits in a single slot for PollNotificationAction. */
static GMutex g_notification_lock;
typedef struct NotificationActionSlot {
    unsigned int daemon_id;
    int action;
    char url[384];
} NotificationActionSlot;
static NotificationActionSlot g_action_slots[KRYON_NOTIFICATION_ACTION_SLOTS];
static unsigned int g_action_slot_next;
static int g_pending_action;
static char g_pending_url[384];

void SetNotificationAppName(const char *name)
{
    if(name == NULL || name[0] == '\0')
        return;
    g_mutex_lock(&g_notification_lock);
    snprintf(g_notification_app_name, sizeof(g_notification_app_name), "%s",
             name);
    g_mutex_unlock(&g_notification_lock);
}

static GDBusConnection *notification_bus(void)
{
    static GDBusConnection *bus;

    if(bus == NULL) {
        g_mutex_lock(&g_notification_lock);
        if(bus == NULL)
            bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        g_mutex_unlock(&g_notification_lock);
    }
    return bus;
}

/* One Notify call. Returns 1 when the daemon accepted it and fills
 * *daemon_id with the id the daemon assigned (0 when it did not give one);
 * callers use that id to correlate ActionInvoked signals. */
static int notification_notify(const char *icon, const char *title,
                               const char *body, int expire_ms,
                               const gchar *const *actions,
                               unsigned int replaces_id, unsigned int urgency,
                               unsigned int *daemon_id)
{
    GDBusConnection *bus = notification_bus();
    GVariant *hints;
    GVariant *result;
    GError *error = NULL;
    int delivered = 0;

    *daemon_id = 0;
    if(bus == NULL)
        return 0;
    hints = g_variant_new_parsed("{"
            "'urgency': <%u>, "
            "'desktop-entry': <%s>"
            "}", urgency, g_notification_app_name);
    /* The Notify message must be assembled from child variants:
     * g_variant_new's "(...as...)" format would demand GVariantBuilders for
     * the array slots (passing NULL or a floating GVariant there is the
     * g_variant_new "expected array GVariantBuilder" abort). */
    GVariant *children[8];
    children[0] = g_variant_new_string(g_notification_app_name);
    children[1] = g_variant_new_uint32(replaces_id);
    children[2] = g_variant_new_string(icon != NULL ? icon : "");
    children[3] = g_variant_new_string(title != NULL ? title : "");
    children[4] = g_variant_new_string(body != NULL ? body : "");
    children[5] = actions != NULL ? g_variant_new_strv(actions, -1)
                                  : g_variant_new_strv(NULL, 0);
    children[6] = hints;
    children[7] = g_variant_new_int32(expire_ms > 0 ? expire_ms : -1);
    result = g_dbus_connection_call_sync(bus,
            KRYON_NOTIFICATIONS_BUS, KRYON_NOTIFICATIONS_PATH,
            KRYON_NOTIFICATIONS_IFACE, "Notify",
            g_variant_new_tuple(children, 8),
            G_VARIANT_TYPE("(u)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1, NULL, &error);
    if(result != NULL) {
        g_variant_get(result, "(u)", daemon_id);
        g_variant_unref(result);
        delivered = 1;
    } else {
        g_error_free(error);
    }
    return delivered;
}

/* ActionInvoked arrives with the daemon's unique bus name as sender, so the
 * subscription matches any sender on the Notifications object path. */
static void
on_notification_action(GDBusConnection *connection, const char *sender,
                       const char *object_path, const char *interface_name,
                       const char *signal_name, GVariant *parameters,
                       gpointer user_data)
{
    guint32 invoked_id = 0;
    const char *action_key = NULL;
    int i;

    (void)connection; (void)sender; (void)object_path;
    (void)interface_name; (void)signal_name; (void)user_data;
    g_variant_get(parameters, "(u&s)", &invoked_id, &action_key);
    if(invoked_id == 0 || action_key == NULL ||
       strcmp(action_key, KRYON_NOTIFICATION_ACTION_KEY) != 0)
        return;
    g_mutex_lock(&g_notification_lock);
    for(i = 0; i < KRYON_NOTIFICATION_ACTION_SLOTS; i++) {
        if(g_action_slots[i].daemon_id == invoked_id) {
            g_pending_action = g_action_slots[i].action;
            snprintf(g_pending_url, sizeof(g_pending_url), "%s",
                     g_action_slots[i].url);
            g_action_slots[i].daemon_id = 0;   /* one shot */
            break;
        }
    }
    g_mutex_unlock(&g_notification_lock);
}

static void
ensure_notification_actions(GDBusConnection *bus)
{
    static int subscribed;

    if(subscribed || bus == NULL)
        return;
    subscribed = 1;
    g_dbus_connection_signal_subscribe(bus, NULL, KRYON_NOTIFICATIONS_IFACE,
                                       "ActionInvoked",
                                       KRYON_NOTIFICATIONS_PATH, NULL,
                                       G_DBUS_SIGNAL_FLAGS_NONE,
                                       on_notification_action, NULL, NULL);
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
                       const char *tag, int id, NotificationPriority priority)
{
    unsigned int replaces_id = id > 0 ? (unsigned int)id : 0;
    unsigned int urgency = priority == NOTIFICATION_PRIORITY_LOW ? 0 :
                           priority == NOTIFICATION_PRIORITY_HIGH ? 2 : 1;
    unsigned int daemon_id;
    int delivered;

    (void)tag;
    if(title == NULL || body == NULL)
        return 0;
    /* The daemon-side icon (desktop-entry) wins; app_icon stays empty. */
    delivered = notification_notify("", title, body, -1, NULL, replaces_id,
                                    urgency, &daemon_id);
    /* Remember the daemon id so Cancel can close it; when the caller
     * supplied its own id the daemon replaced that notification. */
    if(delivered && daemon_id != 0) {
        g_notified[g_notified_count % KRYON_NOTIFIED_MAX] = daemon_id;
        g_notified_count++;
    }
    return delivered;
}

int SendNotificationAction(const char *title, const char *body,
                           const char *icon, int expire_ms,
                           int action, const char *action_label,
                           const char *action_url)
{
    GDBusConnection *bus = notification_bus();
    const gchar *actions[3];
    NotificationActionSlot *slot;
    unsigned int daemon_id;
    int delivered;

    if(bus == NULL || title == NULL || body == NULL)
        return 0;
    if(action == 0 || action_label == NULL || action_label[0] == '\0')
        return SendNotificationEx(title, body, NULL, NOTIFICATION_ID_AUTO,
                                  NOTIFICATION_PRIORITY_DEFAULT);

    ensure_notification_actions(bus);
    actions[0] = KRYON_NOTIFICATION_ACTION_KEY;
    actions[1] = action_label;
    actions[2] = NULL;
    delivered = notification_notify(icon, title, body, expire_ms, actions, 0,
                                    1, &daemon_id);
    if(delivered && daemon_id != 0) {
        g_mutex_lock(&g_notification_lock);
        slot = &g_action_slots[g_action_slot_next %
                               KRYON_NOTIFICATION_ACTION_SLOTS];
        slot->daemon_id = daemon_id;
        slot->action = action;
        snprintf(slot->url, sizeof(slot->url), "%s",
                 action_url != NULL ? action_url : "");
        g_action_slot_next++;
        g_mutex_unlock(&g_notification_lock);
    }
    return delivered;
}

int PollNotificationAction(char *url_buf, int url_buf_size)
{
    int action;

    /* Dispatch pending ActionInvoked callbacks even without an app-side
     * main loop (non-blocking; the tray's gtk_main pumps the same default
     * context when one is running). */
    g_main_context_iteration(NULL, FALSE);
    g_mutex_lock(&g_notification_lock);
    action = g_pending_action;
    g_pending_action = 0;
    if(action != 0 && url_buf != NULL && url_buf_size > 0)
        snprintf(url_buf, (size_t)url_buf_size, "%s", g_pending_url);
    g_mutex_unlock(&g_notification_lock);
    return action;
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
                       const char *tag, int id, NotificationPriority priority)
{
    (void)title; (void)body; (void)tag; (void)id; (void)priority;
    return 0;
}

int SendNotificationAction(const char *title, const char *body,
                           const char *icon, int expire_ms,
                           int action, const char *action_label,
                           const char *action_url)
{
    (void)title; (void)body; (void)icon; (void)expire_ms;
    (void)action; (void)action_label; (void)action_url;
    return 0;
}

int PollNotificationAction(char *url_buf, int url_buf_size)
{
    (void)url_buf; (void)url_buf_size;
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
