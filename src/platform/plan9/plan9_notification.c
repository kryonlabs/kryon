#ifdef KRYON_NATIVE_PLAN9

#include "kryon_plan9.h"
#include "notification.h"

void
SetNotificationAppName(const char *name)
{
    (void)name;
}

int
IsNotificationSupported(void)
{
    return 0;
}

int
IsNotificationPermissionGranted(void)
{
    return 1;
}

int
RequestNotificationPermission(void)
{
    return 1;
}

int
SendNotification(const char *title, const char *body)
{
    (void)title;
    (void)body;
    return 0;
}

int
SendNotificationEx(const char *title, const char *body, const char *tag,
                   int id, NotificationPriority prio)
{
    (void)title;
    (void)body;
    (void)tag;
    (void)id;
    (void)prio;
    return 0;
}

int
SendNotificationAction(const char *title, const char *body, const char *icon,
                       int expire_ms, int action, const char *action_label,
                       const char *action_url)
{
    (void)title;
    (void)body;
    (void)icon;
    (void)expire_ms;
    (void)action;
    (void)action_label;
    (void)action_url;
    return 0;
}

int
PollNotificationAction(char *url_buf, int url_buf_size)
{
    (void)url_buf;
    (void)url_buf_size;
    return 0;
}

void
CancelNotification(const char *tag, int id)
{
    (void)tag;
    (void)id;
}

void
CancelAllNotifications(void)
{
}

#endif
