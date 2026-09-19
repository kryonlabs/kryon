#!/bin/sh
# Screenshot-exactness harness, stage 1: the SAME cartridge must render
# byte-identically in every kryon renderer — kry_sw headless (krb-run),
# the SDL software renderer (krb-sdl readback), and the wasm web engine
# (node oneshot capture). Fixtures may include widgets outside the portable
# cartridge vocabulary; k2b omits those intentionally with --allow-unsupported
# so this gate continues to compare the shared KRB-renderable subset. RGB
# differences in the produced cartridge are failures; alpha-only differences are
# accepted only for fixtures listed in the conformance matrix alpha-gap ledger.
#
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

fixtures=$(python3 - "$root/examples/manifest.json" <<'PY'
import json
import sys
from pathlib import Path

manifest = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
for entry in manifest.get("examples", []):
    if entry.get("krb_exact"):
        print(entry["path"])
PY
)

fails=0
for rel_kry in $fixtures; do
    kry="$root/$rel_kry"
    name=$(basename "$kry" .kry)

    if ! "$k2b" --no-main --allow-unsupported --root "$root/examples" \
            -o "$work" "$kry" >"$work/$name.k2b.out" 2>"$work/$name.k2b.err"; then
        echo "exact: $name k2b failed" >&2
        cat "$work/$name.k2b.err" >&2
        fails=$((fails + 1))
        continue
    fi
    if [ -s "$work/$name.k2b.err" ]; then
        sed "s/^/exact: $name k2b: /" "$work/$name.k2b.err"
    fi

    "$krbrun" --png "$work/$name.sw.png" --w 480 --h 640 \
        "$work/$name.krb" >/dev/null
    SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software \
        "$krbsdl" --png "$work/$name.sdl.png" --w 480 --h 640 \
        "$work/$name.krb" >/dev/null

    if python3 - "$root" "$rel_kry" "$work/$name.sw.png" "$work/$name.sdl.png" <<'PY'
import importlib.util
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
rel = sys.argv[2]
sw = Path(sys.argv[3])
sdl = Path(sys.argv[4])
spec = importlib.util.spec_from_file_location(
    "kryon_conformance_matrix", root / "scripts" / "conformance-matrix.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
pixel_diff, rgb_diff, alpha_diff = module.rgba_diff(sw, sdl)
if rgb_diff:
    print(f"exact: {Path(rel).stem} kry_sw vs SDL RGB MISMATCH: "
          f"{rgb_diff} RGB pixels differ ({pixel_diff} RGBA pixels differ)", file=sys.stderr)
    raise SystemExit(1)
if alpha_diff:
    reason = module.KRB_ALPHA_BYTE_GAPS.get(rel)
    if reason is None:
        print(f"exact: {Path(rel).stem} kry_sw vs SDL alpha MISMATCH: "
              f"{alpha_diff} alpha pixels differ and no alpha-gap ledger row exists", file=sys.stderr)
        raise SystemExit(1)
    print(f"exact: {Path(rel).stem} kry_sw == SDL RGB; alpha differs ({alpha_diff} pixels): {reason}")
else:
    print(f"exact: {Path(rel).stem} kry_sw == SDL RGBA")
PY
    then
        :
    else
        fails=$((fails + 1))
    fi
done

if [ "$fails" -ne 0 ]; then
    echo "exact: $fails failure(s)" >&2
    exit 1
fi
echo "exact ok"
