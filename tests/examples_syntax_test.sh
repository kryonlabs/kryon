#!/bin/sh
set -eu

root="${1:-.}"
k2c="${2:-}"
changed="${KRYON_CHANGED_EXAMPLES:-}"

cd "$root"

if [ -z "$k2c" ]; then
    host="build/$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)"
    if [ -f "$host/bin/k2c" ]; then
        k2c="$host/bin/k2c"
    else
        k2c="$(ls build/*/bin/k2c 2>/dev/null | head -1)"
    fi
fi

if [ ! -f "$k2c" ]; then
    echo "k2c not found: $k2c" >&2
    exit 1
fi

if [ -z "$changed" ]; then
    changed="$(git ls-files 'examples/*.kry')"
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/kryon-examples-syntax.XXXXXX")"
cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

for path in $changed; do
    case "$path" in
        examples/*.kry)
            [ -f "$path" ] || continue
            out="$work/$(basename "$path" .kry)"
            mkdir -p "$out"
            "$k2c" --root examples -o "$out" "$path"
            ;;
    esac
done

echo "examples syntax ok"
