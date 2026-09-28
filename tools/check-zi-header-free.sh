#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# UI modules cannot import C headers. Search from the repository root so
# anchored exclusions match relative paths regardless of the caller's directory.
headers=$(cd "$repo" && rg --files . -g '*.h' -g '!build/**' || true)
if test -n "$headers"; then
    printf 'handwritten headers remain in Kryon:\n%s\n' "$headers" >&2
    exit 1
fi

while IFS= read -r module; do
    test -n "$module" || continue
    source="$repo/src/ui/$module"
    if rg -n '#import[[:space:]]+"[^\"]+\.h"' "$source"; then
        echo "checked Ziran module imports a C header: $module" >&2
        exit 1
    fi
done < "$repo/src/ui/modules.txt"

for source in "$repo"/src/plot/*.zi "$repo"/src/data_views/*.zi; do
    if rg -n '#import[[:space:]]+"[^\"]+\.h"' "$source"; then
        echo "optional Ziran module imports a C header: $source" >&2
        exit 1
    fi
done
