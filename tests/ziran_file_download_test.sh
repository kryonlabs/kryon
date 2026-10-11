#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
server=
cleanup() {
    if [ -n "$server" ]; then kill "$server" 2>/dev/null || true; fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM
"$ziran_root/build/bin/zi2c" --no-main --define PLATFORM_DESKTOP --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/ziran_file_download_test.zi"
"${CC:-cc}" -std=c11 -D_DEFAULT_SOURCE -O2 -I"$ziran_root/include" -iquote "$work/c" \
    "$work"/c/*.c -o "$work/run"
python3 "$repo/tests/http_transfer_server.py" > "$work/port" 2> "$work/server.log" &
server=$!
for attempt in $(seq 1 100); do
    [ -s "$work/port" ] && break
    sleep 0.05
done
(cd "$work" && env -u http_proxy -u https_proxy -u HTTP_PROXY -u HTTPS_PROXY -u ALL_PROXY \
    HTTP_TEST_PORT="$(cat "$work/port")" ./run)
for define in _WIN32 PLATFORM_WEB ANDROID_BUILD; do
    "$ziran_root/build/bin/zi2c" --no-main --define "$define" --root "$repo/src/backend" \
        --module-path "$ziran_root/std" -o "$work/$define" "$repo/src/backend/file_download.zi"
done
echo 'File download: complete file, missing, size limit, release, unreachable network and one at a time passed'
