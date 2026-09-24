#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#module "app"
#import "frame_pacing"
Answer :: () -> i32 #export {
    state: FramePacing
    decision: FramePacingDecision = ConfigureFramePacing(state, 30, 60)
    state = CommitFramePacing(decision)
    return state.target_fps
}
EOF
"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
if "$ziran" run "$work/source.zib" 2> "$work/missing.err"; then
    echo 'frame pacing unexpectedly ran without its timer host' >&2
    exit 1
fi
grep -Fq 'missing host capability: frame_pacing:ApplyTargetFPS' \
    "$work/missing.err"
"${CC:-cc}" -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" -I"$repo/../ziran/include" \
    -o "$work/frame-host-test" "$repo/tests/ziran_frame_host_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$repo/../ziran/build/libziran.a"
"$work/frame-host-test" "$work/source.zib"
"$work/frame-host-test" "$work/saved.zib"
