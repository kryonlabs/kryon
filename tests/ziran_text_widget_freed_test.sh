#!/bin/sh
# A fresh Text passed straight into a Text widget's TextProps literal is
# freed by Ziran after the call: C counts no allocations left after a
# thousand frames of a plain line, a line PaintTextLines shortens to "..."
# (a reassigned view), and a selectable line drawn through the installed
# #reads TextSelectionPainter. AddressSanitizer finds no read of a freed
# label, and source and saved IR agree.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
name=ziran_text_widget_freed_test
source=$repo/tests/$name.zi
"$ziran" ir --root "$repo/tests" $kryon_project_args \
     -o "$work/ir" "$source"
cat > "$work/count.c" <<'C'
#include "ziran_text_widget_freed_test.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
void *__real_realloc(void *, size_t);
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
static long outstanding;
void *__wrap_malloc(size_t size) { void *r = __real_malloc(size); if (r) outstanding++; return r; }
void *__wrap_calloc(size_t n, size_t size) { void *r = __real_calloc(n, size); if (r) outstanding++; return r; }
void *__wrap_realloc(void *p, size_t size) { void *r = __real_realloc(p, size); if (!p && r) outstanding++; return r; }
void __wrap_free(void *p) { if (p) outstanding--; __real_free(p); }
int main(void) {
    static const char *const kinds[] = {"Text", "Text with ellipsis",
        "selectable Text through the selection painter"};
    int failed = 0;
    for (int kind = 0; kind < 3; kind++) {
        int warm = PaintFrames(kind, 20);
        long before = outstanding;
        int drawn = PaintFrames(kind, 1000);
        if (warm != 20 || drawn != 1000) {
            printf("%s: drew %d of 20 frames, then %d of 1000\n", kinds[kind], warm, drawn);
            return 1;
        }
        long left = outstanding - before;
        printf("%s: %ld allocations left after 1000 frames\n", kinds[kind], left);
        if (left != 0) failed = 1;
    }
    return failed ? 2 : PaintCheck();
}
C
cat > "$work/plain.c" <<'C'
#include "ziran_text_widget_freed_test.h"
int main(void) { return PaintCheck(); }
C
for form in source saved; do
    input=$source
    module_root=$repo/tests
    module_path="$kryon_project_args"
    if test "$form" = saved; then
        input=$work/ir/$name.zir
        module_root=$work/ir
        module_path="--project --module-path $work/ir"
    fi
    library=$work/$form-c
    "$ziran" build --target=c --root "$module_root" $module_path \
         -o "$library" "$input"
    "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$ziran_root/include" -I"$library" \
        "$library"/*.c "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=malloc \
        -Wl,--wrap=calloc -Wl,--wrap=free -o "$work/count-$form"
    echo "$form:"
    timeout 60 "$work/count-$form"
    "${CC:-cc}" -std=c11 -g -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -fsanitize=address,undefined \
        -fno-sanitize-recover=all -fno-omit-frame-pointer \
        -I"$ziran_root/include" -I"$library" "$library"/*.c "$work/plain.c" \
        -o "$work/asan-$form"
    ASAN_OPTIONS=detect_leaks=0 timeout 120 "$work/asan-$form"
done
echo 'Text frees a fresh label after painting it, through ellipsis and the selection painter'
