#!/bin/sh
set -eu

k2c=${1:-build/bin/k2c}
work=${TMPDIR:-/tmp}/kryon-k2c-syntax-test.$$
out=$work/out
err=$work/err

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work/src" "$out"

cat > "$work/src/valid.kry" <<'EOF'
#import "thing.h"

shared_count :: int #global #export
local_counter :: int = 7 #global

screen valid {
    first, second := 0
    first, second = 1
    value := first + second
    unused second
    InitializeThing()
    DrawThing()
    value = 1
    first, second = second, first
    NativeThing()
    c #if 0
    c #endif
    for int i = 0; i < 2; i++ {
        if i == 0 {
            continue
        }
        break
        value += i
        value %= 2
        value &= 3
        value |= 4
        value ^= 1
        value <<= 1
        value >>= 1
    }
    while value < 3 {
        value++
    }
    DrawThing(
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
    if value > 0 &&
       first >= 0 {
        value++
    }
    for int j = 0;
        j < 2;
        j++ {
        value += j
    }
    value = value > 0
        ? value
        : 1
    {
        scoped: int = 9
        value = scoped
    }
    label: [32] char = {0}
    int c_count
    char c_label[16]
    Color c_color
    void* c_ptr
    explicit_count: int = 3
    zero_count: int
    text: int = 1
    text = 2
    path: const char*
    if path == nil {
        path = "fallback"
    }
    if value < 0 {
        value = 0
    }
    else if value == 0 {
        value = 1
    }
    else {
        value = 2
    }
    switch value {
    case 1:
        value = 2
        break
    default:
        value = 3
    }
    if value > 4 {
        goto done
    }
    value = 4
done:
    value++
    enum {
        LOCAL_ACTION_NONE,
        LOCAL_ACTION_RUN
    }
    value = LOCAL_ACTION_RUN
    app := (void*)0
    unused app
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/valid.kry" >"$err" 2>&1
grep -q '#include "thing.h"' "$out/src/valid.h"
grep -Fq 'extern int shared_count;' "$out/src/valid.h"
grep -Fq 'int shared_count;' "$out/src/valid.c"
grep -Fq 'int local_counter = 7;' "$out/src/valid.c"
grep -q '__auto_type first = 0;' "$out/src/valid.c"
grep -q '__auto_type second = 0;' "$out/src/valid.c"
grep -Eq '__auto_type __kryon_assign_[0-9]+_0 = 1;' "$out/src/valid.c"
grep -Eq 'first = __kryon_assign_[0-9]+_0;' "$out/src/valid.c"
grep -Eq 'second = __kryon_assign_[0-9]+_0;' "$out/src/valid.c"
grep -Eq '__auto_type __kryon_assign_[0-9]+_0 = second;' "$out/src/valid.c"
grep -Eq '__auto_type __kryon_assign_[0-9]+_1 = first;' "$out/src/valid.c"
grep -Eq 'first = __kryon_assign_[0-9]+_0;' "$out/src/valid.c"
grep -Eq 'second = __kryon_assign_[0-9]+_1;' "$out/src/valid.c"
grep -q '(void)second;' "$out/src/valid.c"
grep -q '(void)app;' "$out/src/valid.c"
grep -q 'continue;' "$out/src/valid.c"
grep -q 'break;' "$out/src/valid.c"
grep -Fq 'value += i;' "$out/src/valid.c"
grep -Fq 'value %= 2;' "$out/src/valid.c"
grep -Fq 'value &= 3;' "$out/src/valid.c"
grep -Fq 'value |= 4;' "$out/src/valid.c"
grep -Fq 'value ^= 1;' "$out/src/valid.c"
grep -Fq 'value <<= 1;' "$out/src/valid.c"
grep -Fq 'value >>= 1;' "$out/src/valid.c"
grep -q 'while(value < 3)' "$out/src/valid.c"
grep -q 'DrawThing( value, (ThingSpec){ .value = value, .label = "hello" } );' "$out/src/valid.c"
grep -q 'value = value + 1;' "$out/src/valid.c"
# A call used as an if condition is wrapped so it registers a source location
# for click-to-source: Push, evaluate into a temp, Pop, then test the temp.
grep -Eq '__auto_type __kryon_cond_[0-9]+ = CheckThing\( value, 1\);' "$out/src/valid.c"
grep -Eq 'if\(__kryon_cond_[0-9]+\) \{' "$out/src/valid.c"
# A non-call condition is emitted unchanged.
grep -Fq 'if(value > 0 && first >= 0) {' "$out/src/valid.c"
grep -Fq 'for(int j = 0; j < 2; j++) {' "$out/src/valid.c"
grep -q 'value = value > 0 ? value : 1;' "$out/src/valid.c"
grep -q '    {' "$out/src/valid.c"
grep -Fq 'int scoped = 9;' "$out/src/valid.c"
grep -Fq 'value = scoped;' "$out/src/valid.c"
grep -Fq 'char label[32] = {0};' "$out/src/valid.c"
grep -Fq 'int c_count;' "$out/src/valid.c"
grep -Fq 'char c_label[16];' "$out/src/valid.c"
grep -Fq 'Color c_color;' "$out/src/valid.c"
grep -Fq 'void* c_ptr;' "$out/src/valid.c"
grep -Fq 'int explicit_count = 3;' "$out/src/valid.c"
grep -Fq 'int zero_count = {0};' "$out/src/valid.c"
grep -Fq 'int text = 1;' "$out/src/valid.c"
grep -Fq 'text = 2;' "$out/src/valid.c"
grep -Fq 'const char* path = {0};' "$out/src/valid.c"
grep -Fq 'if(path == NULL)' "$out/src/valid.c"
grep -Fq 'else if(value == 0) {' "$out/src/valid.c"
grep -Fq 'else {' "$out/src/valid.c"
grep -Fq 'switch(value) {' "$out/src/valid.c"
grep -Fq 'case 1:' "$out/src/valid.c"
grep -Fq 'default:' "$out/src/valid.c"
grep -Fq 'goto done;' "$out/src/valid.c"
grep -Fq 'done:' "$out/src/valid.c"
grep -Fq 'enum { LOCAL_ACTION_NONE, LOCAL_ACTION_RUN };' "$out/src/valid.c"
grep -Fq 'value = LOCAL_ACTION_RUN;' "$out/src/valid.c"
grep -Fq '__auto_type app = (void*)0;' "$out/src/valid.c"

cat > "$work/src/app_loop.kry" <<'EOF'
app "App Loop" {
    size 320 200
    fps 60
}

screen AppLoop() {
    Text("hello", 10, 10, UI_TEXT_14, GetThemeText())
}
EOF

"$k2c" --root "$work" -o "$out" "$work/src/app_loop.kry" >"$err" 2>&1
awk '
    /UIBeginTree\(1\);/ { begin_tree = NR }
    /UIEndTree\(\);/ { end_tree = NR }
    /UIReconcileTree\(\);/ { reconcile = NR }
    /UILayoutTree\(\);/ { layout = NR }
    /UIRouteInput\(\);/ { input = NR }
    /UIUpdateTree\(\);/ { update = NR }
    /DrawUITree\(\);/ { render = NR }
    /DrawUIOverlays\(\);/ { overlays = NR }
    /EndUIFocus\(\);/ { focus = NR }
    /EndDrawing\(\);/ { drawing = NR }
    END {
        if(begin_tree > 0 && end_tree > begin_tree &&
           reconcile > end_tree && layout > reconcile &&
           input > layout && update > input && render > update &&
           overlays > render && focus > overlays && drawing > focus)
            exit 0
        exit 1
    }
' "$out/src/app_loop.c" || {
    echo "generated app loop must run the widget tree pipeline before EndUIFocus" >&2
    exit 1
}

cat > "$work/src/colon_decl.kry" <<'EOF'
#module "colon_decl"
#import "thing.h"

shared_count :: int #global #export
local_counter :: int = 7 #global

helper :: (value: int) -> int {
    return value + 1
}

private_helper :: (value: int) -> int #private {
    return value - 1
}

