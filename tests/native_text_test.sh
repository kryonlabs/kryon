#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
build=$repo/build/text-shaping
mkdir -p "$build"
exec 9>"$build/lock"
flock 9
"$ziran" ir --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" --root "$repo/tests" -o "$build/ir" "$repo/tests/cairo_text.zi"
for source in source saved; do
    input=$repo/tests/cairo_text.zi
    root=$repo/tests
    if [ "$source" = saved ]; then input=$build/ir/cairo_text.zir; root=$build/ir; fi
    for target in c cpp; do
        output=$build/$source-$target
        "$ziran" build --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" --target="$target" --no-main --root "$root" \
            --module-path "$build/ir" --entry cairo_text:Run -o "$output" "$input"
        suffix=c; header=h; compiler=${CC:-cc}; standard=c11
        if [ "$target" = cpp ]; then suffix=cpp; header=hpp; compiler=${CXX:-c++}; standard=c++17; fi
        printf '#include "cairo_text.%s"\nint main(void) { return Run(); }\n' "$header" >"$output/driver.$suffix"
        "$compiler" -std="$standard" -O2 -I"$ziran_root/include" -I"$output" \
            "$output"/*."$suffix" $(pkg-config --libs pangocairo fontconfig freetype2) \
            -lm -lpthread -ldl -o "$output/run"
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            YUE_DESKTOP_RECOVERY=0 "$output/run"
    done
done
echo 'Native shaping, RTL caret and long text: source/saved IR, C/C++ PASS'
