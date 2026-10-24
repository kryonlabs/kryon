#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/text-input-platform-test
source=$repo/tests/text_input_platform_behavior.zi
mkdir -p "$work"
binding=text_input_platform:SetPlatformTextKeyboardVisible=text_input_platform_host:SetPlatformTextKeyboardVisible

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --bind "$binding" --entry text_input_platform_behavior:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --bind "$binding" --entry text_input_platform_behavior:main -o "$work/saved.zx" \
    "$work/ir/text_input_platform_behavior.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

for target in c; do
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" -o "$work/$target" "$source"
        "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
            "$work/c"/*.c -o "$work/c/test"
    env -u DISPLAY -u WAYLAND_DISPLAY "$work/$target/test"
done
"$ziran" build --target=go --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/go" "$source"
(cd "$work/go" && GO111MODULE=off go test .)
printf '%s\n' 'Session text-input queue and keyboard callbacks passed native and portable tests'
