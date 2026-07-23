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
#import "thing.h"

shared_count :: int #global #export
local_counter :: int = 7 #global

screen valid {
    first, second := 0
    first, second = 1
    value := first + second
    unused second
    InitializeThing()
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/valid.kry" >"$err" 2>&1
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
grep -q 'if(CheckThing( value, 1)) {' "$out/src/valid.c"
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/colon_decl.kry" >"$err" 2>&1
grep -Fq 'extern int shared_count;' "$out/src/colon_decl.h"
grep -Fq 'int shared_count;' "$out/src/colon_decl.c"
grep -Fq 'int local_counter = 7;' "$out/src/colon_decl.c"
grep -Fq 'int colon_decl_helper(int value);' "$out/src/colon_decl.h"
grep -Fq 'int c_entry(int value);' "$out/src/colon_decl.h"
if grep -Fq 'private_helper' "$out/src/colon_decl.h"; then
    echo "private function leaked into generated header" >&2
    exit 1
fi
grep -Fq 'static int' "$out/src/colon_decl.c"
grep -Fq 'private_helper(int value)' "$out/src/colon_decl.c"
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
    native (void)app
}
EOF

"$kc" --no-main --root "$work" -o "$out" \
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/colon_types.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/module_default.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/externs.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/interop_directives.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/source_api.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/many_functions.kry" >"$err" 2>&1
grep -q 'int many_40(void);' "$out/src/many_functions.h"

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

"$kc" --no-main --root "$work" -o "$out" "$work/src/settings_ui.kry" >"$err" 2>&1
grep -q '^static int settings_ui_helper(int value)' "$out/src/settings_ui.c"
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/function_pointer.kry" >"$err" 2>&1
grep -q 'static int fp_callback(int value);' "$out/src/function_pointer.c"
grep -q '\.callback = fp_callback,' "$out/src/function_pointer.c"
grep -q 'if(start != NULL)' "$out/src/function_pointer.c"

cat > "$work/src/settings_session.kry" <<'EOF'
#import "thing.h"
ui :: #import "settings_ui"

draw_session :: (label: const char*, w: int) -> int {
    return ui.toggle_row_height(label, w)
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/settings_session.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/args_local.kry" >"$err" 2>&1
grep -q 'static void\* worker(void\* userdata);' "$out/src/args_local.c"
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/settings_direct.kry" >"$err" 2>&1
grep -q 'return settings_ui_toggle_row_height(label, w);' "$out/src/settings_direct.c"

mkdir -p "$work/src/ui"
cat > "$work/src/ui/panel.kry" <<'EOF'
#module "ui.panel"
#import "thing.h"

draw :: (app: void*) {
    native (void)app
}

panel_c_entry :: (app: void*) #export {
    native (void)app
}
EOF

cat > "$work/src/panel_host.kry" <<'EOF'
#import "thing.h"
panel :: #import "src/ui/panel"

draw_panel_host :: (app: void*) {
    panel.draw(app)
}
EOF

"$kc" --no-main --root "$work" -o "$out" \
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/settings/types.kry" >"$err" 2>&1
grep -q '#include "thing.h"' "$out/src/settings/types.h"
grep -q 'typedef struct SettingsThemeState {' "$out/src/settings/types.h"
grep -q 'int value;' "$out/src/settings/types.h"

"$kc" --no-main --root "$work" -o "$out" \
    "$work/src/settings/types.kry" "$work/src/panel_host.kry" >"$err" 2>&1
grep -q '#include "src/settings/types.h"' "$out/kryon_project.h"

cat > "$work/src/panel_direct_host.kry" <<'EOF'
#import "thing.h"
#import "src/ui/panel"

draw_panel_direct_host :: (app: void*) {
    ui_panel.draw(app)
}
EOF

"$kc" --no-main --root "$work" -o "$out" \
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

if "$kc" --no-main --root "$work" -o "$out" \
    "$work/src/bad_import.kry" >"$err" 2>&1; then
    echo "removed import unexpectedly succeeded" >&2
    exit 1
fi
grep -q 'unknown top-level statement: import "src/ui/panel.kry"' "$err"

cat > "$work/src/preview.kry" <<'EOF'
preview stage_preview(viewport: Rectangle) {
    background BLACK
}
EOF

"$kc" --no-main --root "$work" -o "$out" "$work/src/preview.kry" >"$err" 2>&1

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

"$kc" --no-main --root "$work" -o "$out" "$work/src/state_multiline.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/native_c_features.kry" >"$err" 2>&1
grep -q '#include "stddef.h"' "$out/src/native_c_features.c"
grep -q '#define FEATURE_VALUE 7' "$out/src/native_c_features.c"
grep -q '#if ((defined(ANDROID_BUILD)))' "$out/src/native_c_features.c"
grep -Fq 'int platform_ping(int value, const char* tag);' "$out/src/native_c_features.c"
grep -Fq 'void* platform_context(void);' "$out/src/native_c_features.c"
grep -q 'static int helper_value(void);' "$out/src/native_c_features.c"
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/top_level_macros.kry" >"$err" 2>&1
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
grep -q 'static int platform_test_desktop_value(void);' "$out/src/top_level_macros.c"
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/native_structs.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/multiline_fn_decl.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/implicit_call.kry" >"$err" 2>&1
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

"$kc" --no-main --root "$work" -o "$out" "$work/src/expression_statement.kry" >"$err" 2>&1
grep -q '(\*env)->CallVoidMethod(env, object, method);' "$out/src/expression_statement.c"
grep -q '(\*env)->DeleteLocalRef( env, object);' "$out/src/expression_statement.c"

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

if "$kc" --no-main --root "$work" -o "$out" "$work/src/bad_var.kry" >"$err" 2>&1; then
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

if "$kc" --no-main --root "$work" -o "$out" "$work/src/bad_do.kry" >"$err" 2>&1; then
    echo "removed do statement was accepted" >&2
    exit 1
fi
grep -q "'do' syntax was removed" "$err"
grep -Eq 'bad_do\.kry:2:1: error:' "$err"

cat > "$work/src/bad_goto.kry" <<'EOF'
screen bad {
    goto
}
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/bad_goto.kry" >"$err" 2>&1; then
    echo "bad goto statement was accepted" >&2
    exit 1
fi
grep -q 'expected goto label name' "$err"
grep -Eq 'bad_goto\.kry:2:1: error:' "$err"

cat > "$work/src/unbalanced.kry" <<'EOF'
screen bad {
    InitializeThing()
EOF

if "$kc" --no-main --root "$work" -o "$out" "$work/src/unbalanced.kry" >"$err" 2>&1; then
    echo "unbalanced braces were accepted" >&2
    exit 1
fi
grep -q 'unbalanced braces' "$err"
grep -Eq 'unbalanced\.kry:[0-9]+:1: error:' "$err"
