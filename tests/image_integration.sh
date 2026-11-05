#!/bin/sh
# Run under build-queue. Resolve ordinary package exports and real native
# dependency artifacts; no renderer, graphics context or display is needed.
set -eu
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
cd "$repo"
. "$repo/tests/toolchain.sh"
export ZIRAN="$ziran"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
out="$repo/build/scratch/image-integration"
mkdir -p "$out/tmp" "$out/fixtures"
export TMPDIR="$out/tmp"
python3 - "$repo/tests/fixtures/webp/lossless.webp" "$out/fixtures" <<'PY'
from pathlib import Path
import struct, sys
source = Path(sys.argv[1]).read_bytes()
at = 12
payload = None
while at < len(source):
    count = struct.unpack_from('<I', source, at + 4)[0]
    if source[at:at + 4] == b'VP8L': payload = source[at:at + 8 + count + (count & 1)]
    at += 8 + count + (count & 1)
assert payload is not None
def le24(value): return value.to_bytes(3, 'little')
def chunk(tag, data): return tag + struct.pack('<I', len(data)) + data + b'\0' * (len(data) & 1)
extended = bytes([0x12, 0, 0, 0]) + le24(39999) + le24(1)
frame = le24(0) + le24(0) + le24(2) + le24(1) + le24(100) + b'\x02' + payload
body = b'WEBP' + chunk(b'VP8X', extended) + chunk(b'ANIM', bytes(6)) + chunk(b'ANMF', frame)
out = Path(sys.argv[2])
def write(name, body): (out / name).write_bytes(b'RIFF' + struct.pack('<I', len(body)) + body)
write('wide-canvas.webp', body)
extended = bytes([0x12, 0, 0, 0]) + le24(2) + le24(1)
write('many-frames.webp', b'WEBP' + chunk(b'VP8X', extended) + chunk(b'ANIM', bytes(6)) + chunk(b'ANMF', frame) * 5000)
extended = bytes([0x18, 0, 0, 0]) + le24(2) + le24(1)
write('large-metadata.webp', b'WEBP' + chunk(b'VP8X', extended) + payload + chunk(b'EXIF', bytes(1048577)))
PY
"$ziran" lock --offline >"$out/package-lock.log" 2>&1
for request in avif:native webp:libwebp; do
    name=${request%%:*}
    if ! "$ziran" pkg artifact "$request" --platform linux-x86_64-gnu --offline >"$out/$name-descriptor" 2>"$out/$name-artifact.log"; then
        tail -60 "$out/$name-artifact.log" >&2
        exit 1
    fi
done
avif_dir=$(dirname -- "$(cat "$out/avif-descriptor")")
webp_dir=$(dirname -- "$(cat "$out/webp-descriptor")")
if test "$#" = 0; then set -- image_integration image_adapter_contract image_dynamic_integration; fi
for test_name do
    stage="$out/$test_name"
    mkdir -p "$stage"
    "$ziran" ir --project --root "$repo/tests" --module-path "$repo/tests" \
        -o "$stage/ir" "$repo/tests/$test_name.zi"
    for form in source saved; do
        input="$repo/tests/$test_name.zi"
        if test "$form" = saved; then input="$stage/ir/$test_name.zir"; fi
        output="$stage/$form"
        if test "$form" = source; then
            "$ziran" build --project --target=c --root "$repo/tests" --module-path "$repo/tests" -o "$output" "$input"
        else
            "$ziran" build --target=c --root "$stage/ir" --module-path "$stage/ir" -o "$output" "$input"
        fi
        includes=$(find "$output" -type d -printf ' -iquote %p')
        if test "$test_name" = image_integration; then
            set -- "$avif_dir/lib/libavif.a" "$avif_dir/lib/libaom.a" "$webp_dir/lib/libimagewebp.a"
        else
            set --
        fi
        # shellcheck disable=SC2046,SC2086
        "${CC:-cc}" -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
            -I"$ziran_root/include" $includes $(find "$output" -name '*.c') "$@" -ldl -lm -o "$output/test"
        if test "$test_name" = image_integration; then
            set -- "$repo/tests/fixtures/avif" "$repo/tests/fixtures/webp" "$out/fixtures"
        elif test "$test_name" = image_dynamic_integration; then
            set -- "$out/fixtures"
        else
            set -- "$repo/tests/fixtures/avif"
        fi
        ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
            timeout --kill-after=2s 45s "$output/test" "$@"
    done
done
printf '%s\n' 'Local image integration tests passed: ordinary package artifacts, source and saved IR, ASan/UBSan/leak detection'
