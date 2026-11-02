#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/scratch/notification-push-test
mkdir -p "$work"
cat > "$work/app.zi" <<'ZI'
#import "notification_push"
#program_export
Answer :: () -> s32 {
    providers: [3]string = .["org.unifiedpush.distributor.sunup", "org.example.other", "org.example.last"]
    if PushChoose(false, false, "org.example.other", providers[0], providers[:]).distributor != "org.example.other" { return 1 }
    if PushChoose(false, false, "org.removed", providers[0], providers[:]).distributor != providers[0] { return 2 }
    if PushChoose(false, false, "", "", providers[:]).distributor != "org.example.last" { return 3 }
    if PushChoose(false, false, "", "", providers[:0]).status != PushNoDistributor { return 4 }
    if PushChoose(true, false, providers[0], providers[0], providers[:]).status != PushFirebaseUnconfigured { return 5 }
    choice := PushChoose(true, true, providers[0], providers[0], providers[:])
    if choice.provider != PushProviderFcm || choice.distributor.count != 0 || choice.status != PushRegistering { return 6 }
    if !PushAccepts(PushRegistering, 1, 1, "current", "current") ||
        !PushAccepts(PushReady, 1, 1, "current", "current") ||
        !PushAccepts(PushUnavailable, 1, 1, "current", "current") { return 7 }
    if PushAccepts(PushDisabled, 1, 1, "current", "current") ||
        PushAccepts(PushFailed, 1, 1, "current", "current") ||
        PushAccepts(PushNoDistributor, 1, 1, "current", "current") ||
        PushAccepts(PushReady, 1, 2, "current", "current") ||
        PushAccepts(PushReady, 1, 1, "current", "previous") ||
        PushAccepts(PushReady, 1, 1, "", "") { return 8 }
    if PushScopeValid("") || PushScopeValid("account\nother") || !PushScopeValid("account.id") { return 9 }
    state: PushState
    if PushStart("account", "App") != PushUnsupported || PushRead("account", *state) || PushCancel("account") { return 10 }
    return 42
}
ZI
"$ziran" ir --root "$work" --module-path "$repo/src/backend" -o "$work/ir" "$work/app.zi"
for source in source saved; do
    if test "$source" = source; then input=$work/app.zi; root=$work; else input=$work/ir/app.zir; root=$work/ir; fi
    "$ziran" bundle --root "$root" --module-path "$repo/src/backend" --entry app:Answer -o "$work/$source.zx" "$input"
    test "$("$ziran" run "$work/$source.zx")" = 42
done
cmp "$work/source.zx" "$work/saved.zx"
"$ziran" build --target=c --exe --entry app:Answer --root "$work" --module-path "$repo/src/backend" -o "$work/native" "$work/app.zi"
set +e
"$work/native/app"
code=$?
set -e
test "$code" = 42
python3 "$repo/tests/notification_push_android_test.py" --ziran-root "$ziran_root" --work "$work"
echo 'Optional push selection, lifecycle, JNI callbacks and edition isolation local tests passed'
