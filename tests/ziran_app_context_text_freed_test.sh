#!/bin/sh
# AppService, DocumentWrite, DocumentRead and AppReply only read their string
# arguments during the call, so they are #reads: a fresh Text passed to them is
# freed by Ziran right after the call. C counts no allocations left after a
# thousand rounds of each, and AddressSanitizer finds no read of a freed string.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
name=ziran_app_context_text_freed_test
cat > "$work/count.c" <<'C'
#include "ziran_app_context_text_freed_test.h"
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
    static const char *const kinds[] = {"AppService", "DocumentWrite", "AppReply", "DocumentRead"};
    int failed = 0;
    for (int kind = 0; kind < 4; kind++) {
        int warm = CallRounds(kind, 20);
        long before = outstanding;
        int seen = CallRounds(kind, 1000);
        int expected = kind == 0 || kind == 1 ? 2000 : 1000;
        if (warm < 0 || seen != expected) {
            printf("%s: host saw %d of %d strings\n", kinds[kind], seen, expected);
            return 1;
        }
        long left = outstanding - before;
        printf("%s: %ld allocations left after 1000 rounds\n", kinds[kind], left);
        if (left != 0) failed = 1;
    }
    return failed ? 2 : 0;
}
C
cat > "$work/plain.c" <<'C'
#include "ziran_app_context_text_freed_test.h"
int main(void) {
    for (int kind = 0; kind < 4; kind++) if (CallRounds(kind, 200) < 0) return 1;
    return 0;
}
C
library=$work/c
"$ziran" build --target=c --no-main --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$library" "$repo/tests/$name.zi"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ziran_root/include" -I"$library" "$library"/*.c "$work/count.c" \
    -Wl,--wrap=realloc -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free -o "$work/count"
timeout 60 "$work/count"
"${CC:-cc}" -std=c11 -g -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
    -I"$ziran_root/include" -I"$library" "$library"/*.c "$work/plain.c" -o "$work/asan"
ASAN_OPTIONS=detect_leaks=0 timeout 120 "$work/asan"
echo 'Fresh Text passed to AppService, DocumentWrite, DocumentRead and AppReply is freed after the call'
