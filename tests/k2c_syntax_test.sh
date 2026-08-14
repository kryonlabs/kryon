#!/bin/sh
# k2c syntax test — verifies the Kir-based .kry->C pipeline output.
set -eu

k2c=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2c build/*/bin/k2c 2>/dev/null | head -1)}
work=${TMPDIR:-/tmp}/kryon-k2c-syntax-test.$$
root=$(pwd)

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -f "$k2c" ]; then
    echo "k2c not found: $k2c" >&2
    exit 1
fi

mkdir -p "$work/src" "$work/out"

cat > "$work/src/valid.kry" <<'EOF'
#import "kryon.h"

state {
    count: int = 7
    label: [64] char = "hello"
}

screen Valid(viewport: Rectangle) {
    Background(GetThemeBackground())
    Text("hi", ScaleUIPx(10), ScaleUIPx(10), UI_TEXT_16, GetThemeText())
    value := count + 1
    unused value
    if count == nil {
        count = 0
    }
    else if count > 10 {
        count = 1
    }
    else {
        count = 2
    }
    while count < 3 {
        count++
    }
    defer count = 0
}

knr_branches :: (n: int) -> int {
    if n > 0 {
        return 100
    } else if n < 0 {
        return 200
    } else {
        return 300
    }
}
EOF

"$k2c" --root "$work" -o "$work/out" "$work/src/valid.kry"

c="$work/out/src/valid.c"
h="$work/out/src/valid.h"

test -f "$c"
test -f "$h"

# header: guard + include + prototype with converted args
grep -Fq '#ifndef K_SRC_VALID_H' "$h"
grep -Fq '#include "kryon.h"' "$h"
grep -Fq 'void Valid_kry_draw(Rectangle viewport);' "$h"

# source: preamble
grep -Fq '#include "src/valid.h"' "$c"
grep -Fq '#include "ui_inspect.h"' "$c"

# state fields (array type converts to C declarator order)
grep -Fq 'static int count = 7;' "$c"
grep -Fq 'static char label[64] = "hello";' "$c"

# function definition
grep -Fq 'Valid_kry_draw(Rectangle viewport)' "$c"

# calls wrap with Push/Pop + source line
grep -Fq 'PushUIInspectSource("src/valid.kry", 9);' "$c"
grep -Fq 'Background(GetThemeBackground());' "$c"
grep -Fq 'PopUIInspectSource();' "$c"

# inferred decl
grep -Fq '__auto_type value = count + 1;' "$c"

# unused
grep -Fq '(void)value;' "$c"

# nil rewrites to NULL, if/else-if/else chains with parens
grep -Fq 'if(count == NULL) {' "$c"
grep -Fq '} else if(count > 10) {' "$c"
grep -Fq '} else {' "$c"

# K&R '} else {' lines: close + re-open recorded on the else statement,
# bodies must survive (regression: the else branch used to be dropped)
grep -Fq 'if(n > 0) {' "$c"
grep -Fq 'return 100;' "$c"
grep -Fq '} else if(n < 0) {' "$c"
grep -Fq 'return 200;' "$c"
grep -Fq 'return 300;' "$c"

# while with parens
grep -Fq 'while(count < 3) {' "$c"

# defer splices at function end (no 'defer' survives)
grep -Fq 'count = 0;' "$c"
if grep -Fq 'defer ' "$c"; then
    echo "defer keyword survived in output" >&2
    exit 1
fi

# the generated C compiles
cc -fsyntax-only -I"$root/include" -I"$work/out" "$c"

echo "k2c ok"
