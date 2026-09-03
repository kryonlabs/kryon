#!/bin/sh
set -eu

k2kir=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2kir build/*/bin/k2kir 2>/dev/null | head -1)}
root=${2:-.}
work=${TMPDIR:-build}/kryon-k2kir-test.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

if [ ! -f "$k2kir" ]; then
    echo "k2kir not found: $k2kir" >&2
    exit 1
fi

mkdir -p "$work/src" "$work/out"
cat > "$work/src/app.kry" <<'EOF'
#module "demo.app"
#import "kryon.h"
ui :: #import "src/ui/panel"

WEB :: #defined(PLATFORM_WEB)
ALWAYS :: 1
ANSWER :: #run 6 * 7
#assert ANSWER == 42, "fixture #run assertion should pass"
#assert ALWAYS, "fixture assertion should pass"
platform_ping :: (value: int, tag: const char*) -> int #extern
native_abs :: (value: int) -> int #extern "c.abs"

state {
    click_count: int = 0
    label: [32] char = "hello"
}

Counter :: (app: App*) {
    Text("Count")
    if Button("Increment") {
        app->click_count += 1
    }
    value := click_count + 1
    field_value := app->click_count
    index_value := label[0]
    neg_value := -index_value
    float_value := 1.5
    size_value := sizeof(label)
    compound_value := (Rectangle){0,0,1,1}
}
EOF

"$k2kir" --root "$work" -o "$work/out" "$work/src/app.kry"
kir=$work/out/src/app.kir
test -f "$kir"
grep -Fq 'kir 1' "$kir"
grep -Fq 'module demo.app source src/app.kry span src/app.kry:1:1' "$kir"
grep -Fq 'import header kryon.h target kryon.h' "$kir"
grep -Fq 'import module ui target src/ui/panel' "$kir"
grep -Fq 'import extern platform_ping target platform_ping' "$kir"
grep -Fq 'import extern native_abs target c.abs extern_kind c extern_symbol abs' "$kir"
grep -Fq 'signature platform_ping :: (value: int, tag: const char*) -> int #extern' "$kir"
grep -Fq 'assert condition (42) == 42 known 1 value 1 message "fixture #run assertion should pass"' "$kir"
grep -Fq 'assert condition (1) known 1 value 1 message "fixture assertion should pass"' "$kir"
grep -Fq 'state click_count type int init 0' "$kir"
grep -Fq 'function Counter args app: App* return void' "$kir"
grep -Fq 'stmt widget widget Text args "Count" text Text("Count")' "$kir"
grep -Fq 'expr call text Text("Count") name Text op' "$kir"
grep -Fq 'expr string text "Count" name  op' "$kir"
grep -Fq 'stmt if widget  args  text if Button("Increment") {' "$kir"
grep -Fq 'expr call text Button("Increment") name Button op' "$kir"
grep -Fq 'stmt assign widget  args  text app->click_count += 1' "$kir"
grep -Fq 'expr int text 1 name  op' "$kir"
grep -Fq 'stmt decl widget  args  text value := click_count + 1' "$kir"
grep -Fq 'expr binary text click_count + 1 name  op +' "$kir"
grep -Fq 'expr ident text click_count name click_count op' "$kir"
grep -Fq 'expr pointer_member text app->click_count name click_count op ->' "$kir"
grep -Fq 'expr index text label[0] name  op' "$kir"
grep -Fq 'expr unary text -index_value name  op -' "$kir"
grep -Fq 'expr float text 1.5 name  op' "$kir"
grep -Fq 'expr sizeof text sizeof(label) name  op' "$kir"
grep -Fq 'expr compound text (Rectangle){0,0,1,1} name  op' "$kir"
if grep -Fq 'function WEB' "$kir"; then
    echo "top-level #defined binding was emitted as a function" >&2
    exit 1
fi

cat > "$work/src/assert_fail.kry" <<'EOF'
#import "kryon.h"
#assert 1 + 1 == 3, "constant assertion failed"
EOF

if "$k2kir" --root "$work" -o "$work/out" "$work/src/assert_fail.kry" 2>"$work/assert_fail.err"; then
    echo "false constant #assert did not fail during Kry parsing" >&2
    exit 1
fi
grep -Fq 'constant assertion failed' "$work/assert_fail.err"

cat > "$work/src/bad_c_extern.kry" <<'EOF'
#import "kryon.h"
bad :: (value: int) -> int #extern "c.1bad"
EOF

if "$k2kir" --root "$work" -o "$work/out" "$work/src/bad_c_extern.kry" 2>"$work/bad_c_extern.err"; then
    echo "invalid C extern target did not fail during Kry parsing" >&2
    exit 1
fi
grep -Fq 'C extern target must be c.<symbol>' "$work/bad_c_extern.err"

echo "k2kir ok"
