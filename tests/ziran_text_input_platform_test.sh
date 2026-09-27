#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$repo/build/text-input-platform-test
source=$repo/tests/text_input_platform_behavior.zi
mkdir -p "$work"
binding=text_input_platform:SetPlatformTextKeyboardVisible=text_input_platform_host:SetPlatformTextKeyboardVisible

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --bind "$binding" --entry text_input_platform_behavior:main -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --bind "$binding" --entry text_input_platform_behavior:main -o "$work/saved.zib" \
    "$work/ir/text_input_platform_behavior.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

for target in c cpp; do
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" -o "$work/$target" "$source"
    if [ "$target" = c ]; then
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
            "$work/c"/*.c -o "$work/c/test"
    else
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$work/cpp" \
            "$work/cpp"/*.cpp -o "$work/cpp/test"
    fi
    env -u DISPLAY -u WAYLAND_DISPLAY "$work/$target/test"
done
"$ziran" build --target=go --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/go" "$source"
(cd "$work/go" && GO111MODULE=off go test .)
printf '%s\n' 'Session text-input queue and keyboard callbacks passed native and portable tests'
