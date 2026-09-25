#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "notification"

using NotificationPriority;
provider :: #import "provider";

#program_export
Answer :: () -> s32 {
    if !NotificationsSupported() || !NotificationsAllowed() ||
       !RequestNotifications() {
        return -1
    }
    SetNotificationAppName("inbe")
    props: NotificationProps
    props.title = "Reminder"
    props.body = "Take a break"
    props.tag = "inbe.break"
    props.id = 7
    props.priority = cast(NotificationPriority)NotificationPriorityHigh
    if !Notify(props) {
        return -2
    }
    props.suppress_when_focused = true
    if !Notify(props) {
        return -4
    }
    provider.SetFocused(true)
    if Notify(props) {
        return -5
    }
    props.title = ""
    if Notify(props) {
        return -3
    }
    return 42
}
ZI
cat > "$work/provider.zi" <<'ZI'
#import "notification"

using NotificationPriority;

focused: bool;

#program_export
SetFocused :: (value: bool) { focused = value }

#program_export
Focused :: () -> bool { return focused }

#program_export
Supported :: () -> bool { return true }

#program_export
Allowed :: () -> bool { return true }

#program_export
Request :: () -> bool { return true }

#program_export
SetName :: (name: string) {
    unused name
}

#program_export
Send :: (props: NotificationProps) -> bool {
    return props.title == "Reminder" && props.body == "Take a break" &&
           props.tag == "inbe.break" && props.id == 7 &&
           props.priority == cast(NotificationPriority)NotificationPriorityHigh
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi" "$work/provider.zi"
for source in source saved; do
    if test "$source" = source; then
        root=$work
        modules=$repo/src/ui
        app=$work/app.zi
        provider=$work/provider.zi
    else
        root=$work/ir
        modules=$work/ir
        app=$work/ir/app.zir
        provider=$work/ir/provider.zir
    fi
    "$ziran" bundle --root "$root" --module-path "$modules" \
        --entry app:Answer \
        --bind notification:NotificationSupportedHost=provider:Supported \
        --bind notification:NotificationAllowedHost=provider:Allowed \
        --bind notification:NotificationRequestHost=provider:Request \
        --bind notification:NotificationWindowFocusedHost=provider:Focused \
        --bind notification:NotificationSetAppNameHost=provider:SetName \
        --bind notification:NotificationSendHost=provider:Send \
        -o "$work/$source.zib" "$app" "$provider"
    test "$("$ziran" run "$work/$source.zib")" = 42
done
cmp "$work/source.zib" "$work/saved.zib"
