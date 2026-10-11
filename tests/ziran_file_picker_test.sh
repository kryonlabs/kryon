#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/bin"
cat > "$work/bin/zenity" <<'FAKE'
#!/bin/sh
printf '%s\n' "$@" >> "$FAKE_ZENITY_LOG"
printf -- '--\n' >> "$FAKE_ZENITY_LOG"
case $FAKE_ZENITY in
    path) printf '/home/test/data file.json\n' ;;
    cancel) exit 1 ;;
    *) exit 127 ;;
esac
FAKE
chmod +x "$work/bin/zenity"
"$ziran_root/build/bin/zi2c" --no-main --define PLATFORM_DESKTOP --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/ziran_file_picker_test.zi"
"${CC:-cc}" -std=c11 -D_DEFAULT_SOURCE -O2 -I"$ziran_root/include" -iquote "$work/c" \
    "$work"/c/*.c -o "$work/run"
status=0
PATH="$work/bin:$PATH" FAKE_ZENITY_LOG="$work/log" "$work/run" || status=$?
if [ "$status" -ne 0 ]; then
    echo "file picker check $status failed" >&2
    cat "$work/log" >&2 || true
    exit 1
fi
# Each argument reaches zenity whole; a save asks before overwriting.
expected=$(printf '%s\n' --file-selection --title 'Open data' --file-filter 'Data | *.json *.csv' -- \
    --file-selection --title 'Save data' --file-filter 'Data | *.json' --save --confirm-overwrite \
    --filename 'inbe export.json' --)
actual=$(sed -n 1,16p "$work/log")
if [ "$actual" != "$expected" ]; then
    printf "%s\n" "zenity arguments differ:" "$actual" >&2
    exit 1
fi
for define in _WIN32 PLATFORM_WEB ANDROID_BUILD; do
    "$ziran_root/build/bin/zi2c" --no-main --define "$define" --root "$repo/src/backend" \
        --module-path "$ziran_root/std" -o "$work/$define" "$repo/src/backend/file_picker.zi"
done
echo 'File picker: zenity arguments, chosen path, cancel, failure and overlong paths passed'
