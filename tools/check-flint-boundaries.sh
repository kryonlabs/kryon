#!/bin/sh
set -eu

root=${1:-.}
status=0

check_direct_raylib_includes() {
    rg '#include[[:space:]]+[<"]raylib\.h[>"]' "$root"/include "$root"/src "$root"/examples "$root"/tests 2>/dev/null || true
}

matches=$(check_direct_raylib_includes)
if [ -n "$matches" ]; then
    echo "Direct raylib.h includes found; include kryon.h instead:" >&2
    echo "$matches" >&2
    status=1
fi

if rg 'vendor/.*/raylib/src|\bRAYLIB_DIR\b|libraylib\.a' "$root"/include "$root"/src "$root"/examples "$root"/tests 2>/dev/null; then
    echo "Backend Raylib details leaked into app-facing code." >&2
    status=1
fi

exit "$status"
