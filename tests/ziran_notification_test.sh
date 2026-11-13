#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

cat > "$work/test_case.zi" <<'ZI'
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

"$ziran" ir --root "$work" $kryon_project_args --module-path "$work" \
    -o "$work/ir" "$work/test_case.zi" "$work/provider.zi"
for source in source saved; do
    if test "$source" = source; then
        root=$work
        modules="$kryon_project_args --module-path $work"
        app=$work/test_case.zi
        provider=$work/provider.zi
    else
        root=$work/ir
        modules="--project --module-path $work/ir"
        app=$work/ir/test_case.zir
        provider=$work/ir/provider.zir
    fi
    "$ziran" bundle --root "$root" $modules \
        --entry test_case:Answer \
        --bind kryon/notification:NotificationSupportedHost=provider:Supported \
        --bind kryon/notification:NotificationAllowedHost=provider:Allowed \
        --bind kryon/notification:NotificationRequestHost=provider:Request \
        --bind kryon/notification:NotificationWindowFocusedHost=provider:Focused \
        --bind kryon/notification:NotificationSetAppNameHost=provider:SetName \
        --bind kryon/notification:NotificationSendHost=provider:Send \
        -o "$work/$source.zx" "$app" "$provider"
    test "$("$ziran" run "$work/$source.zx")" = 42
done
cmp "$work/source.zx" "$work/saved.zx"
