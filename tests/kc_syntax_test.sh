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
cimport "thing.h"

screen valid {
    first, second := 0
    first, second = 1
    value := first + second
    do InitializeThing()
    draw DrawThing()
    value = 1
    first, second = second, first
    native NativeThing()
    c #if 0
    c #endif
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/valid.kry" >"$err" 2>&1
grep -q '#include "thing.h"' "$out/src/valid.h"
grep -q '__auto_type first = 0;' "$out/src/valid.c"
grep -q '__auto_type second = 0;' "$out/src/valid.c"
grep -Eq '__auto_type __kryon_assign_[0-9]+_0 = 1;' "$out/src/valid.c"
grep -Eq 'first = __kryon_assign_[0-9]+_0;' "$out/src/valid.c"
grep -Eq 'second = __kryon_assign_[0-9]+_0;' "$out/src/valid.c"
grep -Eq '__auto_type __kryon_assign_[0-9]+_0 = second;' "$out/src/valid.c"
grep -Eq '__auto_type __kryon_assign_[0-9]+_1 = first;' "$out/src/valid.c"
grep -Eq 'first = __kryon_assign_[0-9]+_0;' "$out/src/valid.c"
grep -Eq 'second = __kryon_assign_[0-9]+_1;' "$out/src/valid.c"

cat > "$work/src/preview.kry" <<'EOF'
preview stage_preview(viewport: Rectangle) {
    background BLACK
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/preview.kry" >"$err" 2>&1

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

cat > "$work/src/bad_multi_decl.kry" <<'EOF'
screen bad {
    a, b := 1, 2, 3
}
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/bad_multi_decl.kry" >"$err" 2>&1; then
    echo "bad multi-value declaration was accepted" >&2
    exit 1
fi
grep -q 'inferred declaration count mismatch' "$err"
grep -Eq 'bad_multi_decl\.kry:2:1: error:' "$err"

cat > "$work/src/bad_multi_assignment.kry" <<'EOF'
screen bad {
    a, b = 1, 2, 3
}
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/bad_multi_assignment.kry" >"$err" 2>&1; then
    echo "bad multi-value assignment was accepted" >&2
    exit 1
fi
grep -q 'assignment count mismatch' "$err"
grep -Eq 'bad_multi_assignment\.kry:2:1: error:' "$err"

cat > "$work/src/bad_include.kry" <<'EOF'
include "thing.h"

screen bad {
}
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/bad_include.kry" >"$err" 2>&1; then
    echo "legacy include was accepted" >&2
    exit 1
fi
grep -q 'unknown top-level statement: include "thing.h"' "$err"
grep -Eq 'bad_include\.kry:1:1: error:' "$err"

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
