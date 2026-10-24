#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch/style-cache"
exec 9>"$repo/build/scratch/style-cache/test.lock"
flock 9
work="$repo/build/scratch/style-cache/current"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$repo/build/scratch/style-cache/go-cache"
source="$repo/tests/style_cache_behavior.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for input in source saved; do
    module="$source"; roots="--module-path $repo/src/ui --module-path $ziran_root/std"
    if test "$input" = saved; then module="$work/ir/style_cache_behavior.zir"; roots="--module-path $work/ir"; fi
    "$ziran" bundle --root "$repo/tests" $roots --entry style_cache_behavior:main \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx" | tail -1)" = 0
    for target in c go; do
        output="$work/$target-$input"
        executable=--exe
        package=; if test "$target" = go; then package='--pkg main'; fi
        "$ziran" build --target="$target" --root "$repo/tests" $roots $executable $package \
            --entry style_cache_behavior:main -o "$output" "$module"
        case $target in
            c) "$output/style_cache_behavior" ;;
            go) GO111MODULE=off go run "$output"/*.go ;;
        esac
    done
done
printf 'Style parity verified across native targets and portable bundles: %s\n' "$work"