c_entry :: (value: int) -> int #export {
    return helper(value)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/colon_decl.kry" >"$err" 2>&1
grep -Fq 'extern int shared_count;' "$out/src/colon_decl.h"
grep -Fq 'int shared_count;' "$out/src/colon_decl.c"
grep -Fq 'int local_counter = 7;' "$out/src/colon_decl.c"
grep -Fq 'int colon_decl_helper(int value);' "$out/src/colon_decl.h"
grep -Fq 'int c_entry(int value);' "$out/src/colon_decl.h"
if grep -Fq 'private_helper' "$out/src/colon_decl.h"; then
    echo "private function leaked into generated header" >&2
    exit 1
fi
grep -Eq 'static KRYON_PRIVATE_UNUSED int .*private_helper\(int value\)' "$out/src/colon_decl.c"
grep -Fq 'colon_decl_helper(value);' "$out/src/colon_decl.c"

cat > "$work/src/colon_import_host.kry" <<'EOF'
#import "thing.h"
panel :: #import "src/ui/panel"

draw_host :: (app: void*) {
    panel.draw(app)
}
EOF

mkdir -p "$work/src/ui"
cat > "$work/src/ui/panel.kry" <<'EOF'
#module "ui.panel"
#import "thing.h"

draw :: (app: void*) {
    unused app
}
EOF

"$k2c" --no-main --root "$work" -o "$out" \
    "$work/src/ui/panel.kry" "$work/src/colon_import_host.kry" >"$err" 2>&1
grep -q '#include "thing.h"' "$out/src/colon_import_host.h"
grep -q '#include "src/ui/panel.h"' "$out/src/colon_import_host.h"
grep -q 'ui_panel_draw(app);' "$out/src/colon_import_host.c"

cat > "$work/src/colon_types.kry" <<'EOF'
#import "thing.h"
#import <stddef.h>
#import "private_dep.h" #private

Pair :: struct {
    left: int
    right: int
}

Mode :: enum {
    MODE_NONE,
    MODE_RUN,
}

#enum {
    ANON_NONE,
    ANON_RUN,
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/colon_types.kry" >"$err" 2>&1
grep -q 'typedef struct Pair {' "$out/src/colon_types.h"
grep -q '#include <stddef.h>' "$out/src/colon_types.c"
grep -q '#include "private_dep.h"' "$out/src/colon_types.c"
if grep -q '#include "private_dep.h"' "$out/src/colon_types.h"; then
    echo "private import leaked into generated header" >&2
    exit 1
fi
grep -q 'int left;' "$out/src/colon_types.h"
grep -q '} Pair;' "$out/src/colon_types.h"
grep -q 'typedef enum Mode {' "$out/src/colon_types.h"
grep -q 'MODE_RUN,' "$out/src/colon_types.h"
grep -q '} Mode;' "$out/src/colon_types.h"
grep -q 'enum {' "$out/src/colon_types.h"
grep -q 'ANON_RUN,' "$out/src/colon_types.h"

cat > "$work/src/module_default.kry" <<'EOF'
#module "ui.panel"

panel_value :: () -> int {
    return 7
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/module_default.kry" >"$err" 2>&1
grep -q 'int ui_panel_panel_value(void);' "$out/src/module_default.h"
grep -q '#include "src/module_default.h"' "$out/src/module_default.c"

cat > "$work/src/externs.kry" <<'EOF'
GetExternalApp :: () -> struct external_app* #extern

WEB :: #defined(PLATFORM_WEB)

#if WEB {
    web_download_file :: (path: const char*, filename: const char*, mime: const char*) -> int #intrinsic "web"
}

externs_touch :: () -> int {
    return 1
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/externs.kry" >"$err" 2>&1
grep -Fq 'struct external_app* GetExternalApp(void);' "$out/src/externs.c"
grep -q 'static int' "$out/src/externs.c"
grep -Fq 'web_download_file(const char* path, const char* filename, const char* mime)' "$out/src/externs.c"

cat > "$work/src/interop_directives.kry" <<'EOF'
WIN32 :: #defined(_WIN32)

#if WIN32 {
    #pragma "GCC diagnostic push"
    #error "missing platform bridge"
    MessageBoxA :: (hwnd: void*,
                    text: const char*,
                    caption: const char*,
                    kind: unsigned int) -> int #extern #storage "__declspec(dllimport)" #abi "__stdcall"
}

trace_log :: (level: int, text: const char*, ...) -> void #extern
callback_attr :: (value: int) -> void #extern #attr "__attribute__((weak))"

