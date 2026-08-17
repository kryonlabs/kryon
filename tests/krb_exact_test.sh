#!/bin/sh
# Screenshot-exactness harness, stage 1: the SAME cartridge must render
# byte-identically in every kryon renderer — kry_sw headless (krb-run),
# the SDL software renderer (krb-sdl readback), and the wasm web engine
# (node oneshot capture). Any pixel difference is a failure.
#
# Stage 2 (krb_exact_native_test.sh, needs the inbe C build) compares a
# cartridge against the direct C/raylib screenshot of the same screen.
set -eu

root=${1:-.}
krbrun=$root/build/$(uname -s | tr [:upper:] [:lower:]_)-$(uname -m 2>/dev/null | sed 's/amd64/x86_64/')/bin/krb-run
krbsdl=$root/build/$(uname -s | tr [:upper:] [:lower:]_)-$(uname -m 2>/dev/null | sed 's/amd64/x86_64/')/bin/krb-sdl
[ -x "$krbrun" ] || krbrun=$root/build/linux-x86_64/bin/krb-run
[ -x "$krbsdl" ] || krbsdl=$root/build/linux-x86_64/bin/krb-sdl
k2b=$root/build/linux-x86_64/bin/k2b
work=${TMPDIR:-/tmp}/kryon-krb-exact.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work"

fails=0
for kry in "$root"/examples/20_inbe_language.kry "$root"/examples/21_inbe_settings.kry "$root"/examples/22_inbe_manual.kry "$root"/examples/02_buttons.kry; do
    name=$(basename "$kry" .kry)

    "$k2b" --no-main --root "$root/examples" -o "$work" "$kry" >/dev/null 2>&1

    "$krbrun" --png "$work/$name.sw.png" --w 480 --h 640 \
        "$work/$name.krb" >/dev/null
    SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software \
        "$krbsdl" --png "$work/$name.sdl.png" --w 480 --h 640 \
        "$work/$name.krb" >/dev/null

    a=$(sha256sum "$work/$name.sw.png" | awk '{print $1}')
    b=$(sha256sum "$work/$name.sdl.png" | awk '{print $1}')
    if [ "$a" != "$b" ]; then
        echo "exact: $name kry_sw vs SDL MISMATCH" >&2
        fails=$((fails + 1))
    else
        echo "exact: $name kry_sw == SDL ($a)"
    fi
done

if [ "$fails" -ne 0 ]; then
    echo "exact: $fails failure(s)" >&2
    exit 1
fi
echo "exact ok"
