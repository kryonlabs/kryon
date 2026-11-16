#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
out="$repo/build/scratch/native-image-services"
mkdir -p "$out"
python3 tests/image_pixels_fixtures.py
python3 - "$out/huge.png" <<'PY'
import struct, sys, zlib
from pathlib import Path
def chunk(tag, data):
    return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
# Valid header with an image size beyond the native decoder budget.
Path(sys.argv[1]).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 40000, 2, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b'\0')) + chunk(b'IEND', b''))
PY
export KRYON_AVIF_LIBRARY="$repo/build/avif-ziran/libkryonavif.so"
"$ziran" ir --project --module-path "$repo/tests" --root "$repo/tests" --define PLATFORM_DESKTOP \
    -o "$out/ir" tests/native_image_services_test.zi
for form in source saved; do
    if [ "$form" = source ]; then
        LDLIBS=-ldl "$ziran" build --project --module-path "$repo/tests" --root "$repo/tests" \
            --define PLATFORM_DESKTOP --entry native_image_services_test:main --target=c --exe -o "$out/$form" tests/native_image_services_test.zi
    else
        LDLIBS='-lSDL2 -lpangocairo-1.0 -lpango-1.0 -lcairo -lfontconfig -lfreetype -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lX11 -lm -lpthread -ldl' \
            "$ziran" build --root "$out/ir" --module-path "$out/ir" --entry native_image_services_test:main --target=c --exe \
            -o "$out/$form" "$out/ir/native_image_services_test.zir"
    fi
    if ldd "$out/$form/native_image_services_test" | rg -qi raylib; then
        echo 'Unexpected raylib dependency in native image services' >&2
        exit 1
    fi
    SDL_VIDEODRIVER=x11 xvfb-run -a -s '-screen 0 800x600x24 -nolisten tcp' \
        "$out/$form/native_image_services_test"
done
