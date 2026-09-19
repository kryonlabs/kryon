#!/bin/sh
# Law-plan clean-room probe (phase 10 first patch).
#
# Builds and runs a downstream-style native program using only the released
# compiler (k2c) and a C toolchain, in an environment where every Node.js or
# Bend invocation fails immediately: proof tooling must never be required to
# compile, build or run released Kryon code. The shims write a marker file
# if they are ever invoked, so silent success cannot hide a hidden call.
set -eu

root="${1:-.}"
cd "$root"

status=0
note() { printf 'law-cleanroom: %s\n' "$1"; }
fail() { printf 'law-cleanroom: FAIL %s\n' "$1" >&2; status=1; }

k2c="${KRYON_CLEANROOM_K2C:-build/$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)/bin/k2c}"
cc="${CC:-cc}"
if [ ! -x "$k2c" ]; then
    fail "k2c not found at $k2c (build it first)"
    exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/kryon-law-cleanroom.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM
mkdir -p "$work/shims" "$work/src" "$work/out"

# Poisoned shims: any invocation records itself and fails.
for tool in node nodejs bend bun deno; do
    shim="$work/shims/$tool"
    printf '#!/bin/sh\necho "invoked" >> "%s/marker"\nexit 70\n' "$work" > "$shim"
    chmod +x "$shim"
done

cat > "$work/src/app.kry" <<'EOF'
Triple :: (v: i32) -> i32 #export {
    return v * 3
}
EOF

cat > "$work/src/main.c" <<'EOF'
#include "app.h"
#include <stdio.h>
int main(void) {
    if (Triple(14) != 42) {
        fprintf(stderr, "cleanroom: wrong result\n");
        return 1;
    }
    return 0;
}
EOF

PATH="$work/shims:$PATH" "$k2c" --strict --no-main --root "$work/src" -o "$work/out" "$work/src/app.kry"
PATH="$work/shims:$PATH" "$cc" -std=c99 -O2 -Wall -Werror \
    -I"$work/out" -I"$root/include" \
    "$work/src/main.c" "$work/out/app.c" -lm -o "$work/app"
PATH="$work/shims:$PATH" "$work/app"

if [ -f "$work/marker" ]; then
    fail "a proof-tier tool was invoked during the clean-room build:"
    cat "$work/marker" >&2
else
    note "k2c + $cc compiled and ran a downstream program with node/bend poisoned"
    note "PASS released toolchain works without proof dependencies"
fi
exit "$status"
