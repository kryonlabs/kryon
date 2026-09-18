#!/bin/sh
# Generated web/*.js artifacts must be untracked and reproducible from
# runtime/*.kry; the .kry sources are the single source of truth.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

platform=$(uname -s | tr '[:upper:]' '[:lower:]')
k2js=${K2JS:-"$root/build/$platform-$(uname -m)/bin/k2js"}
if [ ! -x "$k2js" ]; then
    echo "web generated check: compiler missing (run: make k2js)" >&2
    exit 1
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/kryon-web-generated.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM
"$k2js" --strict --no-main --root runtime --runtime ./kryon-runtime.js \
    -o "$work" runtime/kss_parser.kry runtime/kss_formatter.kry \
    runtime/text_input.kry runtime/style_sheet.kry runtime/style.kry \
    runtime/surface.kry runtime/control_props.kry runtime/drawing_props.kry runtime/instance.kry

status=0
for name in kss_parser kss_formatter text_input style_sheet style surface control_props drawing_props instance; do
    if git ls-files --error-unmatch "web/$name.js" >/dev/null 2>&1; then
        echo "generated artifact must not be tracked: web/$name.js" >&2
        status=1
    fi
    generated="$work/$name.js"
    if ! cmp -s "$generated" "web/$name.js"; then
        echo "web/$name.js differs from k2js output of runtime/$name.kry" >&2
        echo "  regenerate with: make -W runtime/kss_parser.kry -W runtime/instance.kry generate-web-runtime" >&2
        status=1
    fi
done
if [ "$status" -ne 0 ]; then
    exit 1
fi
echo "web generated modules ok"
