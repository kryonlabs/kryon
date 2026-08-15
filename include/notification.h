#ifndef KRYON_NOTIFICATION_H
#define KRYON_NOTIFICATION_H

/*
 * Cross-platform user notifications.
 *
 * Backends, chosen at build time:
 *   - Android (__ANDROID__): pure JNI against NotificationManager /
 *     Notification.Builder through the native activity. Channels are
 *     created on API 26+, the legacy priority path covers API 21-25, and
 *     POST_NOTIFICATIONS is requested at runtime on API 33+.
 *   - Web (PLATFORM_WEB): the browser Notification API.
 *   - Desktop Linux (KRYON_NOTIFICATION_GDBUS): org.freedesktop.Notifications
 *     over the session bus (GDBus). Compile with -DKRYON_NOTIFICATION_GDBUS
 *     and link gio-2.0; mk/vendor.mk exports the flags when available.
 *   - Otherwise a stub: nothing is supported, sends return 0.
 *
 * `tag` groups notifications that replace each other (Android notify tag,
 * web Notification tag); `id` addresses one notification for cancel.
 * NOTIFICATION_ID_AUTO lets the platform assign.
 */

#define NOTIFICATION_ID_AUTO 0

typedef enum {
    NOTIFICATION_PRIORITY_DEFAULT = 0,
    NOTIFICATION_PRIORITY_LOW,      /* silent / no heads-up */
    NOTIFICATION_PRIORITY_HIGH      /* heads-up on Android, urgent elsewhere */
} NotificationPriority;

/* Daemon-side application name shown by desktop notifications and used to
 * resolve the icon (desktop-entry name, e.g. "inbe"). Default: "kryon". */
void SetNotificationAppName(const char *name);

/* 1 when a backend is compiled in and reachable. */
int IsNotificationSupported(void);

/* 1 when notifications may be shown right now (permission granted, or the
 * platform has no permission concept). */
int IsNotificationPermissionGranted(void);

/* Start the permission request if needed (Android 33+, browsers). Returns 1
 * if already granted, 0 otherwise; the answer may arrive asynchronously --
 * poll IsNotificationPermissionGranted(). */
int RequestNotificationPermission(void);

/* Fire-and-forget notification; returns 1 when handed to a backend. */
int SendNotification(const char *title, const char *body);

/* Full form: notifications sharing (tag, id) replace each other. */
int SendNotificationEx(const char *title, const char *body,
                       const char *tag, int id, int priority);

/* Dismiss a previously sent notification. */
void CancelNotification(const char *tag, int id);

/* Dismiss everything this app sent (best effort where the platform has no
 * global cancel). */
void CancelAllNotifications(void);

#endif /* KRYON_NOTIFICATION_H */
