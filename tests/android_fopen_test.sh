#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY KRYON_TEST_DRIVE
work="$repo/work/android-fopen"
rm -rf "$work"
mkdir -p "$work/c"
"$ziran" build --target=c --root "$repo/src/backend" --module-path "$ziran_root/std" \
    --no-main -o "$work/c" "$repo/src/backend/android_fopen.zi"
cat > "$work/main.c" <<'C'
#include <stdio.h>
// raylib's reader dereferences the missing activity; reaching it is the crash.
static int asset_calls;
void *KryonRaylibFopen(const char *path, const char *mode) { (void)path; (void)mode; asset_calls++; return NULL; }
int main(void)
{
    FILE *file = fopen("/dev/urandom", "rb");
    if (file == NULL || asset_calls != 0) return 1;
    fclose(file);
    if (fopen("assets/picture.png", "rb") != NULL || asset_calls != 1) return 2;
    return 0;
}
C
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" -I"$work/c" "$work/main.c" "$work/c"/*.c \
    -Wl,--wrap=fopen -o "$work/test"
"$work/test"
printf '%s\n' 'Android fopen: absolute paths open without an activity, others stay with raylib'
