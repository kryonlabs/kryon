#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran_root=${ZIRAN_ROOT:-"$repo/../ziran"}
# The org-grouped local layout keeps Ziran under ziranlang/.
if [ -z "${ZIRAN_ROOT:-}" ] && [ ! -d "$ziran_root" ]; then
    ziran_root=$repo/../../ziranlang/ziran
fi
ziran=${ZIRAN_BIN:-"$ziran_root/build/bin/ziran"}
work=$repo/build/test/libdraw-keys
mkdir -p "$work"
for target in c cpp go; do
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/backend" --entry libdraw_keys_test:main \
        -o "$work/$target" "$repo/tests/libdraw_keys_test.zi"
    case "$target" in
        c) cc -std=c99 -I"$ziran_root/include" "$work/c"/*.c -o "$work/run-c"; "$work/run-c" ;;
        cpp) c++ -std=c++17 -I"$ziran_root/include" "$work/cpp"/*.cpp -o "$work/run-cpp"; "$work/run-cpp" ;;
        go)
            mv "$work/go/libdraw_keys_test.go" "$work/go/key_probe.go"
            sed -i 's/^package ziran$/package main/' "$work/go"/*.go
            printf '%s\n' 'package main' 'func main() { if LibdrawKeysTest_Main() != 0 { panic("libdraw keys") } }' > "$work/go/main.go"
            (cd "$work/go" && GOCACHE="$work/go-cache" go run *.go)
            ;;
    esac
done
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/backend" \
    --entry libdraw_keys_test:main -o "$work/keys.zib" "$repo/tests/libdraw_keys_test.zi"
test "$("$ziran" run "$work/keys.zib")" = 0
echo 'Libdraw keyboard: C, C++, Go and portable input semantics passed'
