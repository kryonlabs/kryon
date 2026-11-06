#!/bin/sh
# Labels, emphasis, slanted text, image paths and Text widget labels are
# copied into the session's frame text when they are queued, so the caller
# may free them at once. Fresh `Text` arguments to the paint calls are freed
# by Ziran right after the statement: C counts no allocations left after a
# thousand frames of each kind, AddressSanitizer finds no read of a freed
# label, and the frame text keeps its first capacity. Source and saved IR
# agree on C, Go and portable bundles.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
name=ziran_paint_text_freed_test
source=$repo/tests/$name.zi
host=$name
set -- \
    --bind font_metrics:MeasureGlyphWidth=$host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=$host:MeasureGlyphLineHeight \
    --bind raster:RasterLine=$host:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=$host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=$host:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=$host:RasterText \
    --bind raster_text:RasterTextClipped=$host:RasterTextClipped \
    --bind raster_text:RasterTextRotated=ziran_mesh_noop_host:RasterTextRotated \
    --bind raster_triangles:RasterTriangles=$host:RasterTriangles \
    --bind paint_queue:RasterImage=$host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
cat > "$work/count.c" <<'C'
#include "ziran_paint_text_freed_test.h"
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
    static const char *const kinds[] = {"PaintLabel", "PaintClippedLabel",
        "PaintClippedEmphasis", "PaintClippedSlanted", "PaintImage", "Text",
        "PaintLabel outside a frame"};
    int failed = 0;
    for (int kind = 0; kind < 7; kind++) {
        int warm = PaintFrames(kind, 20);
        long before = outstanding;
        int drawn = PaintFrames(kind, 1000);
        if (warm != 20 || drawn != 1000) {
            printf("%s: drew %d of 20, then %d of 1000\n", kinds[kind], warm, drawn);
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
#include "ziran_paint_text_freed_test.h"
int main(void) { return PaintCheck(); }
C
for form in source saved; do
    input=$source
    module_root=$repo/tests
    module_path=$repo/src/ui
    if test "$form" = saved; then
        input=$work/ir/$name.zir
        module_root=$work/ir
        module_path=$work/ir
    fi
    # A library build, without an entry, is called from the counting and
    # sanitized C programs.
    library=$work/$form-c
    "$ziran" build --target=c --root "$module_root" --module-path "$module_path" \
        --module-path "$ziran_root/std" -o "$library" "$input"
    "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$ziran_root/include" -I"$library" \
        "$library"/*.c "$work/count.c" -Wl,--wrap=realloc -Wl,--wrap=malloc \
        -Wl,--wrap=calloc -Wl,--wrap=free -o "$work/count-$form"
    timeout 60 "$work/count-$form"
    "${CC:-cc}" -std=c11 -g -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -fsanitize=address,undefined \
        -fno-sanitize-recover=all -fno-omit-frame-pointer \
        -I"$ziran_root/include" -I"$library" "$library"/*.c "$work/plain.c" \
        -o "$work/asan-$form"
    ASAN_OPTIONS=detect_leaks=0 timeout 120 "$work/asan-$form"
    "$ziran" build --target=go --pkg main --entry "$name:PaintCheck" "$@" \
        --root "$module_root" --module-path "$module_path" \
        --module-path "$ziran_root/std" --exe -o "$work/$form-go" "$input"
    timeout 60 env GO111MODULE=off go run "$work/$form-go"/*.go
    "$ziran" bundle --entry "$name:PaintCheck" "$@" --root "$module_root" \
        --module-path "$module_path" --module-path "$ziran_root/std" \
        -o "$work/$form.zx" "$input"
    test "$(timeout 60 "$ziran" run "$work/$form.zx")" = 0
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Kryon copies queued labels and paths into reused frame text, so a caller may free them at once'
