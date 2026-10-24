#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/test"
output=$(mktemp -d "$repo/build/test/host-keys.XXXXXX")
trap 'rm -rf "$output"' EXIT HUP INT TERM
# Each host's key translation must agree across generated languages and the
# portable VM.
for host in desktop terminal; do
work=$output/$host
test_module=${host}_keys_test
mkdir -p "$work"
for target in c go; do
    set --
    if [ "$target" = go ]; then set -- --pkg main --exe; fi
    "$ziran" build "$@" --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/backend" --entry $test_module:main \
        -o "$work/$target" "$repo/tests/$test_module.zi"
    case "$target" in
        c) cc -std=c99 -I"$ziran_root/include" "$work/c"/*.c -o "$work/run-c"; "$work/run-c" ;;
        go)
            GO111MODULE=off go run "$work/go"/*.go
            ;;
    esac
done
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/backend" \
    --entry $test_module:main -o "$work/keys.zx" "$repo/tests/$test_module.zi"
test "$("$ziran" run "$work/keys.zx")" = 0
done
echo 'Desktop and terminal keyboard: C, Go and portable key codes passed'
