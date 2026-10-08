#!/bin/sh
# Windows must not import the Linux Fontconfig loader. Bundled fonts use the
# shared file/data path; absence of OS discovery must be reported truthfully.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/windows-typeface.XXXXXX")
wine_started=0
cleanup() {
    test_status=$?
    trap - EXIT HUP INT TERM
    if test "$wine_started" = 1; then
        WINEPREFIX="$work/prefix" wineserver -k || true
        WINEPREFIX="$work/prefix" wineserver -w || true
    fi
    rm -rf "$work"
    exit "$test_status"
}
trap cleanup EXIT HUP INT TERM
source=$repo/tests/raylib_typeface_windows_test.zi
"$ziran" ir --define _WIN32 --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input=$work/ir/raylib_typeface_windows_test.zir; fi
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --define _WIN32 --root "$repo/tests" \
            --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
            --module-path "$ziran_root/std" -o "$output" "$input"
        if test -e "$output/system_typeface_linux.c" || \
            test -e "$output/system_typeface_linux.cpp" || \
            test -e "$output/system_typeface_android.c" || \
            test -e "$output/system_typeface_android.cpp"; then
            echo 'Windows renderer imported a foreign OS font loader' >&2
            exit 1
        fi
        if test "$target" = c; then compiler=${CC:-cc}; standard=c11; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        "$compiler" -std="$standard" -O2 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$output" "$output"/*."$extension" -lm \
            -o "$output/test"
        "$output/test"
        if test "$target" = c && test -n "${WIN64_RAYLIB_A:-}" && \
            command -v "${WIN64_CC:-x86_64-w64-mingw32-gcc}" >/dev/null 2>&1; then
            test -f "$WIN64_RAYLIB_A"
            # The PE linker retains Raylib's public program exports. Link the
            # real Windows archive rather than supplying fake host functions.
            "${WIN64_CC:-x86_64-w64-mingw32-gcc}" -std=c11 -O2 \
                -ffunction-sections -fdata-sections -Wl,--gc-sections \
                -I"$output" "$output"/*.c "$WIN64_RAYLIB_A" \
                -lopengl32 -lgdi32 -lwinmm -lshell32 -luser32 -lm -o "$output/test.exe"
            echo "Windows font discovery boundary cross-linked ($form)"
            if command -v wine >/dev/null 2>&1 && \
                command -v wineserver >/dev/null 2>&1 && \
                command -v xvfb-run >/dev/null 2>&1; then
                wine_started=1
                WINEPREFIX="$work/prefix" WINEDEBUG=-all WINEDLLOVERRIDES='mscoree,mshtml=' \
                    xvfb-run -a wine "$output/test.exe"
                echo "Windows font discovery boundary executed under private Wine ($form)"
            fi
        fi
    done
done
echo 'Windows font discovery boundary passed C/C++ source and saved IR without POSIX dynamic loading'
