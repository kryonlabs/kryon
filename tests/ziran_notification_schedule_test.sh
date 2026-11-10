#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
cat > "$work/app.zi" <<'ZI'
#import "notification_schedule"
#import "notification_schedule_policy"
#import "notification"
#program_export
Answer :: () -> s32 {
    if !NotificationDateValid(2028, 2, 29) || NotificationDateValid(2100, 2, 29) ||
        NotificationDateValid(2026, 4, 31) || NotificationDateValid(2026, 0, 1) { return 1 }
    if !NotificationDayAllowed(20261102, 20261101, 20261101, true, 1, 127) { return 2 }
    if NotificationDayAllowed(20261102, 20261102, 0, true, 1, 127) { return 3 }
    if NotificationDayAllowed(20261102, 20261103, 0, true, 1, 127) { return 4 }
    if NotificationDayAllowed(20261102, 0, 20261102, false, 1, 127) { return 5 }
    if NotificationDayAllowed(20261102, 0, 20261103, false, 1, 127) { return 6 }
    if NotificationDayAllowed(20261102, 0, 0, false, 1, 1) { return 7 }
    if NotificationDayAllowed(20261102, 0, 0, false, -1, 127) { return 8 }
    if NotificationDayAllowed(20261102, 0, 0, false, 1, 127, 20261103) { return 9 }
    if !NotificationDayAllowed(20261102, 20261102, 0, false, 1, 127, 20261102) { return 10 }
    props: NotificationProps
    props.tag = "game.chest"
    props.id = 1
    props.title = "Chest"
    props.body = "Ready"
    if !DailyNotificationValid(props, 19, 0, 0) { return 11 }
    if DailyNotificationValid(props, 24, 0, 0) || DailyNotificationValid(props, 19, 60, 0) || DailyNotificationValid(props, 19, 0, -1) { return 12 }
    props.tag = ""
    if DailyNotificationValid(props, 19, 0, 0) { return 13 }
    // Non-Android hosts must honestly report persistent scheduling unavailable.
    if DailyNotificationsSupported() || ScheduleDailyNotification(props, 19) ||
        CancelDailyNotification("game.chest", 1) || NotificationAppOpened("game.chest", 1) { return 14 }
    if NotificationOwnerPrefix("hosted:shovey:chest") != "hosted:shovey:" ||
        NotificationOwnerPrefix("hosted:shovey") != "" || NotificationOwnerPrefix("plain") != "" { return 15 }
    dates: [2]s32 = .[20261102, 20261103]
    if !NotificationDatesValid(dates[:]) { return 16 }
    dates[1] = dates[0]
    if NotificationDatesValid(dates[:]) { return 17 }
    dates[1] = 20260230
    if NotificationDatesValid(dates[:]) { return 18 }
    return 42
}
ZI
"$ziran" ir --root "$work" $kryon_ui_args --module-path "$repo/src/backend" -o "$work/ir" "$work/app.zi"
for source in source saved; do
    if test "$source" = source; then
        input=$work/app.zi
        root=$work
        modules=$repo/src/backend
    else
        input=$work/ir/app.zir
        root=$work/ir
        modules=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$modules" $kryon_ui_args --entry app:Answer -o "$work/$source.zx" "$input"
    test "$("$ziran" run "$work/$source.zx")" = 42
done
cmp "$work/source.zx" "$work/saved.zx"
"$ziran" build --target=c --exe --entry app:Answer --root "$work" --module-path "$repo/src/backend" $kryon_ui_args -o "$work/native" "$work/app.zi"
set +e
"$work/native/app"
code=$?
set -e
test "$code" = 42
