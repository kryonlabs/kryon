#!/bin/sh
# Scene's shapes and RenderScene's lighting, shadows and smoothing, natively
# and in the portable runner.
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/scene_behavior.zi
"$ziran" build --target=c --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "std=$ziran_root/std" --module-path "$ziran_root/std" \
    --exe --entry scene_behavior:main -o "$work/c" "$source"
"$work/c/scene_behavior"

"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "std=$ziran_root/std" --module-path "$ziran_root/std" \
    --entry scene_behavior:main -o "$work/scene.zx" "$source"
test "$("$ziran" run "$work/scene.zx")" = 0
