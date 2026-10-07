#!/bin/sh
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
. "$root/tests/toolchain.sh"
out=$(mktemp -d "$root/build/media-check-XXXXXX")
trap 'rm -rf "$out"' EXIT HUP INT TERM
mkdir "$out/bin"
python3 - "$out/tone.wav" <<'PY'
import sys, wave
with wave.open(sys.argv[1], 'wb') as file:
    file.setnchannels(1)
    file.setsampwidth(2)
    file.setframerate(8000)
    file.writeframes(b'\0\0' * 8000 * 60)
PY
# A controlled YouTube transport fixture exercises the owned streaming pipe
# with real ffplay decoding. It never calls the live site or uses credentials.
cat > "$out/bin/yt-dlp" <<'PY'
#!/usr/bin/python3
import os, shutil, sys
assert sys.argv[-2:] == ['--', 'https://www.youtube.com/watch?v=dQw4w9WgXcQ']
assert sys.argv[sys.argv.index('-o') + 1] == '-'
with open(os.environ['MEDIA_TEST_FILE'], 'rb') as source:
    shutil.copyfileobj(source, sys.stdout.buffer)
PY
chmod +x "$out/bin/yt-dlp"
for test in media_linux_test remote_media_test; do
    LDLIBS='-lglib-2.0 -lpthread' "$ziran" build --root "$root/tests" \
        --module-path "$root/src/backend" --target=c --entry "$test:main" --exe \
        -o "$out/$test" "$root/tests/$test.zi"
    env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
        SDL_AUDIODRIVER=dummy MEDIA_TEST_FILE="$out/tone.wav" \
        PATH="$out/bin:$PATH" "$out/$test/$test"
done
