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
"$ziran" build --target=c --no-main --define _WIN32 --define __x86_64__ --root "$repo/tests" \
    --module-path "$repo/src/backend" -o "$work/c" "$repo/tests/app_files_windows_test.zi"
"${CC:-x86_64-w64-mingw32-gcc}" -O2 -std=c11 -ffunction-sections -fdata-sections \
    -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c -Wl,--gc-sections -static -static-libgcc -o "$work/run.exe"
xvfb-run -a -n 400 -s '-screen 0 1280x1000x24' sh -eu -c '
    trap "wineserver -k" EXIT HUP INT TERM
    wineboot --init
    rm -rf "$WINEPREFIX/drive_c/kryon-app-files-test"
    wine "$1" C:/kryon-app-files-test
' sh "$work/run.exe"
echo 'Windows private storage: write result, replacement, nested records and reopen passed'
