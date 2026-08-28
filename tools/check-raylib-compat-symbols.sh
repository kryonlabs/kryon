#!/bin/sh
set -eu

raylib_header=${1:-vendor/raylib/src/raylib.h}
rename_header=${2:-build/generated/raylib_backend_rename.h}
wrapper_source=${3:-build/generated/kryon_raylib_wrappers.c}
# Front-end sources that define surface symbols once for every backend
# (kept in sync with shared_symbols in generate-kryon-compat.sh).
frontend_sources="${4:-src/backend/kry_surface_math.c} src/backend/kry_screenshot.c src/backend/kry_event_wait.c src/backend/kry_instance.c src/platform/open_uri.c"

if [ ! -f "$raylib_header" ]; then
    echo "raylib header not found: $raylib_header" >&2
    exit 1
fi
if [ ! -f "$rename_header" ]; then
    echo "rename header not found: $rename_header" >&2
    exit 1
fi
if [ ! -f "$wrapper_source" ]; then
    echo "wrapper source not found: $wrapper_source" >&2
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
    ' "$raylib_header" | sort -u
)

status=0

for symbol in $symbols; do
    if ! grep -Eq "^#define[[:space:]]+$symbol[[:space:]]+" "$rename_header"; then
        echo "missing backend rename for raylib symbol: $symbol" >&2
        status=1
    fi
    defined=0
    if grep -Eq "[[:space:]\*]($symbol|BackendRaw_$symbol)\\(" "$wrapper_source"; then
        defined=1
    fi
    for frontend in $frontend_sources; do
        if [ -f "$frontend" ] && grep -Eq "[[:space:]\*]$symbol\\(" "$frontend"; then
            defined=1
        fi
    done
    if [ "$defined" -ne 1 ]; then
        echo "missing public wrapper for raylib symbol: $symbol" >&2
        status=1
    fi
done

exit "$status"
