#include "notification_schedule.h"
#include "notification.h"

#include <time.h>

int
KryNotificationDayKeyNow(void)
{
    time_t t = time(0);
    struct tm *tm = localtime(&t);

    if(tm == 0)
        return 0;
    return (tm->tm_year + 1900) * 1000 + tm->tm_yday;
}

int
KryNotificationReminderDue(const KryNotificationReminder *reminder,
                           int day_key, int hour)
{
    if(reminder == 0 || !reminder->enabled || day_key <= 0)
        return 0;
    if(reminder->last_day == day_key)
        return 0;
    return reminder->hour == hour;
}

int
KryNotificationSendReminder(KryNotificationReminder *reminder,
                            const char *title, const char *body,
                            int day_key, int hour)
{
    if(!KryNotificationReminderDue(reminder, day_key, hour))
        return 0;
    if(SendNotificationEx(title, body,
                          reminder->tag != 0 ? reminder->tag : "",
                          reminder->id, reminder->priority)) {
        reminder->last_day = day_key;
        return 1;
    }
    return 0;
}
