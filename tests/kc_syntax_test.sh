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
    for int i = 0; i < 2; i++ {
        if i == 0 {
            continue
        }
        break
        value += i
    }
    while value < 3 {
        value++
    }
    do DrawThing(
        value,
        (ThingSpec){
            .value = value,
            .label = "hello"
        }
    )
    value = value +
            1
    if CheckThing(
        value,
        1) {
        value++
    }
    value = value > 0
        ? value
        : 1
    c {
    value++
    c }
    label: [32] char = {0}
    explicit_count: int = 3
    zero_count: int
    app := (void*)0
    native (void)app
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
grep -q 'continue;' "$out/src/valid.c"
grep -q 'break;' "$out/src/valid.c"
grep -q 'while(value < 3)' "$out/src/valid.c"
grep -q 'DrawThing( value, (ThingSpec){ .value = value, .label = "hello" } );' "$out/src/valid.c"
grep -q 'value = value + 1;' "$out/src/valid.c"
grep -q 'if(CheckThing( value, 1)) {' "$out/src/valid.c"
grep -q 'value = value > 0 ? value : 1;' "$out/src/valid.c"
grep -q '    {' "$out/src/valid.c"
grep -q '    value++;' "$out/src/valid.c"
grep -Fq 'char label[32] = {0};' "$out/src/valid.c"
grep -Fq 'int explicit_count = 3;' "$out/src/valid.c"
grep -Fq 'int zero_count = {0};' "$out/src/valid.c"
grep -Fq '__auto_type app = (void*)0;' "$out/src/valid.c"

{
    printf 'cimport "thing.h"\n\n'
    for i in $(seq 1 40); do
        printf 'pub fn many_%02d() -> int {\n    return %d\n}\n\n' "$i" "$i"
    done
} > "$work/src/many_functions.kry"

"$kc" --no-main --root "$work" -o "$out" "$work/src/many_functions.kry" >"$err" 2>&1
grep -q 'int many_40(void);' "$out/src/many_functions.h"

cat > "$work/src/settings_ui.kry" <<'EOF'
mod settings_ui
cimport "thing.h"

fn helper(value: int) -> int {
    return value + 1
}

