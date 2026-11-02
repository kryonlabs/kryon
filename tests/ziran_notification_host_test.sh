#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
git -C "$repo" check-ignore -q work
mkdir -p "$repo/work"
work=$(mktemp -d "$repo/work/notification-host.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
for variant in good spoof wrong disabled; do
    define=
    case "$variant" in
        spoof) define='--define NOTIFICATION_SPOOF_TAG' ;;
        wrong) define='--define NOTIFICATION_WRONG_ID' ;;
        disabled) define='--define NOTIFICATION_DISABLED' ;;
    esac
    "$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
        --define KRYON_HOSTED_UI $define -o "$work/ir-$variant" "$repo/tests/notification_schedule_guest.zi"
    "$ziran" bundle --root "$repo/tests" --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
        --define KRYON_HOSTED_UI $define --entry notification_schedule_guest:main \
        -o "$work/$variant-source.zx" "$repo/tests/notification_schedule_guest.zi"
    "$ziran" bundle --root "$work/ir-$variant" --module-path "$work/ir-$variant" \
        --entry notification_schedule_guest:main -o "$work/$variant-saved.zx" "$work/ir-$variant/notification_schedule_guest.zir"
    cmp "$work/$variant-source.zx" "$work/$variant-saved.zx"
done
cp "$repo/tests/notification_schedule_provider.zi" "$work/notification_schedule.zi"
"$ziran" build --target=c --no-main --entry notification_schedule_host_test:main \
    --root "$repo/tests" --module-path "$work" --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
    -o "$work/native" "$repo/tests/notification_schedule_host_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$work/native" -I"$ziran_root/include" \
    "$work"/native/*.c "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" -Wl,--gc-sections -lm -lpthread -o "$work/native/notification_schedule_host_test"
for form in source saved; do
    for variant in good spoof wrong disabled; do cp "$work/$variant-$form.zx" "$work/$variant.zx"; done
    (cd "$work" && GLIBC_TUNABLES=glibc.malloc.tcache_count=0 MALLOC_PERTURB_=165 "$work/native/notification_schedule_host_test")
done
echo 'Native hosted reminders: source/saved ZX, verified identity, tag isolation, opt-out and malformed calls passed'
