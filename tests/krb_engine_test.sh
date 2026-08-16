#!/bin/sh
# Engine conformance test (plan 11, phase 1): compile an example cartridge,
# render it headless with the kry_sw engine wrapped in the recorder, and
# compare both the frame PNG and the vtable call stream against committed
# goldens. KRB_ENGINE_REGOLDEN=1 refreshes the golden files.
set -eu

k2b=${1:?k2b path}
krbrun=${2:?krb-run path}
root=${3:-.}
golden_dir=$root/tests/golden/krb-engine
work=${TMPDIR:-/tmp}/kryon-krb-engine-test.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work" "$golden_dir"

"$k2b" --root "$root/examples" -o "$work" "$root/examples/02_buttons.kry" \
    >/dev/null 2>&1
if [ ! -f "$work/02_buttons.krb" ]; then
    echo "krb_engine_test: k2b produced no cartridge" >&2
    exit 1
fi

"$krbrun" --png "$work/frame.png" --record "$work/stream.txt" \
    --w 400 --h 300 "$work/02_buttons.krb" >/dev/null

calls=$(wc -l < "$work/stream.txt")
if [ "$calls" -lt 10 ]; then
    echo "krb_engine_test: call stream suspiciously short: $calls" >&2
    exit 1
fi

if [ "${KRB_ENGINE_REGOLDEN:-0}" = "1" ]; then
    sha256sum "$work/frame.png" | awk '{print $1}' > "$golden_dir/frame.png.sha256"
    sha256sum "$work/stream.txt" | awk '{print $1}' > "$golden_dir/stream.txt.sha256"
    echo "krb_engine_test: goldens refreshed (calls=$calls)"
    exit 0
fi

for golden in frame.png stream.txt; do
    if [ ! -f "$golden_dir/$golden.sha256" ]; then
        echo "krb_engine_test: no golden for $golden; run with KRB_ENGINE_REGOLDEN=1" >&2
        exit 1
    fi
    want=$(cat "$golden_dir/$golden.sha256")
    got=$(sha256sum "$work/$golden" | awk '{print $1}')
    if [ "$want" != "$got" ]; then
        echo "krb_engine_test: $golden diverged from golden (want $want got $got)" >&2
        exit 1
    fi
done

echo "krb engine ok calls=$calls"
