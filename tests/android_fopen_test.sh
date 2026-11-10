#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY KRYON_TEST_DRIVE
work="$repo/work/android-fopen"
rm -rf "$work"
mkdir -p "$work/c"
"$ziran" build --target=c --define ANDROID_BUILD \
    --root "$repo/src/backend" --module-path "$ziran_root/std" \
    --no-main -o "$work/c" "$repo/src/backend/android_fopen.zi"
cat > "$work/main.c" <<'C'
#include <stdio.h>
void *AAssetManager_open(void *manager, const char *name, int mode)
{
    (void)manager; (void)name; (void)mode; return 0;
}
void AAsset_close(void *asset) { (void)asset; }
int AAsset_getLength(void *asset) { (void)asset; return 0; }
void *AAsset_getBuffer(void *asset) { (void)asset; return 0; }
void *funopen(void *cookie, void *read, void *write, void *seek, void *close)
{
    (void)cookie; (void)read; (void)write; (void)seek; (void)close;
    return 0;
}
void ANativeActivity_finish(void *activity) { (void)activity; }
int main(void)
{
    FILE *file = fopen("/dev/urandom", "rb");
    if (file == NULL) return 1;
    fclose(file);
    if (fopen("assets/picture.png", "rb") != NULL) return 2;
    return 0;
}
C
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" -I"$work/c" "$work/main.c" "$work/c"/*.c \
    -Wl,--wrap=fopen -Wl,--wrap=fopen64 -o "$work/test"
"$work/test"
printf '%s\n' 'Android fopen: absolute paths open without an activity; relative assets need one'
