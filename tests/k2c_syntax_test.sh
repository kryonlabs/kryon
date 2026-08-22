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

# Anonymous enums are declarations, not top-level `#` comments.
#enum {
    FixtureCount = 4
    FixtureLimit = 12
}

state {
    count: int = FixtureCount
    label: [64] char = "hello"
    field_text: [64] char = "account"
    field_cursor: int = 7
    area_text: [128] char = "notes"
    area_cursor: int = 5
    area_scroll: int = 0
}

screen Valid(viewport: Rectangle) {
    int legacy_count
    BeginFrame()
    Background(GetThemeBackground())
    Text("hi", ScaleUIPx(10), ScaleUIPx(10), Text16, GetThemeText())
    Column((ColumnProps){.bounds = {ScaleUIPx(4), ScaleUIPx(40), ScaleUIPx(180), ScaleUIPx(120)}, .gap = ScaleUIPx(4), .padding = ScaleUIPx(6), .key = Key("form")})
    TextField((TextFieldProps){.bounds = {0, 0, ScaleUIPx(160), ScaleUIPx(24)}, .text = field_text, .text_size = sizeof(field_text), .cursor_position = &field_cursor, .focused = NULL, .max_codepoints = 63, .font = Text16, .focus_id = 101})
    TextArea((TextAreaProps){.bounds = {0, 0, ScaleUIPx(160), ScaleUIPx(48)}, .text = area_text, .text_size = sizeof(area_text), .cursor_position = &area_cursor, .focused = NULL, .scroll_y = &area_scroll, .max_codepoints = 127, .font = Text16, .line_gap = ScaleUIPx(4), .focus_id = 102, .placeholder = "Notes"})
    Row((RowProps){.bounds = {0, 0, ScaleUIPx(160), ScaleUIPx(32)}, .gap = ScaleUIPx(4), .padding = 0, .key = Key("actions")})
    Button((ButtonProps){.bounds = {0, 0, ScaleUIPx(70), ScaleUIPx(28)}, .label = "Save", .style = ButtonStylePrimary, .font = Text16, .id = 103})
    End()
    End()
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
    EndFrame()
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

WEB :: #defined(PLATFORM_WEB)
ANDROID :: ANDROID_BUILD
ALWAYS :: 1
ANSWER :: #run 6 * 7
#assert ANSWER == 42, "valid #run assertion failed"
#assert ALWAYS, "valid fixture assertion failed"
#if WEB {
    #assert 0, "web-only fixture assertion failed"
}

chain_root :: () -> const char* {
    cached: const char* = nil

    #if WEB {
        cached = "web"
    } #else_if ANDROID {
        cached = "android"
    } #else {
        cached = "desktop"
    }
    return cached
}

concat_sql :: () -> const char* {
    sql: const char* = "INSERT INTO t("
        "a, b, c) "
        "VALUES(1,2,3)"
    return sql
}
EOF

"$k2c" --root "$work" -o "$work/out" "$work/src/valid.kry"
sh "$root/tests/check_clean_generated_output.sh" "$work/out"

c="$work/out/src/valid.c"
h="$work/out/src/valid.h"

test -f "$c"
test -f "$h"

# header: guard + include + prototype with converted args
grep -Fq '#ifndef K_SRC_VALID_H' "$h"
grep -Fq '#include "kryon.h"' "$h"
grep -Fq 'void Valid_kry_draw(Rectangle viewport);' "$h"
grep -Fq 'FixtureCount = 4,' "$h"
grep -Fq 'FixtureLimit = 12,' "$h"

# source: preamble
grep -Fq '#include "src/valid.h"' "$c"
grep -Fq '#include "ui_inspect.h"' "$c"

# state fields (array type converts to C declarator order)
grep -Fq 'static int count = FixtureCount;' "$c"
grep -Fq 'static char label[64] = "hello";' "$c"
grep -Fq 'static char field_text[64] = "account";' "$c"
grep -Fq 'static int field_cursor = 7;' "$c"
grep -Fq 'static char area_text[128] = "notes";' "$c"
grep -Fq 'static int area_cursor = 5;' "$c"
grep -Fq 'static int area_scroll = 0;' "$c"

# function definition
grep -Fq 'Valid_kry_draw(Rectangle viewport)' "$c"
grep -Fq 'int legacy_count;' "$c"

# calls wrap with Push/Pop + source line
grep -Fq 'PushUIInspectSource("src/valid.kry", 21);' "$c"
grep -Fq 'BeginFrame();' "$c"
grep -Fq 'Background(GetThemeBackground());' "$c"
grep -Fq 'Text("hi", ScaleUIPx(10), ScaleUIPx(10), Text16, GetThemeText());' "$c"
grep -Fq 'Column((ColumnProps)' "$c"
grep -Fq 'TextField((TextFieldProps)' "$c"
grep -Fq 'TextArea((TextAreaProps)' "$c"
grep -Fq 'Row((RowProps)' "$c"
grep -Fq 'Button((ButtonProps)' "$c"
grep -Fq 'EndFrame();' "$c"
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

# '#if/#else_if/#else' chains with K&R '} #else_if {' closers: every branch
# must lower and compile (regression: chains past the first region vanished,
# leaving e.g. an empty data root on desktop builds)
grep -Fq '#if (defined(PLATFORM_WEB))' "$c"
grep -Fq '"web"' "$c"
grep -Fq '#elif' "$c"
grep -Fq '"android"' "$c"
grep -Fq '#else' "$c"
grep -Fq '"desktop"' "$c"
grep -Fq '#endif' "$c"

# top-level #assert emits a native compile-time check
grep -Fq '#if !((42) == 42)' "$c"
grep -Fq '#error "valid #run assertion failed"' "$c"
grep -Fq '#if !((1))' "$c"
grep -Fq '#error "valid fixture assertion failed"' "$c"
grep -Fq '#if (defined(PLATFORM_WEB))' "$c"
grep -Fq '#error "web-only fixture assertion failed"' "$c"

# adjacent string literals on continuation lines join into ONE statement
# (regression: each fragment became an orphan expression statement and the
# initializer kept only its first fragment — SQL prepared from it failed)
grep -Fq 'const char* sql = "INSERT INTO t(" "a, b, c) " "VALUES(1,2,3)";' "$c"
if grep -Fq '"a, b, c) ";' "$c"; then
    echo "string fragment leaked as an orphan statement" >&2
    exit 1
fi

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

cat > "$work/src/assert_fail.kry" <<'EOF'
#import "kryon.h"
NEVER :: 0
#assert NEVER, "intentional assertion failure"

screen AssertFail(viewport: Rectangle) {
    Background(GetThemeBackground())
}
EOF

if "$k2c" --root "$work" -o "$work/out" "$work/src/assert_fail.kry" 2>"$work/assert_fail.err"; then
    echo "false #assert did not fail during Kry parsing" >&2
    exit 1
fi
grep -Fq 'intentional assertion failure' "$work/assert_fail.err"

echo "k2c ok"
