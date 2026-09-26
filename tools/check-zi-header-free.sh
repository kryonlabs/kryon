#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# The test-only portable host adapter shares the C bundle runtime's callback ABI.
# UI modules still cannot import C headers. Search from the repository root so
# anchored exclusions match relative paths regardless of the caller's directory.
headers=$(cd "$repo" && rg --files . -g '*.h' -g '!vendor/**' -g '!build/**' \
    -g '!tests/support/kryon_portable_host.h' || true)
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

for source in "$repo"/src/plot/*.zi "$repo"/src/data_views/*.zi \
    "$repo"/src/game/*.zi; do
    if rg -n '#import[[:space:]]+"[^\"]+\.h"' "$source"; then
        echo "optional Ziran module imports a C header: $source" >&2
        exit 1
    fi
done
