#!/bin/sh
# Law-plan dependency boundary probe (phase 1).
#
# Maintainer/CI proof dependencies (Bend, Node proof tooling, law packages)
# must never leak into released surfaces: public headers, the compiler and
# runtime sources that ship to downstream apps and users. This probe scans
# those surfaces for proof-tier references and records the pinned Bend
# checkout as the baseline proof asset. It is a source-tree boundary check;
# the phase 10 clean-room package/install probes extend it.
set -eu

root="${1:-.}"
cd "$root"

status=0
note() { printf 'law-boundary: %s\n' "$1"; }
fail() { printf 'law-boundary: FAIL %s\n' "$1" >&2; status=1; }

# 1. Released source surfaces must not reference proof-tier dependencies.
pattern='vendor/bend|bend-laws|bend-pin|PROOF\.bend|LAWS\.bend|checkLaws|laws/inventory|laws/focus'
for dir in include cmd runtime; do
    if [ -d "$dir" ]; then
        if grep -rInE "$pattern" "$dir" >/dev/null 2>&1; then
            fail "$dir references proof-tier dependencies:"
            grep -rInE "$pattern" "$dir" >&2 || true
        else
            note "$dir is free of proof-tier references"
        fi
    fi
done

# 2. The pinned Bend checkout is the baseline proof asset.
pinned=$(sed -n 's/.*"commit"[: ]*"\([0-9a-f]\{40\}\)".*/\1/p' tools/bend-pin.json | head -n 1)
if [ -z "$pinned" ]; then
    fail "tools/bend-pin.json has no 40-hex commit pin"
else
    if [ -d vendor/bend/.git ] || git -C vendor/bend rev-parse --git-dir >/dev/null 2>&1; then
        actual=$(git -C vendor/bend rev-parse HEAD)
        if [ "$pinned" = "$actual" ]; then
            note "vendor/bend matches the pinned checker commit $pinned"
        else
            fail "vendor/bend is at $actual but the pin says $pinned"
        fi
    else
        fail "vendor/bend is not initialized; the pin cannot be verified"
    fi
fi

# 3. The maintained runtime stays in .kry; law packages stay under laws/.
stray=$(find . -maxdepth 3 -name '*.bend' -not -path './laws/*' -not -path './vendor/*' -not -path './build/*' -print -quit 2>/dev/null || true)
if [ -n "$stray" ]; then
    fail "Bend proof package outside laws/: $stray"
else
    note "no Bend packages outside laws/"
fi

if [ "$status" -eq 0 ]; then
    note "PASS released surfaces are free of proof dependencies"
fi
exit "$status"
