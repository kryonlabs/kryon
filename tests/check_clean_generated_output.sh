#!/bin/sh
set -eu

target=${1:?usage: check_clean_generated_output.sh PATH}

matches="$(
    rg -n 'DrawUI|UIText|TextInputControl|UIRender|BeginDrawing|EndDrawing|import "C"|go/kryui' "$target" \
        --glob '*.go' \
        --glob '*.c' \
        --glob '*.h' || true
)"

if [ -n "$matches" ]; then
    echo "generated output contains blocked generated-runtime names:" >&2
    echo "$matches" >&2
    exit 1
fi
