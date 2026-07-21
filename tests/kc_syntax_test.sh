#!/bin/sh
set -eu

kc=${1:-build/bin/kc}
work=${TMPDIR:-/tmp}/kryon-kc-syntax-test.$$
out=$work/out
err=$work/err

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work/src" "$out"

cat > "$work/src/valid.kry" <<'EOF'
screen valid {
    do InitializeThing()
    draw DrawThing()
    set value = 1
    native NativeThing()
    c #if 0
    c #endif
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/valid.kry" >"$err" 2>&1

cat > "$work/src/implicit_call.kry" <<'EOF'
screen bad {
    InitializeThing()
}
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/implicit_call.kry" >"$err" 2>&1; then
    echo "implicit call was accepted" >&2
    exit 1
fi
grep -q 'implicit call statements are not allowed' "$err"
grep -Eq 'implicit_call\.kry:2:1: error:' "$err"

cat > "$work/src/implicit_assignment.kry" <<'EOF'
screen bad {
    value = 1
}
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/implicit_assignment.kry" >"$err" 2>&1; then
    echo "implicit assignment was accepted" >&2
    exit 1
fi
grep -q 'implicit assignments are not allowed' "$err"
grep -Eq 'implicit_assignment\.kry:2:1: error:' "$err"

cat > "$work/src/unbalanced.kry" <<'EOF'
screen bad {
    do InitializeThing()
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/unbalanced.kry" >"$err" 2>&1; then
    echo "unbalanced braces were accepted" >&2
    exit 1
fi
grep -q 'unbalanced braces' "$err"
grep -Eq 'unbalanced\.kry:[0-9]+:1: error:' "$err"
