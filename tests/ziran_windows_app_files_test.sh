#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY LISTEN_PID LISTEN_FDS LISTEN_FDNAMES NOTIFY_SOCKET
export YUE_DESKTOP_RECOVERY=0 WINEDEBUG=-all
work="$repo/work/windows-app-files"
git -C "$repo" check-ignore -q work/windows-app-files/probe
mkdir -p "$work/home" "$work/config" "$work/data" "$work/cache" "$work/runtime"
chmod 700 "$work/runtime"
export HOME="$work/home" XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" XDG_CACHE_HOME="$work/cache" XDG_RUNTIME_DIR="$work/runtime" WINEPREFIX="$work/prefix"
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
"$ziran" build --target=c --no-main --define _WIN32 --define __x86_64__ --root "$repo/tests" \
    --module-path "$repo/src/backend" -o "$work/c" "$repo/tests/app_files_windows_test.zi"
"${WINDOWS_CC:-x86_64-w64-mingw32-gcc}" -O2 -std=c11 -ffunction-sections -fdata-sections \
    -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c -Wl,--gc-sections -static -static-libgcc -o "$work/run.exe"
xvfb-run -a -n 400 -s '-screen 0 1280x1000x24' sh -eu -c '
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
