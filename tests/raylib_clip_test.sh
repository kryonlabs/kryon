#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
compiler=${ZI2C_BIN:-"$root/../ziran/build/bin/zi2c"}
standard=${ZIRAN_STD:-"$root/../ziran/std"}
raylib=${RAYLIB_A:-"$root/vendor/raylib/src/libraylib.a"}
work=$(mktemp -d "$root/build/raylib-clip.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$compiler" --no-main --root "$root/tests" \
    --module-path "$root/src/backend" --module-path "$root/src/ui" \
    --module-path "$standard" -o "$work/c" \
    "$root/tests/raylib_clip_behavior.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$root/../ziran/include" -iquote "$work/c" \
    "$work/c"/*.c "$raylib" ${RAYLIB_LIBS:--lGL -lX11 -lm -ldl -pthread} \
    -o "$work/test"
python3 - "$work/red.png" <<'PY'
import struct
import sys
import zlib
from pathlib import Path

def chunk(kind, data):
    payload = kind + data
    return struct.pack('>I', len(data)) + payload + struct.pack('>I', zlib.crc32(payload))

pixels = b''.join(b'\0' + bytes((230, 20, 20, 255)) * 16 for _ in range(16))
Path(sys.argv[1]).write_bytes(b'\x89PNG\r\n\x1a\n' +
    chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 16, 8, 6, 0, 0, 0)) +
    chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))
PY
cd "$work"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY \
    timeout 20s xvfb-run -a -n 300 -s '-screen 0 640x480x24' ./test
echo 'Raylib translated/scaled text and image clips, including nesting: passed'
