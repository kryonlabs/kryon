#!/bin/sh
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
. "$root/tests/toolchain.sh"
out="$root/build/scratch/video-test"
mkdir -p "$out"
ffmpeg -v error -nostdin -y -f lavfi -i testsrc2=size=160x90:rate=24 -f lavfi -i sine=frequency=440:sample_rate=44100 -t 4 -c:v libx264 -pix_fmt yuv420p -c:a aac -threads 1 "$out/fixture.mp4"
LDLIBS='-ldl' "$ziran" build --root "$root/tests" --module-path "$root/src/backend" --target=c --entry video_linux_test:main --exe -o "$out/native" "$root/tests/video_linux_test.zi"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER KRYON_VIDEO_TEST_SILENT=1 "$out/native/video_linux_test" "$out/fixture.mp4"
