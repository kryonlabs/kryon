#!/bin/sh
# web/*.js runtime modules must stay byte-identical to the k2js output of
# runtime/*.kry; the .kry sources are the single source of truth.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

status=0
for name in kss_parser text_input style_sheet style surface control_props drawing_props; do
    generated="build/$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)/web-text/$name.js"
    if [ ! -f "$generated" ]; then
        echo "web generated check: $generated missing (run: make \$(BUILD_DIR)/web-text/$name.js)" >&2
        status=1
        continue
    fi
    if ! cmp -s "$generated" "web/$name.js"; then
        echo "web/$name.js differs from k2js output of runtime/$name.kry" >&2
        echo "  regenerate with: make \$(BUILD_DIR)/web-text/$name.js" >&2
        status=1
    fi
done
if [ "$status" -ne 0 ]; then
    exit 1
fi
echo "web generated modules ok"
