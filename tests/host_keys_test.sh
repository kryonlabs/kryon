#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran_root=${ZIRAN_ROOT:-"$repo/../ziran"}
# The org-grouped local layout keeps Ziran under ziranlang/.
if [ -z "${ZIRAN_ROOT:-}" ] && [ ! -d "$ziran_root" ]; then
    ziran_root=$repo/../../ziranlang/ziran
fi
ziran=${ZIRAN_BIN:-"$ziran_root/build/bin/ziran"}
# Each host's key translation must agree across generated languages and the
# portable VM.
for host in libdraw desktop terminal; do
work=$repo/build/test/$host-keys
test_module=${host}_keys_test
mkdir -p "$work"
for target in c cpp go; do
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/backend" --entry $test_module:main \
        -o "$work/$target" "$repo/tests/$test_module.zi"
    case "$target" in
        c) cc -std=c99 -I"$ziran_root/include" "$work/c"/*.c -o "$work/run-c"; "$work/run-c" ;;
        cpp) c++ -std=c++17 -I"$ziran_root/include" "$work/cpp"/*.cpp -o "$work/run-cpp"; "$work/run-cpp" ;;
        go)
            mv "$work/go/$test_module.go" "$work/go/key_probe.go"
            sed -i 's/^package ziran$/package main/' "$work/go"/*.go
            entry=$(grep -o '^func [A-Za-z]*KeysTest_Main' "$work/go"/*.go | head -1 | sed 's/.*func //')
            printf '%s\n' 'package main' "func main() { if $entry() != 0 { panic(\"$host keys\") } }" > "$work/go/main.go"
            (cd "$work/go" && GOCACHE="$work/go-cache" go run *.go)
            ;;
    esac
done
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/backend" \
    --entry $test_module:main -o "$work/keys.zib" "$repo/tests/$test_module.zi"
test "$("$ziran" run "$work/keys.zib")" = 0
done
echo 'Libdraw, desktop, and terminal keyboard: C, C++, Go and portable key codes passed'
