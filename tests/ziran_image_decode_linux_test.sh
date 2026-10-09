#!/bin/sh
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
. "$root/tests/toolchain.sh"
out="$root/build/scratch/image-decode"
mkdir -p "$out"
python3 - "$out/original.png" <<'PY'
from PIL import Image
import sys
image=Image.new('RGBA',(5000,1000),(0,200,0,255))
image.putpixel((4500,300),(200,0,0,128))
image.save(sys.argv[1])
PY
for target in c cpp; do
    "$ziran" build --root "$root/tests" --module-path "$root/src/backend" --target="$target" --no-main -o "$out/$target" "$root/tests/image_decode_linux_test.zi"
    suffix=c; header=h; compiler=${CC:-cc}; standard=c11
    if [ "$target" = cpp ]; then suffix=cpp; header=hpp; compiler=${CXX:-c++}; standard=c++17; fi
    printf '#include "image_decode_linux_test.%s"\nint main(int argc, char **argv) { return image_decode_linux_test_main(argc, (uint8_t **)argv); }\n' "$header" > "$out/$target/driver.$suffix"
    "$compiler" -std="$standard" -O2 -I"$ziran_root/include" -I"$out/$target" "$out/$target"/*."$suffix" -ldl -o "$out/$target/run"
    env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER "$out/$target/run" "$out/original.png"
done
