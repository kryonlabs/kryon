#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
out="$repo/build/scratch/android-image-services"
mkdir -p "$out"
python3 tests/image_pixels_fixtures.py
python3 - "$out" <<'PY'
import struct, sys
from PIL import Image
from pathlib import Path
out = Path(sys.argv[1])
# Large art with fine details and alpha: the retained reduction must match
# Pillow's premultiplied Lanczos result, including hidden edge colors.
large = Image.new('RGBA', (2048, 1024))
large.putdata([((x * 17) % 256, (y * 31) % 256, 255 if x % 3 else 0,
                (x + y) % 256) for y in range(1024) for x in range(2048)])
large.save(out / 'large.webp', lossless=True)
(out / 'large.rgba').write_bytes(large.resize((256, 128), Image.Resampling.LANCZOS).tobytes())
out.joinpath('huge.png').write_bytes(b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 40000, 2))
# A header-only JPEG drives the fake activity, rather than pretending to
# exercise Android's BitmapFactory on this desktop test host.
tiff = b'II' + struct.pack('<HIH', 42, 8, 1) + struct.pack('<HHII', 274, 3, 1, 6) + bytes(4)
exif = b'Exif\0\0' + tiff
sof = bytes([8]) + struct.pack('>HH', 1, 2) + bytes([1, 1, 0x11, 0])
out.joinpath('orientation6.jpg').write_bytes(b'\xff\xd8\xff\xe1' + struct.pack('>H', len(exif) + 2) + exif + b'\xff\xc0' + struct.pack('>H', len(sof) + 2) + sof + b'\xff\xd9')
PY
"$ziran" ir $kryon_project_args --root "$repo/tests" --define ANDROID_BUILD --entry android_image_services_test:main -o "$out/ir" "$repo/tests/android_image_services_test.zi"
for form in source saved; do
    input="$repo/tests/android_image_services_test.zi"; root="$repo/tests"
    if test "$form" = saved; then input="$out/ir/android_image_services_test.zir"; root="$out/ir"; fi
    output="$out/$form"
    "$ziran" build $kryon_project_args --target=c --root "$root" --define ANDROID_BUILD --entry android_image_services_test:main --prune-stale -o "$output" "$input"
    includes=$(find "$output" -type d -printf ' -iquote %p')
    # shellcheck disable=SC2046,SC2086
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
        $(find "$output" -name '*.c') "$repo/build/webp-ziran/libkryonwebp.a" \
        "$repo/build/avif-ziran/avif/libavif.a" "$repo/build/avif-ziran/aom/libaom.a" \
        "$ziran_root/build/libziran.a" -Wl,--gc-sections -lz -ldl -lm -lpthread -o "$output/test"
    "$output/test"
done
