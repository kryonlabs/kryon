#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-blend"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/pixmap_blend_test.zi"
"$ziran" build --project --root "$repo/tests" --target=c --define PIXMAP_BLEND_EXHAUSTIVE \
    --entry pixmap_blend_test:main --exe -o "$work/exhaustive" "$source"
"$work/exhaustive/pixmap_blend_test"
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in ${PIXMAP_BLEND_TARGETS:-c cpp go rust py}; do
    for input in source saved; do
        executable=--exe; if test "$target" = cpp; then executable=; fi
        package=; if test "$target" = go; then package='--pkg main'; fi
        if test "$input" = source; then
            "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_blend_test:main $package $executable -o "$work/$target-$input" "$source"
        else
            "$ziran" build --root "$work/ir" --module-path "$work/ir" --target="$target" --entry pixmap_blend_test:main $package $executable -o "$work/$target-$input" "$work/ir/pixmap_blend_test.zir"
        fi
        case $target in
            cpp) "${CXX:-c++}" -std=c++17 -O2 -I"$ziran_root/include" "$work/$target-$input"/*.cpp -pthread -lm -o "$work/$target-$input/pixmap_blend_test"; "$work/$target-$input/pixmap_blend_test" ;;
            go) env GO111MODULE=off go run "$work/$target-$input"/*.go ;;
            rust) CARGO_TARGET_DIR="$work/rust-target-$input" cargo run --offline --quiet --manifest-path "$work/$target-$input/Cargo.toml" ;;
            py) python3 "$work/$target-$input/__main__.py" ;;
            c) "$work/$target-$input/pixmap_blend_test" ;;
        esac
    done
done
for input in source saved; do
    if test "$input" = source; then
        "$ziran" bundle --project --root "$repo/tests" --entry pixmap_blend_test:main -o "$work/$input.zib" "$source"
    else
        "$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_blend_test:main -o "$work/$input.zib" "$work/ir/pixmap_blend_test.zir"
    fi
    test "$("$ziran" run "$work/$input.zib" | tail -1)" = 0
done
