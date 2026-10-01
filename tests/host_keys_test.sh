#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/test"
output=$(mktemp -d "$repo/build/test/host-keys.XXXXXX")
trap 'rm -rf "$output"' EXIT HUP INT TERM
# Each host's key translation must agree across generated languages and the
# portable VM.
for host in libdraw desktop terminal; do
work=$output/$host
test_module=${host}_keys_test
mkdir -p "$work"
for target in c cpp go; do
    set --
    if [ "$target" = go ]; then set -- --pkg main --exe; fi
    "$ziran" build "$@" --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/backend" --entry $test_module:main \
        -o "$work/$target" "$repo/tests/$test_module.zi"
    case "$target" in
        c) cc -std=c99 -I"$ziran_root/include" "$work/c"/*.c -o "$work/run-c"; "$work/run-c" ;;
        cpp) c++ -std=c++17 -I"$ziran_root/include" "$work/cpp"/*.cpp -o "$work/run-cpp"; "$work/run-cpp" ;;
        go)
            GO111MODULE=off go run "$work/go"/*.go
            ;;
    esac
done
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/backend" \
    --entry $test_module:main -o "$work/keys.zib" "$repo/tests/$test_module.zi"
test "$("$ziran" run "$work/keys.zib")" = 0
done
echo 'Libdraw, desktop, and terminal keyboard: C, C++, Go and portable key codes passed'
