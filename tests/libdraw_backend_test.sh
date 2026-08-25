#!/bin/sh
# libdraw backend test - builds and runs a clean-surface Kryon app against
# plan9port devdraw under Xvfb when available.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
plan9=${PLAN9PORT_DIR:-/mnt/storage/Projects/plan9port}
build=${BUILD_DIR:-build/linux-x86_64-libdraw}
bin="$root/$build/tests/libdraw_smoke_test"
hierarchy_bin="$root/$build/tests/libdraw_hierarchy_test"
work=${TMPDIR:-/tmp}/kryon-libdraw-test.$$

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -x "$plan9/bin/devdraw" ] || [ ! -d "$plan9/include" ]; then
    echo "libdraw test: plan9port not found at $plan9 - skipping" >&2
    exit 0
fi

mkdir -p "$work"
make -C "$root" BUILD_DIR="$build" KRYON_BACKEND=libdraw \
    PLAN9PORT_DIR="$plan9" "$build/tests/libdraw_smoke_test"
make -C "$root" BUILD_DIR="$build" KRYON_BACKEND=libdraw \
    PLAN9PORT_DIR="$plan9" "$build/tests/libdraw_hierarchy_test"

out="$work/libdraw-smoke.png"
if command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run -a env PLAN9="$plan9" PATH="$plan9/bin:$PATH" \
        DEVDRAW="$plan9/bin/devdraw" KRYON_LIBDRAW_SMOKE_OUT="$out" "$bin"
    xvfb-run -a env PLAN9="$plan9" PATH="$plan9/bin:$PATH" \
        DEVDRAW="$plan9/bin/devdraw" "$hierarchy_bin"
elif [ -n "${DISPLAY:-}" ]; then
    env PLAN9="$plan9" PATH="$plan9/bin:$PATH" \
        DEVDRAW="$plan9/bin/devdraw" KRYON_LIBDRAW_SMOKE_OUT="$out" "$bin"
    env PLAN9="$plan9" PATH="$plan9/bin:$PATH" \
        DEVDRAW="$plan9/bin/devdraw" "$hierarchy_bin"
else
    echo "libdraw test: no DISPLAY and xvfb-run not found - skipping runtime" >&2
    exit 0
fi

test -s "$out"
echo "libdraw backend smoke ok"