interop_touch :: () -> int {
    return 1
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/interop_directives.kry" >"$err" 2>&1
grep -q '#pragma GCC diagnostic push' "$out/src/interop_directives.c"
grep -q '#error "missing platform bridge"' "$out/src/interop_directives.c"
grep -Fq '__declspec(dllimport) int __stdcall MessageBoxA(void* hwnd, const char* text, const char* caption, unsigned int kind);' "$out/src/interop_directives.c"
grep -Fq 'void trace_log(int level, const char* text, ...);' "$out/src/interop_directives.c"
grep -Fq 'void callback_attr(int value) __attribute__((weak));' "$out/src/interop_directives.c"

cat > "$work/src/source_api.kry" <<'EOF'
#module "storage.source"
#output "source_impl"
#import "source_api.h"

source_open :: () -> int #export {
    return 1
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/source_api.kry" >"$err" 2>&1
grep -q '#include "source_api.h"' "$out/src/source_impl.h"
grep -q '#include "src/source_impl.h"' "$out/src/source_impl.c"
if test -e "$out/src/source_api.h"; then
    echo "module implementation generated colliding source_api.h" >&2
    exit 1
fi

{
    printf '#import "thing.h"\n\n'
    for i in $(seq 1 40); do
        printf 'many_%02d :: () -> int {\n    return %d\n}\n\n' "$i" "$i"
    done
} > "$work/src/many_functions.kry"

"$k2c" --no-main --root "$work" -o "$out" "$work/src/many_functions.kry" >"$err" 2>&1
grep -q 'int many_40(void);' "$out/src/many_functions.h"

# --- a single function with a body well past the old 512-line cap must compile.
# Function bodies grow on demand; there is no fixed per-function statement cap,
# so large UI draw functions are not rejected.
{
    printf '#import "thing.h"\n\n'
    printf 'big :: () -> int {\n    total: int = 0\n'
    for i in $(seq 1 700); do
        printf '    total += %d\n' "$i"
    done
    printf '    return total\n}\n'
} > "$work/src/big_body.kry"

"$k2c" --no-main --root "$work" -o "$out" "$work/src/big_body.kry" >"$err" 2>&1
[ "$(grep -c 'total += ' "$out/src/big_body.c")" -ge 700 ] || {
    echo "large function body was truncated" >&2; exit 1; }
grep -q 'return total;' "$out/src/big_body.c"

cat > "$work/src/settings_ui.kry" <<'EOF'
#module "settings_ui"
#import "thing.h"

helper :: (value: int) -> int #private {
    return value + 1
}

toggle_row_height :: (label: const char*, w: int) -> int {
    return helper(w)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/settings_ui.kry" >"$err" 2>&1
grep -Eq '^static KRYON_PRIVATE_UNUSED int settings_ui_helper\(int value\)' "$out/src/settings_ui.c"
grep -q 'int settings_ui_toggle_row_height(const char\* label, int w);' "$out/src/settings_ui.h"
grep -q 'return settings_ui_helper(w);' "$out/src/settings_ui.c"

cat > "$work/src/function_pointer.kry" <<'EOF'
#module "fp"
#import "thing.h"

Handler :: struct {
    callback: int (*)(int)
}

callback :: (value: int) -> int #private {
    return value
}

start :: (value: int) -> int #private {
    return value
}

bind_callback :: () -> Handler {
    return (Handler){
        .callback = callback,
    }
}

