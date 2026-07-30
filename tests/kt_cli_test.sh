#!/bin/sh
set -eu

case $1 in
/*) kt=$1 ;;
*) kt=$(pwd)/$1 ;;
esac
tmp=${TMPDIR:-/tmp}/kryon-kt-test-$$

cleanup() {
    rm -rf "$tmp"
}
trap cleanup EXIT INT TERM

mkdir -p "$tmp/tests"
cat >"$tmp/tests/editor.kt" <<'EOF'
# kt parser smoke
open .
tap @source
type "print(\"hi\")"
key C-s
see "Saved"
shot editor
EOF

(
    cd "$tmp"
    "$kt" -headless tests/editor.kt
)

test -d "$tmp/logs"
test -f "$tmp/logs/kt.log"
test -d "$tmp/snapshots"
grep -q 'tests/editor.kt:2: open .' "$tmp/logs/kt.log"
grep -q 'tests/editor.kt:7: shot editor' "$tmp/logs/kt.log"

cat >"$tmp/tests/bad.kt" <<'EOF'
tap @source extra
EOF

if (cd "$tmp" && "$kt" tests/bad.kt >/dev/null 2>&1); then
    echo "kt accepted trailing text" >&2
    exit 1
fi
