#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/android-egl-composition"
mkdir -p "$out"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0 EGL_PLATFORM=surfaceless
export LIBGL_ALWAYS_SOFTWARE=1
"$ziran" ir $kryon_project_args --target=c --root "$repo/tests" --entry android_egl_composition_test:main \
    -o "$out/ir" "$repo/tests/android_egl_composition_test.zi"
LDLIBS="-lEGL -lGLESv2 -lGLX -lm" "${ziran%/*}/zi2c" --entry android_egl_composition_test:main \
    --exe --root "$out/ir" -o "$out/c" "$out/ir/"*.zir
"$out/c/android_egl_composition_test"
printf '%s\n' 'EGL composition, window blit, retained damage, snapshots, and renderer restart passed'