check_shadow :: (start: int*) -> int {
    if start != nil {
        return 1
    }
    return 0
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/function_pointer.kry" >"$err" 2>&1
grep -Eq 'static KRYON_PRIVATE_UNUSED int fp_callback\(int value\);' "$out/src/function_pointer.c"
grep -q '\.callback = fp_callback,' "$out/src/function_pointer.c"
grep -q 'if(start != NULL)' "$out/src/function_pointer.c"

cat > "$work/src/settings_session.kry" <<'EOF'
#import "thing.h"
ui :: #import "settings_ui"

draw_session :: (label: const char*, w: int) -> int {
    return ui.toggle_row_height(label, w)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/settings_session.kry" >"$err" 2>&1
grep -q '#include "settings_ui.h"' "$out/src/settings_session.h"
grep -q 'return settings_ui_toggle_row_height(label, w);' "$out/src/settings_session.c"

cat > "$work/src/args_local.kry" <<'EOF'
#import "thing.h"
#import <stdlib.h>

WorkerArgs :: struct {
    value: int
}

worker :: (userdata: void*) -> void* #private {
    args := (WorkerArgs*)userdata
    if args != nil {
        args->value = 1
    }
    return userdata
}

allocate_args :: () -> int {
    args: WorkerArgs*
    args = malloc(sizeof(*args))
    if args == nil {
        return 0
    }
    free(args)
    return 1
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/args_local.kry" >"$err" 2>&1
grep -Eq 'static KRYON_PRIVATE_UNUSED void\* worker\(void\* userdata\);' "$out/src/args_local.c"
grep -Fq 'args = (WorkerArgs*)userdata;' "$out/src/args_local.c"
grep -Fq 'WorkerArgs* args = {0};' "$out/src/args_local.c"
grep -Fq 'args = malloc(sizeof(*args));' "$out/src/args_local.c"

cat > "$work/src/settings_direct.kry" <<'EOF'
#import "thing.h"
#import "settings_ui"

draw_direct :: (label: const char*, w: int) -> int {
    return settings_ui.toggle_row_height(label, w)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/settings_direct.kry" >"$err" 2>&1
grep -q 'return settings_ui_toggle_row_height(label, w);' "$out/src/settings_direct.c"

mkdir -p "$work/src/ui"
cat > "$work/src/ui/panel.kry" <<'EOF'
#module "ui.panel"
#import "thing.h"

draw :: (app: void*) {
    unused app
}

panel_c_entry :: (app: void*) #export {
    unused app
}
EOF

cat > "$work/src/panel_host.kry" <<'EOF'
#import "thing.h"
panel :: #import "src/ui/panel"

draw_panel_host :: (app: void*) {
    panel.draw(app)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" \
    "$work/src/ui/panel.kry" "$work/src/panel_host.kry" >"$err" 2>&1
grep -q '#include "src/ui/panel.h"' "$out/kryon_project.h"
grep -q '#include "src/panel_host.h"' "$out/kryon_project.h"
grep -q '#include "src/ui/panel.h"' "$out/src/panel_host.h"
grep -q 'void ui_panel_draw(void\* app);' "$out/src/ui/panel.h"
grep -q 'void panel_c_entry(void\* app);' "$out/src/ui/panel.h"
if grep -q 'ui_panel_panel_c_entry' "$out/src/ui/panel.h"; then
    echo "export function was module-prefixed" >&2
    exit 1
fi
grep -q 'ui_panel_draw(app);' "$out/src/panel_host.c"

mkdir -p "$work/src/settings"
cat > "$work/src/settings/types.kry" <<'EOF'
#module "settings.types"
#import "thing.h"

SettingsThemeState :: struct {
    value: int
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/settings/types.kry" >"$err" 2>&1
grep -q '#include "thing.h"' "$out/src/settings/types.h"
grep -q 'typedef struct SettingsThemeState {' "$out/src/settings/types.h"
grep -q 'int value;' "$out/src/settings/types.h"

"$k2c" --no-main --root "$work" -o "$out" \
    "$work/src/settings/types.kry" "$work/src/panel_host.kry" >"$err" 2>&1
grep -q '#include "src/settings/types.h"' "$out/kryon_project.h"

cat > "$work/src/panel_direct_host.kry" <<'EOF'
#import "thing.h"
#import "src/ui/panel"

draw_panel_direct_host :: (app: void*) {
    ui_panel.draw(app)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" \
    "$work/src/ui/panel.kry" "$work/src/panel_direct_host.kry" >"$err" 2>&1
grep -q '#include "src/ui/panel.h"' "$out/src/panel_direct_host.h"
grep -q 'ui_panel_draw(app);' "$out/src/panel_direct_host.c"

cat > "$work/src/bad_import.kry" <<'EOF'
#import "thing.h"
import "src/ui/panel.kry"

old_import_host :: (app: void*) {
    ui_panel.draw(app)
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" \
    "$work/src/bad_import.kry" >"$err" 2>&1; then
    echo "removed import unexpectedly succeeded" >&2
    exit 1
fi
grep -q 'unknown top-level statement: import "src/ui/panel.kry"' "$err"

cat > "$work/src/preview.kry" <<'EOF'
preview stage_preview(viewport: Rectangle) {
    Background(BLACK)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/preview.kry" >"$err" 2>&1

cat > "$work/src/state_multiline.kry" <<'EOF'
#import "stddef.h"

state {
    labels: [2] const char* = {
        "one",
        "two",
    }
}

state_label :: (index: int) -> const char* {
    return labels[index]
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/state_multiline.kry" >"$err" 2>&1
grep -q 'static const char\* labels\[2\] = {' "$out/src/state_multiline.c"
grep -q '"one",' "$out/src/state_multiline.c"
grep -q '"two",' "$out/src/state_multiline.c"

cat > "$work/src/native_c_features.kry" <<'EOF'
ANDROID :: #defined(ANDROID_BUILD)
FEATURE_ENABLED :: ANDROID
FEATURE_VALUE :: #define 7

#import "stddef.h" #private
platform_ping :: (value: int,
                        tag: const char*) -> int #extern #if FEATURE_ENABLED
platform_context :: () -> void* #extern #if FEATURE_ENABLED

static records: [2] const int = {
    helper_value(),
    FEATURE_VALUE,
}

helper_value :: () -> int #private {
    return nil == nil ? 1 : 0
}

native_feature_value :: () -> int {
    #if FEATURE_ENABLED {
        return platform_ping(records[0], "native")
    } #else {
        return records[1]
    }
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/native_c_features.kry" >"$err" 2>&1
grep -q '#include "stddef.h"' "$out/src/native_c_features.c"
grep -q '#define FEATURE_VALUE 7' "$out/src/native_c_features.c"
grep -q '#if ((defined(ANDROID_BUILD)))' "$out/src/native_c_features.c"
grep -Fq 'int platform_ping(int value, const char* tag);' "$out/src/native_c_features.c"
grep -Fq 'void* platform_context(void);' "$out/src/native_c_features.c"
grep -Eq 'static KRYON_PRIVATE_UNUSED int helper_value\(void\);' "$out/src/native_c_features.c"
grep -q 'static const int records\[2\] = {' "$out/src/native_c_features.c"
grep -q 'return NULL == NULL ? 1 : 0;' "$out/src/native_c_features.c"
grep -q '#else' "$out/src/native_c_features.c"
grep -q '#endif' "$out/src/native_c_features.c"

cat > "$work/src/top_level_macros.kry" <<'EOF'
WEB :: #defined(PLATFORM_WEB)
DESKTOP :: !WEB

#module "platform.test"
#import "thing.h"

#if WEB {
    #import <emscripten.h>
    PLATFORM_VALUE :: #define 7
    guarded_counter :: int = 3 #global #export
    web_ping :: (value: int,
                       tag: const char*) -> int #extern
    web_download_file :: (path: const char*,
                                              filename: const char*,
                                              mime: const char*) -> int #intrinsic "web"
    web_context_click_in_bounds :: (x0: int, y0: int, x1: int, y1: int) -> int #intrinsic "web"
    static web_ready: int = 1

    WebCallback :: int (*)(int) #type

    WebState :: struct {
        value: int
    }

    platform_value :: () -> int {
        return web_ping(web_ready, "web")
    }

    platform_download :: (path: const char*) -> int {
        return web_download_file(path, "file.bin", "application/octet-stream")
    }

    platform_context_click :: () -> int {
        return web_context_click_in_bounds(1, 2, 3, 4)
    }
} #else_if DESKTOP {
    static desktop_ready: int = 2

    desktop_value :: () -> int #private {
        return desktop_ready
    }

    platform_value :: () -> int {
        return desktop_value()
    }
} #else {
    platform_value :: () -> int {
        return 0
    }
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/top_level_macros.kry" >"$err" 2>&1
grep -q '#if (defined(PLATFORM_WEB))' "$out/src/top_level_macros.h"
grep -q 'typedef int (\*WebCallback)(int);' "$out/src/top_level_macros.h"
grep -q 'typedef struct WebState {' "$out/src/top_level_macros.h"
grep -q 'int platform_test_platform_value(void);' "$out/src/top_level_macros.h"
grep -q 'extern int guarded_counter;' "$out/src/top_level_macros.h"
grep -q '#include <emscripten.h>' "$out/src/top_level_macros.c"
grep -q '#define PLATFORM_VALUE 7' "$out/src/top_level_macros.c"
grep -q 'int guarded_counter = 3;' "$out/src/top_level_macros.c"
grep -Fq 'int web_ping(int value, const char* tag);' "$out/src/top_level_macros.c"
grep -q 'static int' "$out/src/top_level_macros.c"
grep -Fq 'web_download_file(const char* path, const char* filename, const char* mime)' "$out/src/top_level_macros.c"
grep -Fq 'web_context_click_in_bounds(int x0, int y0, int x1, int y1)' "$out/src/top_level_macros.c"
grep -Fq 'return web_download_file(path, "file.bin", "application/octet-stream");' "$out/src/top_level_macros.c"
grep -Fq 'return web_context_click_in_bounds(1, 2, 3, 4);' "$out/src/top_level_macros.c"
grep -Fq 'EM_ASM_INT' "$out/src/top_level_macros.c"
grep -Fq 'Module.__kryonContextClick' "$out/src/top_level_macros.c"
! grep -Fq 'Module.__inbeContextClick' "$out/src/top_level_macros.c"
grep -q 'static int web_ready = 1;' "$out/src/top_level_macros.c"
grep -Eq 'static KRYON_PRIVATE_UNUSED int platform_test_desktop_value\(void\);' "$out/src/top_level_macros.c"
grep -q 'static int desktop_ready = 2;' "$out/src/top_level_macros.c"
grep -q '#if !((defined(PLATFORM_WEB))) && ((!(defined(PLATFORM_WEB))))' "$out/src/top_level_macros.c"

cat > "$work/src/native_structs.kry" <<'EOF'
#import "stddef.h"

#enum {
    PUBLIC_FIRST = 1
    PUBLIC_SECOND
}

PublicMode :: enum {
    PUBLIC_MODE_ONE = 1
    PUBLIC_MODE_TWO
}

Callback :: int (*)(int) #type
LocalSize :: unsigned long #type #private

#enum #private {
    LOCAL_FIRST = 1
    LOCAL_SECOND
}

LocalMode :: enum #private {
    LOCAL_MODE_ONE = 1
    LOCAL_MODE_TWO
}

