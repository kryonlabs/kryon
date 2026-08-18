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
                       const char *tag, int id, NotificationPriority priority);

/* Action-capable form (desktop popups): expire_ms <= 0 keeps the daemon
 * default; icon is an absolute path or icon name, "" lets the daemon resolve
 * the desktop-entry icon registered via SetNotificationAppName. When action
 * != 0 and action_label != NULL the popup carries one button labeled
 * action_label, and clicking it is delivered once by
 * PollNotificationAction() together with a copy of action_url. Backends
 * without action support return 0 (callers should fall back). Never
 * blocks. */
int SendNotificationAction(const char *title, const char *body,
                           const char *icon, int expire_ms,
                           int action, const char *action_label,
                           const char *action_url);

/* Returns the pending notification action (0 if none) and copies its URL
 * into url_buf when non-NULL. One slot: a click arriving before the previous
 * one was polled replaces it. Also pumps the signal dispatch context, so
 * clicks arrive without an app-side main loop. */
int PollNotificationAction(char *url_buf, int url_buf_size);

/* Dismiss a previously sent notification. */
void CancelNotification(const char *tag, int id);

/* Dismiss everything this app sent (best effort where the platform has no
 * global cancel). */
void CancelAllNotifications(void);

#endif /* KRYON_NOTIFICATION_H */
