#!/bin/sh
set -eu

root="${1:-.}"
generated="${2:-}"

cd "$root"

fail=0

if [ -n "$generated" ] && [ -d "$generated" ]; then
    generated_matches="$(
        rg -n '\b(DrawUI[A-Za-z0-9_]*|UIText[A-Za-z0-9_]*|TextInputControl|UIRender[A-Za-z0-9_]*|UINodeId|UIKey|BeginUIFrame|EndUIFrame|TextWrapped|TextColored|TextDisabled|TextInRect)\b' \
            "$generated" \
            --glob '*.c' \
            --glob '*.h' || true
    )"
    if [ -n "$generated_matches" ]; then
        printf '%s\n' "$generated_matches" >&2
        echo "kryon-laws-api: generated output contains blocked runtime names" >&2
        fail=1
    fi
fi

matches="$(
    rg -n '\b(DrawTexture|DrawTexturePro|DrawTextureRec|Texture|UIText[A-Za-z0-9_]*|TextInputControl|UIRender[A-Za-z0-9_]*)\s*\(' \
        examples tests/fixtures \
        --glob '*.kry' || true
)"

if [ -n "$matches" ]; then
    printf '%s\n' "$matches" >&2
    echo "kryon-laws-api: app-facing .kry uses a blocked UI surface" >&2
    fail=1
fi

python3 - <<'PY' || fail=1
from pathlib import Path
import sys

root = Path(".")
errors = []
for header in sorted((root / "include").glob("ui_*_props.generated.h")):
    name = header.name.removeprefix("ui_").removesuffix(".generated.h")
    source = root / "runtime" / f"{name}.kry"
    if not source.exists():
        errors.append(f"{header}: missing generated props source {source}")

if errors:
    for error in errors:
        print(error, file=sys.stderr)
    sys.exit(1)

print("kryon-laws-api: generated props ownership ok")
PY

exit "$fail"
