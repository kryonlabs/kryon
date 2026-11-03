#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY LISTEN_PID LISTEN_FDS LISTEN_FDNAMES NOTIFY_SOCKET
export YUE_DESKTOP_RECOVERY=0 WINEDEBUG=-all
work="$repo/build/scratch/windows-app-files"
git -C "$repo" check-ignore -q build/scratch/windows-app-files/probe
mkdir -p "$work/home" "$work/config" "$work/data" "$work/cache" "$work/runtime" "$work/tmp"
chmod 700 "$work/runtime"
export TMPDIR="$work/tmp" XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" XDG_CACHE_HOME="$work/cache" XDG_RUNTIME_DIR="$work/runtime" WINEPREFIX="$work/prefix"
# Ubuntu's Wine 10 keeps wineserver beside the multiarch loader, without
# installing a wineserver command on PATH. Use that same server for bootstrap
# and cleanup; never fall back to the owner's default prefix.
wine=$(command -v wine)
if command -v wineserver >/dev/null 2>&1; then
    WINESERVER=$(command -v wineserver)
else
    WINESERVER=
    for server in "$(dirname "$(readlink -f "$wine")")/wineserver" \
        /usr/lib/x86_64-linux-gnu/wine/wineserver /usr/lib/wine/wineserver; do
        if [ -x "$server" ]; then WINESERVER=$server; break; fi
    done
fi
[ -n "$WINESERVER" ] && [ -x "$WINESERVER" ] || {
    echo 'Windows storage test requires the Wine server (install wine64)' >&2
    exit 1
}
export WINESERVER
# Nix splits mcfgthread's headers and static runtime. Its GCC wrapper propagates
# the dev output but does not add the runtime to standalone link searches.
# Follow only this compiler's declared inputs, never arbitrary store versions.
windows_cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
set --
if [ "$("$windows_cc" -print-file-name=libmcfgthread.a)" = libmcfgthread.a ]; then
    cc_root=$(dirname "$(dirname "$(readlink -f "$(command -v "$windows_cc")")")")
    inputs="$cc_root/nix-support/propagated-target-target-deps"
    if [ -f "$inputs" ]; then
        for input in $(cat "$inputs"); do
            runtimes="$input/nix-support/propagated-build-inputs"
            for runtime in "$input" $(cat "$runtimes" 2>/dev/null || :); do
                if [ -f "$runtime/lib/libmcfgthread.a" ]; then
                    set -- "-L$runtime/lib"
                    break
                fi
            done
            [ "$#" -eq 0 ] || break
        done
    fi
fi
"$ziran" build --target=c --no-main --define _WIN32 --define __x86_64__ --root "$repo/tests" \
    --module-path "$repo/src/backend" -o "$work/c" "$repo/tests/app_files_windows_test.zi"
"$windows_cc" "$@" -O2 -std=c11 -ffunction-sections -fdata-sections \
    -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c -Wl,--gc-sections -static -static-libgcc -o "$work/run.exe"
# Debian Wine 10's temporary-directory.patch frees secure_getenv("TMPDIR")
# when /run/user/$uid is absent (Debian #1110936). Unset it only for Wine,
# including wineserver cleanup; Xvfb and compilation keep our private tmpdir.
# The package fix is to duplicate TMPDIR before freeing it in server_tmpdir().
xvfb-run -a -n 400 -s '-screen 0 1280x1000x24' env -u TMPDIR HOME="$work/home" sh -eu -c '
    unset WINEARCH WINELOADER WINEDLLPATH WINEDLLOVERRIDES WINELOADERNOEXEC
    export WINEARCH=win64
    cleanup() {
        result=$?
        trap - EXIT HUP INT TERM
        "$WINESERVER" -k || { cleanup_result=$?; [ "$result" -ne 0 ] || result=$cleanup_result; }
        "$WINESERVER" -w || { cleanup_result=$?; [ "$result" -ne 0 ] || result=$cleanup_result; }
        exit "$result"
    }
    trap cleanup EXIT
    trap "exit 129" HUP
    trap "exit 130" INT
    trap "exit 143" TERM
    # Run the PE program directly: distribution wineboot wrappers vary. Keep
    # errors visible even when the normal Wine debug output is disabled.
    WINEDEBUG=err+all "$1" wineboot.exe --init || {
        result=$?
        echo "Wine bootstrap failed (exit $result)" >&2
        exit "$result"
    }
    rm -rf "$WINEPREFIX/drive_c/kryon-app-files-test"
    "$1" "$2" C:/kryon-app-files-test
' sh "$wine" "$work/run.exe"
echo 'Windows private storage: write result, replacement, nested records and reopen passed'
