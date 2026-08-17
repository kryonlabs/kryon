#!/bin/sh
# Screenshot-exactness harness, stage 2: cartridge render vs the direct
# C/raylib inbe build of the SAME screen.
#
#   tests/krb_exact_native_test.sh <inbe-binary> <scene> <screen.kry>
#
# Renders the native app screenshot (--screenshot scene) at 480x640 dark,
# builds and renders the cartridge at the same size, and pixel-compares.
# Exit 0 only on a 0-pixel difference. The comparator tolerates nothing:
# "100% exact" means identical bytes.
set -eu

inbe=${1:?inbe binary}
scene=${2:?scene name}
kry=${3:?screen .kry}
workdir=${4:-$PWD} # app cwd: repo root (fonts/locales live there)
root=$(dirname "$0")/..
k2b=$root/build/linux-x86_64/bin/k2b
krbrun=$root/build/linux-x86_64/bin/krb-run
work=${TMPDIR:-/tmp}/kryon-krb-native.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work"

# raylib's post-swap glReadPixels reads blank/transparent on software GL
# (llvmpipe, both local headless X and CI xvfb). Xvfb DOES composite GL
# windows into its root framebuffer, so when INBE_SHOT_WINDOW is set the
# app holds the warmed-up scene on screen and we capture with xwd instead.
if [ -n "${KRB_EXACT_XWD:-}" ]; then
    # Xvfb composites GL windows; capture with xwd/imagemagick instead of
    # raylib's post-swap glReadPixels (blank on software GL).
    (cd "$workdir" && INBE_SHOT_WINDOW=1 "$inbe" --screenshot /dev/null \
        --screenshot-scene "$scene" --screenshot-width 480 \
        --screenshot-height 640 --screenshot-dark 1 \
        >/dev/null 2>&1) &
    app_pid=$!
    sleep 8
    win_id=""
    for id in $(xdotool search --onlyvisible --name "" 2>/dev/null); do
        geom=$(xdotool getwindowgeometry --shell "$id" 2>/dev/null)
        w=$(printf '%s\n' "$geom" | awk -F= '/^WIDTH/{print $2}')
        h=$(printf '%s\n' "$geom" | awk -F= '/^HEIGHT/{print $2}')
        if [ "$w" = "480" ] && [ "$h" = "640" ]; then
            win_id=$id
            break
        fi
    done
    if [ -n "$win_id" ] && command -v import >/dev/null; then
        pos=$(xdotool getwindowgeometry --shell "$win_id" | awk -F= '/^POSITION/{gsub(/,/," ",$2); print $2}')
        wx=$(printf '%s\n' "$pos" | awk '{print $1}')
        wy=$(printf '%s\n' "$pos" | awk '{print $2}')
        import -window root -crop 480x640+"$wx"+"$wy" +repage "$work/native.png"
    else
        echo "native: xwd capture unavailable (need xdotool+imagemagick)" >&2
    fi
    kill "$app_pid" 2>/dev/null
    sleep 2
    kill -9 "$app_pid" 2>/dev/null
    wait "$app_pid" 2>/dev/null
else
    # The armed pre-swap capture writes /tmp/kryon-native.png itself
    # (raylib's exporter does not honor the image on this stack).
    rm -f /tmp/kryon-native.png
    (cd "$workdir" && INBE_SHOT_ARM=1 "$inbe" --screenshot /dev/null \
        --screenshot-scene "$scene" --screenshot-width 480 \
        --screenshot-height 640 --screenshot-dark 1 >/dev/null 2>&1) || true
    if [ -s /tmp/kryon-native.png ]; then
        cp /tmp/kryon-native.png "$work/native.png"
    fi
fi

if [ ! -s "$work/native.png" ]; then
    echo "native: no screenshot produced (GL readback broken here?)" >&2
    exit 2
fi

"$k2b" --no-main --root "$(dirname "$kry")" -o "$work" "$kry" >/dev/null 2>&1
KRB_RUN_THEME_LIGHT=1 "$krbrun" --png "$work/cart.png" --w 480 --h 640 "$work/$(basename "${kry%.kry}").krb" >/dev/null

exec python3 - "$work/native.png" "$work/cart.png" <<'EOF'
import struct, sys, zlib

def load(p):
    d = open(p, 'rb').read()
    pos = 8
    idat = b''
    while pos < len(d):
        ln, = struct.unpack('>I', d[pos:pos+4])
        t = d[pos+4:pos+8]
        if t == b'IDAT':
            idat += d[pos+8:pos+8+ln]
        pos += 12 + ln
    raw = zlib.decompress(idat)
    w, h = struct.unpack('>II', d[16:24])
    return raw, w, h

a, aw, ah = load(sys.argv[1])
b, bw, bh = load(sys.argv[2])
if (aw, ah) != (bw, bh):
    print(f'native: size mismatch {aw}x{ah} vs {bw}x{bh}', file=sys.stderr)
    sys.exit(1)
diff = 0
first = []
for y in range(ah):
    for x in range(aw):
        pa = a[y*(1+aw*4)+1+x*4:y*(1+aw*4)+1+x*4+4]
        pb = b[y*(1+bw*4)+1+x*4:y*(1+bw*4)+1+x*4+4]
        if pa != pb:
            diff += 1
            if len(first) < 4:
                first.append((x, y, pa.hex(), pb.hex()))
print(f'native exact: {diff} differing pixels of {aw*ah}')
for f in first:
    print('  at', f)
sys.exit(0 if diff == 0 else 1)
EOF
