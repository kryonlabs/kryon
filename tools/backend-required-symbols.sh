#!/bin/sh
# Report the kryon-surface symbols kryon's own code actually calls: the
# subset a non-raylib backend must implement for real (see docs/BACKENDS.md).
# The full surface is larger; anything not listed here may be a zero-return
# stub like the generated null backend provides.
#
# Scans src/, cmd/ (which also emits the calls generated .kry apps make),
# tests/, and examples/*.c for call sites of every surface symbol. The public
# surface declarations in include/ are not call sites and are skipped.
#
# Usage: sh tools/backend-required-symbols.sh [repo-root]

set -eu

root=${1:-.}
header="$root/include/kryon_compat.generated.h"

if [ ! -f "$header" ]; then
    echo "surface header not found: $header" >&2
    exit 1
fi

symbols=$(
    awk '
        /^[[:space:]]*(RLAPI|RMAPI)[[:space:]]/ {
            line = $0
            sub(/\/\/.*$/, "", line)
            sub(/\(.*/, "", line)
            n = split(line, parts, /[[:space:]\*]+/)
            if(n > 0 && parts[n] ~ /^[A-Za-z_][A-Za-z0-9_]*$/)
                print parts[n]
        }
    ' "$header" | sort -u
)

for symbol in $symbols; do
    count=$(
        grep -RlE "(^|[^A-Za-z0-9_])$symbol\(" \
            "$root/src" "$root/cmd" "$root/tests" \
            --include='*.c' 2>/dev/null | wc -l
    )
    examples_count=$(
        grep -lE "(^|[^A-Za-z0-9_])$symbol\(" \
            "$root"/examples/*.c 2>/dev/null | wc -l
    )
    if [ "$count" -gt 0 ] || [ "$examples_count" -gt 0 ]; then
        printf '%s\n' "$symbol"
    fi
done
