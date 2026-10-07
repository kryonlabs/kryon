#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$root/tests/toolchain.sh"
ziran_dir=$ziran_root
compiler=${ZI2C_BIN:-"$ziran_dir/build/bin/zi2c"}
standard=${ZIRAN_STD:-"$ziran_dir/std"}
raylib_source=${RAYLIB_SOURCE:-$("$ziran" pkg path raylib)}
raylib=${RAYLIB_A:-"$raylib_source/src/libraylib.a"}
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
work=$(mktemp -d "$root/build/raylib-clip.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$compiler" --no-main --root "$root/tests" \
    --module-path "$root/src/backend" --module-path "$root/src/ui" \
    --module-path "$standard" -o "$work/c" \
    "$root/tests/raylib_clip_behavior.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$ziran_dir/include" -iquote "$work/c" \
    "$work/c"/*.c "$raylib" $raylib_libs \
    -o "$work/test"
python3 - "$work/red.png" "$work/split.png" <<'PY'
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
pixels = b''.join(b'\0' + bytes((230, 20, 20, 255)) * 8 +
                  bytes((20, 20, 230, 255)) * 8 for _ in range(16))
Path(sys.argv[2]).write_bytes(b'\x89PNG\r\n\x1a\n' +
    chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 16, 8, 6, 0, 0, 0)) +
    chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))
PY
cd "$work"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY \
    -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 \
    timeout 20s xvfb-run -a -n 300 -s '-screen 0 640x480x24' ./test
echo 'Raylib translated/scaled text and image clips, including nesting: passed'
