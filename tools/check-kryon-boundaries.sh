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

legacy_api_matches=$(
    rg '\bUIRender[A-Za-z0-9_]*\b|\bKKey(Pressed|Down)\b|\bK(SetKey|UpdateKey)[A-Za-z0-9_]*\b|\bK_KEY_[A-Z0-9_]+\b|kryon_(draw|input|types)\.h' \
        "$root"/include "$root"/src "$root"/examples "$root"/tests "$root"/cmd 2>/dev/null || true
)
if [ -n "$legacy_api_matches" ]; then
    echo "Legacy Kryon compatibility API found; use the canonical Kryon-owned API directly:" >&2
    echo "$legacy_api_matches" >&2
    status=1
fi

exit "$status"
