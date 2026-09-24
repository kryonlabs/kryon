#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
headers=$(rg --files "$repo" -g '*.h' -g '!vendor/**' -g '!build/**' || true)
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