PublicPair :: struct {
    name: const char*
    values: [2] int
    matrix: [2][3] int
    mode: PublicMode
}

LocalCtx :: struct #private {
    count: int
    pair: PublicPair*
}

native_struct_count :: (pair: PublicPair*) -> int {
    ctx: LocalCtx = {0}
    ctx.count = pair != nil ? PUBLIC_SECOND + LOCAL_SECOND : 0
    ctx.pair = pair
    return ctx.count
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/native_structs.kry" >"$err" 2>&1
grep -q 'enum {' "$out/src/native_structs.h"
grep -q 'PUBLIC_FIRST = 1,' "$out/src/native_structs.h"
grep -q 'PUBLIC_SECOND,' "$out/src/native_structs.h"
grep -q 'typedef enum PublicMode {' "$out/src/native_structs.h"
grep -q 'PUBLIC_MODE_TWO,' "$out/src/native_structs.h"
grep -q '} PublicMode;' "$out/src/native_structs.h"
grep -Fq 'typedef int (*Callback)(int);' "$out/src/native_structs.h"
grep -q 'typedef unsigned long LocalSize;' "$out/src/native_structs.c"
grep -q 'LOCAL_FIRST = 1,' "$out/src/native_structs.c"
grep -q 'LOCAL_SECOND,' "$out/src/native_structs.c"
grep -q 'typedef enum LocalMode {' "$out/src/native_structs.c"
grep -q 'LOCAL_MODE_TWO,' "$out/src/native_structs.c"
grep -q '} LocalMode;' "$out/src/native_structs.c"
grep -q 'typedef struct PublicPair {' "$out/src/native_structs.h"
grep -q 'const char\* name;' "$out/src/native_structs.h"
grep -q 'int values\[2\];' "$out/src/native_structs.h"
grep -q 'int matrix\[2\]\[3\];' "$out/src/native_structs.h"
grep -q 'PublicMode mode;' "$out/src/native_structs.h"
grep -q '} PublicPair;' "$out/src/native_structs.h"
grep -q 'typedef struct LocalCtx {' "$out/src/native_structs.c"
grep -q 'PublicPair\* pair;' "$out/src/native_structs.c"
grep -q '} LocalCtx;' "$out/src/native_structs.c"

cat > "$work/src/multiline_fn_decl.kry" <<'EOF'
#import "stddef.h"

multiline_sum :: (first: int,
                     second: int,
                     third: int) -> int {
    return first + second + third
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/multiline_fn_decl.kry" >"$err" 2>&1
grep -q 'int multiline_sum(int first, int second, int third);' "$out/src/multiline_fn_decl.h"
grep -q 'multiline_sum(int first, int second, int third)' "$out/src/multiline_fn_decl.c"

cat > "$work/src/implicit_call.kry" <<'EOF'
screen calls {
    InitializeThing()
    DrawThing(
        1,
        2)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/implicit_call.kry" >"$err" 2>&1
grep -q 'InitializeThing();' "$out/src/implicit_call.c"
grep -q 'DrawThing( 1, 2);' "$out/src/implicit_call.c"

cat > "$work/src/expression_statement.kry" <<'EOF'
#import "stddef.h"

expression_statement :: (env: void*, object: void*, method: void*) {
    (*env)->CallVoidMethod(env, object, method)
    (*env)->DeleteLocalRef(
        env,
        object)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/expression_statement.kry" >"$err" 2>&1
grep -q '(\*env)->CallVoidMethod(env, object, method);' "$out/src/expression_statement.c"
grep -q '(\*env)->DeleteLocalRef( env, object);' "$out/src/expression_statement.c"

cat > "$work/src/bad_multi_decl.kry" <<'EOF'
screen bad {
    a, b := 1, 2, 3
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_multi_decl.kry" >"$err" 2>&1; then
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

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_multi_assignment.kry" >"$err" 2>&1; then
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

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_include.kry" >"$err" 2>&1; then
    echo "removed include was accepted" >&2
    exit 1
fi
grep -q 'unknown top-level statement: include "thing.h"' "$err"
grep -Eq 'bad_include\.kry:1:1: error:' "$err"

cat > "$work/src/bad_var.kry" <<'EOF'
screen bad {
    var label: [32] char = {0}
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_var.kry" >"$err" 2>&1; then
    echo "removed var declaration was accepted" >&2
    exit 1
fi
grep -q "'var' syntax was removed" "$err"
grep -Eq 'bad_var\.kry:2:1: error:' "$err"

cat > "$work/src/bad_do.kry" <<'EOF'
screen bad {
    do InitializeThing()
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_do.kry" >"$err" 2>&1; then
    echo "removed do statement was accepted" >&2
    exit 1
fi
grep -q "'do' syntax was removed" "$err"
grep -Eq 'bad_do\.kry:2:1: error:' "$err"

# Removed widget verbs: draw, native, set, widget must now error with a
# migration hint, mirroring the let/var/do removals above.
for verb in draw native set widget; do
    cat > "$work/src/bad_${verb}.kry" <<EOF
screen bad {
    ${verb} InitializeThing()
}
EOF
    if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_${verb}.kry" >"$err" 2>&1; then
        echo "removed ${verb} statement was accepted" >&2
        exit 1
    fi
    grep -q "was removed" "$err"
    grep -Eq "bad_${verb}\.kry:2:1: error:" "$err"
done

# Removed sugar keywords (second pass): each must error with a migration hint.
# Each has distinct syntax, so they get bespoke test cases.
check_removed() {
    # $1 = short name, $2 = .kry body, $3 = expected error substring
    cat > "$work/src/bad_$1.kry" <<EOF
screen bad {
$2
}
EOF
    if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_$1.kry" >"$err" 2>&1; then
        echo "removed $1 statement was accepted" >&2
        exit 1
    fi
    grep -q "$3" "$err"
    grep -Eq "bad_$1\.kry:2:1: error:" "$err"
}
check_removed advance        '    advance x by 1'                       "advance"
check_removed clamp_min      '    clamp_min x 1'                        "clamp_min"
check_removed clamp_max      '    clamp_max x 1'                        "clamp_max"
check_removed c_rect         '    c_rect r = 0,0,1,1'                   "c_rect"
check_removed texture        '    texture t = 0'                        "texture"
check_removed set_theme      '    set_theme THEME_SKY light'            "set_theme"
check_removed button         '    button "x" x: 0 y: 0 {'               "button"
check_removed event          '    event foo() {'                        "event"
check_removed icon_button    '    icon_button 0 {'                      "icon_button"
check_removed on_key         '    on key KEY_A {'                       "on key"
check_removed on_key_down    '    on key_down KEY_A {'                  "on key_down"

cat > "$work/src/bad_goto.kry" <<'EOF'
screen bad {
    goto
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_goto.kry" >"$err" 2>&1; then
    echo "bad goto statement was accepted" >&2
    exit 1
fi
grep -q 'expected goto label name' "$err"
grep -Eq 'bad_goto\.kry:2:1: error:' "$err"

# --- error recovery: a file with several distinct errors reports all of them,
# not just the first. Each malformed statement records a diagnostic and parsing
# continues, so the IDE's problems pane can show every error at once.
cat > "$work/src/multi_error.kry" <<'EOF'
screen multi {
    a, b := 1, 2, 3
    var x = 5
    let y = 6
    good := 10
    c, d = 1, 2, 3
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/multi_error.kry" >"$err" 2>&1; then
    echo "multi-error file was accepted" >&2
    exit 1
fi
# All four distinct errors must appear, in source order.
grep -q 'multi_error\.kry:2:1: error: inferred declaration count mismatch' "$err"
grep -q "multi_error\.kry:3:1: error: 'var' syntax was removed" "$err"
grep -q "multi_error\.kry:4:1: error: 'let' syntax was removed" "$err"
grep -q 'multi_error\.kry:6:1: error: assignment count mismatch' "$err"
# Exactly four error lines (no spurious duplicates from recovery).
[ "$(grep -c ': error:' "$err")" -eq 4 ] || {
    echo "multi-error recovery produced wrong error count:" >&2
    grep ': error:' "$err" >&2
    exit 1
}

cat > "$work/src/unbalanced.kry" <<'EOF'
screen bad {
    InitializeThing()
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/unbalanced.kry" >"$err" 2>&1; then
    echo "unbalanced braces were accepted" >&2
    exit 1
fi
grep -q 'unbalanced braces' "$err"
grep -Eq 'unbalanced\.kry:[0-9]+:1: error:' "$err"

# --- defer: deferred statements run at scope exit, before return/break/continue
cat > "$work/src/defer.kry" <<'EOF'
#import "thing.h"

work :: (x: int) -> int {
    f := x
    defer Release(f)
    if x < 0 {
        return 0
    }
    if x > 10 {
        return x
    }
    for int i = 0; i < 3; i++ {
        defer Log(i)
        if i == 1 {
            break
        }
    }
    return x + f
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/defer.kry" >"$err" 2>&1
# Function-body defer fires before each return and before the final return.
# The defer and return land on consecutive lines; use grep -A1 (no -q, since
# -q suppresses the context lines the second grep needs to see).
grep -Fq 'Release(f);' "$out/src/defer.c"
grep -E -A1 'Release\(f\);$' "$out/src/defer.c" | grep -Eq 'return 0;'
grep -Fq 'return x;' "$out/src/defer.c"
grep -Fq 'return x + f;' "$out/src/defer.c"
# Loop-body defer fires before break (registered before the break in source).
grep -E -A1 'Log\(i\);$' "$out/src/defer.c" | grep -Eq 'break;'
# Loop-body defer also fires at the end of each non-break iteration.
grep -E -B1 'Log\(i\);$' "$out/src/defer.c" | grep -Eq '\}'
# The original `defer` declaration must not survive into the output.
if grep -q 'defer ' "$out/src/defer.c"; then
    echo "defer declaration leaked into generated C" >&2
    exit 1
fi

# --- defer: multiple defers in one scope run in reverse (LIFO) order
cat > "$work/src/defer_order.kry" <<'EOF'
#import "thing.h"

multi :: () -> int {
    defer First()
    defer Second()
    defer Third()
    return 0
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/defer_order.kry" >"$err" 2>&1
# Defers run in reverse (LIFO) order: Third (registered last) runs first,
# First (registered first) runs last. Compare line numbers in the output.
l_third=$(grep -n 'Third();' "$out/src/defer_order.c" | cut -d: -f1)
l_second=$(grep -n 'Second();' "$out/src/defer_order.c" | cut -d: -f1)
l_first=$(grep -n 'First();' "$out/src/defer_order.c" | cut -d: -f1)
[ -n "$l_third" ] && [ -n "$l_second" ] && [ -n "$l_first" ] || {
    echo "defer order: missing one of Third/Second/First" >&2; exit 1; }
[ "$l_third" -lt "$l_second" ] && [ "$l_second" -lt "$l_first" ] || {
    echo "defer order: expected Third < Second < First by line" >&2; exit 1; }

# --- defer: break/continue unwind only the enclosing loop body
cat > "$work/src/defer_break.kry" <<'EOF'
#import "thing.h"

loop_fn :: (n: int) -> int {
    for int i = 0; i < n; i++ {
        defer Cleanup(i)
        if i == 2 {
            break
        }
    }
    return n
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/defer_break.kry" >"$err" 2>&1
# Cleanup fires before the break (defer declared before the break in source).
grep -E -A1 'Cleanup\(i\);$' "$out/src/defer_break.c" | grep -Eq 'break;'
# Cleanup also fires at the end of each iteration that reaches the loop close.
grep -E -B1 'Cleanup\(i\);$' "$out/src/defer_break.c" | grep -Eq '\}'

# --- defer: malformed (missing statement) is a clean error
cat > "$work/src/bad_defer.kry" <<'EOF'
screen bad {
    defer
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_defer.kry" >"$err" 2>&1; then
    echo "empty defer was accepted" >&2
    exit 1
fi
grep -q 'expected defer statement' "$err"
grep -Eq 'bad_defer\.kry:2:1: error:' "$err"

# --- defer inside switch cases: each case's defers fire on its own path only
# (regression: apply_defers previously treated the switch as one scope, leaking
# case defers across cases — a defer in case 1 ran in default too)
cat > "$work/src/defer_switch.kry" <<'EOF'
#import "thing.h"
sw :: (s: int) -> int {
    switch s {
    case 1:
        defer Cleanup(s)
        break
    case 2:
        defer Other(s)
        return s
    default:
        defer Default(s)
    }
    return 0
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/defer_switch.kry" >"$err" 2>&1
# Cleanup fires before break (case 1 path).
grep -E -A1 'Cleanup\(s\);$' "$out/src/defer_switch.c" | grep -Eq 'break;'
# Other fires before return (case 2 path).
grep -E -A1 'Other\(s\);$' "$out/src/defer_switch.c" | grep -Eq 'return s;'
# Default fires at the switch close (default fall-through path).
grep -E -B1 'Default\(s\);$' "$out/src/defer_switch.c" | grep -Eq 'default:'
# No defer leaks across cases: Cleanup must NOT appear after case 2 or default,
# Other must NOT appear after default. Count occurrences — each should fire
# exactly once.
[ "$(grep -c 'Cleanup(s);' "$out/src/defer_switch.c")" = 1 ] || {
    echo "defer_switch: Cleanup leaked across cases" >&2; exit 1; }
[ "$(grep -c 'Other(s);' "$out/src/defer_switch.c")" = 1 ] || {
    echo "defer_switch: Other leaked across cases" >&2; exit 1; }
[ "$(grep -c 'Default(s);' "$out/src/defer_switch.c")" = 1 ] || {
    echo "defer_switch: Default fired more than once" >&2; exit 1; }

# --- a call used as an if condition registers its source line for inspection
# (regression: UIButton-in-if previously had no source, so click-to-source
# on buttons did nothing)
cat > "$work/src/if_call.kry" <<'EOF'
#import "thing.h"

screen IfCall {
    if Button((ButtonProps){
        .bounds = {0, 0, 100, 40},
        .label = "Click",
        .style = UI_BUTTON_STYLE_PRIMARY,
        .id = 10,
    }) {
        DoThing()
    }
    if value > 0 {
        DoThing()
    }
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/if_call.kry" >"$err" 2>&1
# The call condition is wrapped: Push, temp assign, Pop, then test the temp.
grep -Eq 'PushUIInspectSource\([^)]*if_call\.kry", 4\);' "$out/src/if_call.c"
grep -Eq '__auto_type __kryon_cond_[0-9]+ = Button\(' "$out/src/if_call.c"
grep -Eq 'PopUIInspectSource\(\);' "$out/src/if_call.c"
grep -Eq 'if\(__kryon_cond_[0-9]+\) \{' "$out/src/if_call.c"
# A non-call condition is emitted unchanged (no temp, no wrapping).
grep -Fq 'if(value > 0) {' "$out/src/if_call.c"

# --- tab bar is available as a Kryon widget API and updates selection pointer
cat > "$work/src/tabbar_widget.kry" <<'EOF'
#import "kryon.h"

state {
    selected: int = 0
    tabs: [3] const UITab = {{"Files"},{"Search"},{"Output"}}
}

screen TabbarWidget {
    if TabBar((TabBarProps){{0,0,360,0},tabs,3,selected,0,0,0,NULL,1,NULL}) >= 0 {
        DoThing()
    }
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/tabbar_widget.kry" >"$err" 2>&1
grep -Fq 'if(TabBar((TabBarProps){{0,0,360,0},tabs,3,selected,0,0,0,NULL,1,NULL}) >= 0) {' "$out/src/tabbar_widget.c"

# --- compound assignment operators not previously covered (-=, *=, /=, --)
cat > "$work/src/compound.kry" <<'EOF'
#import "thing.h"
screen Compound {
    v := 10
    v -= 2
    v *= 3
    v /= 4
    v--
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/compound.kry" >"$err" 2>&1
grep -Fq 'v -= 2;' "$out/src/compound.c"
grep -Fq 'v *= 3;' "$out/src/compound.c"
grep -Fq 'v /= 4;' "$out/src/compound.c"
grep -Fq 'v--;' "$out/src/compound.c"

# --- defer inside a while loop and before continue (loop-body defer)
cat > "$work/src/defer_while.kry" <<'EOF'
#import "thing.h"
loop :: (n: int) -> int {
    i := 0
    while i < n {
        defer Tick(i)
        i++
        if i == 2 {
            continue
        }
    }
    return i
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/defer_while.kry" >"$err" 2>&1
# defer declared before the continue fires before the continue.
grep -E -A1 'Tick\(i\);$' "$out/src/defer_while.c" | grep -Eq 'continue;'
# defer also fires at the end of each iteration that reaches the loop close.
grep -E -B1 'Tick\(i\);$' "$out/src/defer_while.c" | grep -Eq '\}'

# --- defer inside an anonymous block scope (block-close defer)
cat > "$work/src/defer_block.kry" <<'EOF'
#import "thing.h"
block_fn :: () -> int {
    s := 1
    {
        defer Scoped()
        s = 2
    }
    return s
}
EOF

"$k2c" --no-main --root "$work" -o "$out" "$work/src/defer_block.kry" >"$err" 2>&1
# Scoped fires at the anonymous block's closing brace (after s = 2).
grep -E -B1 'Scoped\(\);$' "$out/src/defer_block.c" | grep -Eq 's = 2;'

# --- main generation: omitting --no-main emits an int main entry point
cat > "$work/src/withmain.kry" <<'EOF'
#import "thing.h"
app "Test App" {
    size 800 600
    fps 60
}
screen Main {
    Text("hi", 0, 0, 16, GetThemeText())
}
EOF

"$k2c" --root "$work" -o "$out" "$work/src/withmain.kry" >"$err" 2>&1
grep -Eq '^int$' "$out/src/withmain.c"
grep -Eq '^main\(void\)$' "$out/src/withmain.c"
grep -q 'InitWindow(800, 600, "Test App");' "$out/src/withmain.c"
grep -q 'SetTargetFPS(60);' "$out/src/withmain.c"
grep -q 'while(!WindowShouldClose())' "$out/src/withmain.c"
grep -q 'Main_kry_draw();' "$out/src/withmain.c"
# --no-main must NOT emit main.
"$k2c" --no-main --root "$work" -o "$out" "$work/src/withmain.kry" >"$err" 2>&1
if grep -q 'int main' "$out/src/withmain.c"; then
    echo "--no-main still emitted main" >&2
    exit 1
fi

# --- hook-driven main: when the app{} block names init/frame/shutdown hooks,
# k2c emits a main() that calls init() once, frame() each iteration, and
# shutdown() at exit, WITHOUT bracketing drawing — the program owns its loop.
# This is how a standalone .kry app (like the IDE) takes full control of the
# frame, including nested render-texture passes and inspection overlays.
cat > "$work/src/hookmain.kry" <<'EOF'
#import "thing.h"

app "Hook App" {
    size 640 480
    fps 60
    init on_init
    frame on_frame
    shutdown on_shutdown
}

on_init :: () {
    DrawThing(0)
}

on_frame :: () {
    DrawThing(1)
}

on_shutdown :: () {
    DrawThing(2)
}
EOF

"$k2c" --root "$work" -o "$out" "$work/src/hookmain.kry" >"$err" 2>&1
# init runs once before the loop; frame inside it; shutdown after.
grep -q 'on_init();' "$out/src/hookmain.c"
grep -q 'while(!WindowShouldClose())' "$out/src/hookmain.c"
grep -q 'on_frame();' "$out/src/hookmain.c"
grep -q 'on_shutdown();' "$out/src/hookmain.c"
# The program owns drawing: k2c must NOT emit BeginDrawing/EndDrawing itself.
if grep -q 'BeginDrawing' "$out/src/hookmain.c"; then
    echo "hook-driven main emitted BeginDrawing (should be program-owned)" >&2
    exit 1
fi
# A module-qualified hook resolves to its module-prefixed C name.
cat > "$work/src/hookmod.kry" <<'EOF'
#module "hookmod"
#import "thing.h"

app "Hook Mod App" {
    size 640 480
    frame step
}

step :: () {
    DrawThing(1)
}
EOF

"$k2c" --root "$work" -o "$out" "$work/src/hookmod.kry" >"$err" 2>&1
grep -q 'hookmod_step();' "$out/src/hookmod.c"
if grep -q 'BeginDrawing' "$out/src/hookmod.c"; then
    echo "module hook-driven main emitted BeginDrawing" >&2
    exit 1
fi

# --- removed keyword: 'let' produces a removal error
cat > "$work/src/bad_let.kry" <<'EOF'
screen bad {
    let x = 1
}
EOF

if "$k2c" --no-main --root "$work" -o "$out" "$work/src/bad_let.kry" >"$err" 2>&1; then
    echo "let was accepted" >&2
    exit 1
fi
grep -q "'let' syntax was removed" "$err"
grep -Eq 'bad_let\.kry:2:1: error:' "$err"

# --- --dump-ast reconstructs an AST with no unclassified (UNKNOWN) nodes
cat > "$work/src/ast.kry" <<'EOF'
#import "thing.h"
ast_fn :: (n: int) -> int {
    v := 0
    count: int = 5
    name: const char* = "hi"
    defer Cleanup(v)
    if n > 0 {
        return n
    }
    for int i = 0; i < n; i++ {
        v += i
        if i == 2 {
            break
        }
    }
    while v < 3 {
        v++
    }
    switch v {
    case 1:
        v = 2
    default:
        v = 3
    }
    unused v
    return v
}
EOF

"$k2c" --dump-ast --no-main --root "$work" -o "$out" "$work/src/ast.kry" >"$err" 2>&1
# The dump header names the function.
grep -q 'function ast_fn' "$err"
# Every statement kind is classified — no UNKNOWN nodes (full capture).
if grep -q ' UNKNOWN ' "$err"; then
    echo "dump-ast produced unclassified nodes" >&2
    exit 1
fi
# The expected control-flow kinds are present.
grep -Eq '^IF ' "$err"
grep -Eq '^FOR ' "$err"
grep -Eq '^WHILE ' "$err"
grep -Eq '^SWITCH ' "$err"
grep -Eq '^DEFER ' "$err"
grep -Eq '^RETURN ' "$err"
# Typed declarations classify as DECL (not ASSIGN): "int count = 5;" and
# "const char* name = "hi";" are declarations, while "v += i" is an assignment.
grep -Eq '^DECL .*int count = 5;' "$err"
grep -Eq '^DECL .*const char\* name =' "$err"
grep -Eq '^  ASSIGN .*v \+= i;' "$err"
# --dump-ast must NOT write the generated .c/.h (it parses but skips emit).
if [ -f "$out/src/ast.c" ]; then
    echo "dump-ast wrote generated files" >&2
    exit 1
fi

# --- qualified type references across modules: alias.Type must resolve to the
# bare C type name in every position (params, return, struct field, local, and
# global), while qualified *calls* (alias.fn(...)) keep rewriting to
# module_fn(...). Structs/enums are emitted with their bare declared name in C,
# so the alias prefix must be dropped, not module-prefixed.
mkdir -p "$work/src/mod_types"
cat > "$work/src/mod_types/state.kry" <<'EOF'
#module "mod_types.state"
#import "thing.h"

Counter :: struct {
    value: int
}

counter_tick :: (c: Counter*) -> int {
    c->value += 1
    return c->value
}
EOF

cat > "$work/src/mod_types/host.kry" <<'EOF'
#import "thing.h"
state :: #import "src/mod_types/state"

counter :: state.Counter = {0} #global

panel :: (c: state.Counter*) -> state.Counter {
    local: state.Counter = {0}
    local.value = state.counter_tick(c)
    return local
}

Pair :: struct {
    inner: state.Counter
    ptr: state.Counter*
}

take :: (c: state.Counter*) -> int {
    return state.counter_tick(c)
}
EOF

"$k2c" --no-main --root "$work" -o "$out" \
    "$work/src/mod_types/state.kry" "$work/src/mod_types/host.kry" >"$err" 2>&1
# Qualified call rewrites to the module-prefixed C function name.
grep -q 'mod_types_state_counter_tick(c);' "$out/src/mod_types/host.c"
# Qualified types resolve to the bare C type name in every position:
# parameter, return type, struct field, local declaration, and global.
# (The header carries the single-line prototype; the .c splits the return
# type onto its own line, so assert the parameter line there.)
grep -q 'Counter panel(Counter\* c);' "$out/src/mod_types/host.h"
grep -q 'panel(Counter\* c)' "$out/src/mod_types/host.c"
grep -q 'Counter local = {0};' "$out/src/mod_types/host.c"
grep -q 'Counter inner;' "$out/src/mod_types/host.h"
grep -q 'Counter\* ptr;' "$out/src/mod_types/host.h"
grep -q 'int take(Counter\* c);' "$out/src/mod_types/host.h"
grep -q 'Counter counter = {0};' "$out/src/mod_types/host.c"
# The header includes the imported module's generated header.
grep -q '#include "src/mod_types/state.h"' "$out/src/mod_types/host.h"
# No qualified alias text survives into the generated C.
if grep -q 'state.Counter' "$out/src/mod_types/host.c" \
    || grep -q 'state.Counter' "$out/src/mod_types/host.h"; then
    echo "qualified type alias leaked into generated C" >&2
    exit 1
fi
