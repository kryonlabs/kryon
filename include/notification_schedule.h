#ifndef KRYON_NOTIFICATION_SCHEDULE_H
#define KRYON_NOTIFICATION_SCHEDULE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KryNotificationReminder {
    int enabled;
    int hour;
    int last_day;
    const char *tag;
    int id;
    int priority;
} KryNotificationReminder;

int KryNotificationDayKeyNow(void);
int KryNotificationReminderDue(const KryNotificationReminder *reminder,
                               int day_key, int hour);
int KryNotificationSendReminder(KryNotificationReminder *reminder,
                                const char *title, const char *body,
                                int day_key, int hour);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_NOTIFICATION_SCHEDULE_H */