pub fn toggle_row_height(label: const char*, w: int) -> int {
    return helper(w)
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/settings_ui.kry" >"$err" 2>&1
grep -q '^settings_ui_helper(int value)' "$out/src/settings_ui.c"
grep -q 'int settings_ui_toggle_row_height(const char\* label, int w);' "$out/src/settings_ui.h"
grep -q 'return settings_ui_helper(w);' "$out/src/settings_ui.c"

cat > "$work/src/settings_session.kry" <<'EOF'
cimport "thing.h"
ui := use "settings_ui"

pub fn draw_session(label: const char*, w: int) -> int {
    return ui.toggle_row_height(label, w)
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/settings_session.kry" >"$err" 2>&1
grep -q '#include "settings_ui.h"' "$out/src/settings_session.h"
grep -q 'return settings_ui_toggle_row_height(label, w);' "$out/src/settings_session.c"

cat > "$work/src/settings_direct.kry" <<'EOF'
cimport "thing.h"
use "settings_ui"

pub fn draw_direct(label: const char*, w: int) -> int {
    return settings_ui.toggle_row_height(label, w)
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/settings_direct.kry" >"$err" 2>&1
grep -q 'return settings_ui_toggle_row_height(label, w);' "$out/src/settings_direct.c"

cat > "$work/src/preview.kry" <<'EOF'
preview stage_preview(viewport: Rectangle) {
    background BLACK
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/preview.kry" >"$err" 2>&1

cat > "$work/src/state_multiline.kry" <<'EOF'
cimport "stddef.h"

state {
    labels: [2] const char* = {
        "one",
        "two",
    }
}

pub fn state_label(index: int) -> const char* {
    return labels[index]
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/state_multiline.kry" >"$err" 2>&1
grep -q 'static const char\* labels\[2\] = {' "$out/src/state_multiline.c"
grep -q '"one",' "$out/src/state_multiline.c"
grep -q '"two",' "$out/src/state_multiline.c"

cat > "$work/src/native_c_features.kry" <<'EOF'
ANDROID :: #defined(ANDROID_BUILD)
FEATURE_ENABLED :: ANDROID

c {
    include "stddef.h"
    define FEATURE_VALUE = 7
    extern fn platform_ping(value: int) -> int #if FEATURE_ENABLED
}

static records: [2] const int = {
    helper_value(),
    FEATURE_VALUE,
}

fn helper_value() -> int {
    return nil == nil ? 1 : 0
}

pub fn native_feature_value() -> int {
    #if FEATURE_ENABLED {
        return platform_ping(records[0])
    } #else {
        return records[1]
    }
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/native_c_features.kry" >"$err" 2>&1
grep -q '#include "stddef.h"' "$out/src/native_c_features.c"
grep -q '#define FEATURE_VALUE 7' "$out/src/native_c_features.c"
grep -q '#if ((defined(ANDROID_BUILD)))' "$out/src/native_c_features.c"
grep -q 'int platform_ping(int value);' "$out/src/native_c_features.c"
grep -q 'static int helper_value(void);' "$out/src/native_c_features.c"
grep -q 'static const int records\[2\] = {' "$out/src/native_c_features.c"
grep -q 'return NULL == NULL ? 1 : 0;' "$out/src/native_c_features.c"
grep -q '#else' "$out/src/native_c_features.c"
grep -q '#endif' "$out/src/native_c_features.c"

cat > "$work/src/native_structs.kry" <<'EOF'
cimport "stddef.h"

pub enum {
    PUBLIC_FIRST = 1
    PUBLIC_SECOND
}

enum {
    LOCAL_FIRST = 1
    LOCAL_SECOND
}

pub struct PublicPair {
    name: const char*
    values: [2] int
}

struct LocalCtx {
    count: int
    pair: PublicPair*
}

pub fn native_struct_count(pair: PublicPair*) -> int {
    ctx: LocalCtx = {0}
    ctx.count = pair != nil ? PUBLIC_SECOND + LOCAL_SECOND : 0
    ctx.pair = pair
    return ctx.count
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/native_structs.kry" >"$err" 2>&1
grep -q 'enum {' "$out/src/native_structs.h"
grep -q 'PUBLIC_FIRST = 1,' "$out/src/native_structs.h"
grep -q 'PUBLIC_SECOND,' "$out/src/native_structs.h"
grep -q 'LOCAL_FIRST = 1,' "$out/src/native_structs.c"
grep -q 'LOCAL_SECOND,' "$out/src/native_structs.c"
grep -q 'typedef struct PublicPair {' "$out/src/native_structs.h"
grep -q 'const char\* name;' "$out/src/native_structs.h"
grep -q 'int values\[2\];' "$out/src/native_structs.h"
grep -q '} PublicPair;' "$out/src/native_structs.h"
grep -q 'typedef struct LocalCtx {' "$out/src/native_structs.c"
grep -q 'PublicPair\* pair;' "$out/src/native_structs.c"
grep -q '} LocalCtx;' "$out/src/native_structs.c"

cat > "$work/src/multiline_fn_decl.kry" <<'EOF'
cimport "stddef.h"

pub fn multiline_sum(first: int,
                     second: int,
                     third: int) -> int {
    return first + second + third
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/multiline_fn_decl.kry" >"$err" 2>&1
grep -q 'int multiline_sum(int first, int second, int third);' "$out/src/multiline_fn_decl.h"
grep -q 'multiline_sum(int first, int second, int third)' "$out/src/multiline_fn_decl.c"

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

cat > "$work/src/bad_var.kry" <<'EOF'
screen bad {
    var label: [32] char = {0}
}
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/bad_var.kry" >"$err" 2>&1; then
    echo "legacy var declaration was accepted" >&2
    exit 1
fi
grep -q "'var' syntax was removed" "$err"
grep -Eq 'bad_var\.kry:2:1: error:' "$err"

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
