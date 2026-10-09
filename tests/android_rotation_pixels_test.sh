#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/scratch/android-rotation
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
archive=${RAYLIB_A:-"$repo/build/raylib-ziran/libraylib.a"}
if ! test -f "$archive"; then
    raylib_source=$("$ziran" pkg path raylib)
    mkdir -p "$work/raylib-source"
    cp -R "$raylib_source/src/." "$work/raylib-source/"
    make -j4 -C "$work/raylib-source" RAYLIB_SRC_PATH=. RAYLIB_RELEASE_PATH=.. \
        PLATFORM=PLATFORM_DESKTOP_SDL GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
        RAYLIB_LIBTYPE=STATIC RAYLIB_MODULE_AUDIO=TRUE RAYLIB_MODULE_MODELS=TRUE \
        SDL_INCLUDE_PATH="$(pkg-config --variable=includedir sdl2)" \
        CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(pkg-config --cflags sdl2 libdrm gbm egl glesv2) -O2 -ffunction-sections -fdata-sections"
    archive=$work/libraylib.a
fi
libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
source=$repo/tests/android_rotation_pixels.zi
"$ziran" ir --define ANDROID_BUILD --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
cd "$repo"
# Reproduce the pre-fix portrait-first failure with the actual Android symbol
# visibility: native dimensions rotate, but the portrait buffer is never reset.
sed '/^[[:space:]]*CORE;/d' "$repo/cmake/libmain.map.txt" > "$work/hidden-core.map"
"$ziran" build --target=c --define ANDROID_BUILD --define HIDDEN_CORE_FIXTURE \
    --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    -o "$work/hidden" "$source"
"${CC:-cc}" -std=c11 -O1 -fPIC -rdynamic -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -Wl,--wrap=GetWindowHandle \
    -Wl,--version-script="$work/hidden-core.map" -I"$work/hidden" \
    "$work/hidden"/*.c "$archive" $libs -o "$work/hidden/test"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
    -u GDK_DISPLAY LIBGL_ALWAYS_SOFTWARE=1 YUE_DESKTOP_RECOVERY=0 \
    timeout --kill-after=2s 30s xvfb-run -a -n 300 \
    -s '-screen 0 900x900x24' "$work/hidden/test"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input=$work/ir/android_rotation_pixels.zir; fi
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --define ANDROID_BUILD --root "$repo/tests" \
            --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
            --module-path "$ziran_root/std" -o "$output" "$input"
        if test "$target" = c; then compiler=${CC:-cc}; standard=c11; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        "$compiler" -std="$standard" -O1 -fPIC -rdynamic \
            -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -Wl,--wrap=GetWindowHandle -Wl,--version-script="$repo/cmake/libmain.map.txt" \
            -I"$output" "$output"/*."$extension" "$archive" $libs -o "$output/test"
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            -u GDK_DISPLAY LIBGL_ALWAYS_SOFTWARE=1 YUE_DESKTOP_RECOVERY=0 \
            timeout --kill-after=2s 30s xvfb-run -a -n 300 \
            -s '-screen 0 900x900x24' "$output/test"
    done
done
echo 'Android portrait/landscape/portrait: native pixels, projection, scale and exported CORE passed C/C++ source and saved IR'
