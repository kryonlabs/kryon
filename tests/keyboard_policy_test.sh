#!/bin/sh
set -eu
build=${1:-build/linux-x86_64}
work=$(mktemp -d "${TMPDIR:-/tmp}/kryon-keyboard-policy.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM
"$build/bin/k2js" --strict --no-main --root runtime \
    --runtime ./kryon-runtime.js -o "$work" runtime/*.kry
cp web/kryon-runtime.js web/text_edit.js "$work/"
cp tests/keyboard_policy_test.mjs tests/web_text_edit_test.mjs "$work/"
printf '%s\n' '{"type":"module"}' > "$work/package.json"
node "$work/keyboard_policy_test.mjs"
node "$work/web_text_edit_test.mjs"
