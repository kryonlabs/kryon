#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$repo/build/test/libdraw_native_plan9.$$
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/libdraw_native_plan9_host.zi
capture=$work/capture.rgba
text=$work/text.log

"$ziran" check --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" "$source"
"$ziran" build --target=plan9-c --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std" -o "$work/generated" "$source"
if rg -n 'cairo|dlsym|usleep' "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'native libdraw output retained a plan9port dependency' >&2
    exit 1
fi
if rg -n '__asm__' "$work/generated"/*.c "$work/generated"/*.h; then
    echo 'native libdraw output retained unsupported assembler names' >&2
    exit 1
fi
rg -q '^DrawDisplay\* initdisplay\(' "$work/generated/libdraw_native.h"
rg -q '^int32_t gengetwindow\(' "$work/generated/libdraw_native.h"
rg -q '^DrawImage\* allocimage\(' "$work/generated/libdraw_native.h"
rg -q '^DrawPoint string\(' "$work/generated/libdraw_native.h"

mkdir "$work/include"
cat > "$work/include/u.h" <<'H'
#ifndef FAKE_U_H
#define FAKE_U_H
typedef signed char schar;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef long vlong;
typedef unsigned long uvlong;
typedef unsigned long ulong;
typedef unsigned long usize;
#endif
H
cat > "$work/include/libc.h" <<'H'
#ifndef FAKE_LIBC_H
#define FAKE_LIBC_H
extern void *malloc(usize);
extern void *calloc(usize, usize);
extern void *realloc(void *, usize);
extern void free(void *);
extern void *memmove(void *, const void *, usize);
extern void *memset(void *, int, usize);
extern int memcmp(const void *, const void *, usize);
extern int fprint(int, const char *, ...);
extern void abort(void);
extern int snprint(char *, int, const char *, ...);
extern void exits(const char *);
extern char *getenv(const char *);
extern int create(const char *, int, int);
extern long write(int, const void *, unsigned long);
extern int close(int);
#endif
H
cat > "$work/fake.c" <<'H'
#include <stdarg.h>
#include "libdraw_native.h"

extern int vsnprintf(char *, usize, const char *, va_list);
extern void exit(int);
extern usize strlen(const char *);
extern int open(const char *, int, ...);

static DrawImage fake_window;
static DrawScreen fake_screen;
static DrawSubfont fake_subfont = {
    NULL, 128, 16, 12, NULL, NULL, 1
};
static DrawFont fake_font = {NULL, NULL, 16, 12};
static uint32_t fake_last_color;

DrawDisplay *initdisplay(int8_t *device, int8_t *window, DrawErrorRoutine *error) {
    (void)device; (void)window; (void)error;
    return (void *)1;
}
void closedisplay(DrawDisplay *display) { (void)display; }
DrawSubfont *getdefont(DrawDisplay *display) { (void)display; return &fake_subfont; }
DrawFont *buildfont(DrawDisplay *display, int8_t *description, int8_t *name) {
    (void)display; (void)description; (void)name;
    return &fake_font;
}
int gengetwindow(DrawDisplay *display, int8_t *name,
                 DrawImage **window, DrawScreen **screen, int refresh) {
    (void)display; (void)name; (void)refresh;
    fake_window.bounds.max.x = 96;
    fake_window.bounds.max.y = 64;
    *window = &fake_window;
    *screen = &fake_screen;
    return 1;
}
DrawImage *allocimage(DrawDisplay *display, DrawRectangle bounds,
                      uint32_t channel, int replicate, uint32_t color) {
    (void)display; (void)bounds; (void)channel; (void)replicate;
    uint8_t *memory = calloc(1, sizeof(DrawImage) + sizeof(uint32_t));
    if(memory == NULL) return NULL;
    *(uint32_t *)(memory + sizeof(DrawImage)) = color;
    return (DrawImage *)memory;
}
void freeimage(DrawImage *image) {
    if(image != &fake_window) free(image);
}
void draw(DrawImage *target, DrawRectangle bounds, DrawImage *source,
          void *mask, DrawPoint point) {
    (void)bounds; (void)mask; (void)point;
    if(target == NULL || source == NULL || source == &fake_window) return;
    fake_last_color = *(uint32_t *)((uint8_t *)source + sizeof(DrawImage));
}
DrawPoint string(DrawImage *target, DrawPoint point, DrawImage *color,
                 DrawPoint background, DrawFont *font, int8_t *text) {
    const char *path = getenv("FAKE_TEXT_PATH");
    (void)target; (void)background; (void)font;
    if(color != NULL)
        fake_last_color = *(uint32_t *)((uint8_t *)color + sizeof(DrawImage));
    if(path != NULL && text != NULL) {
        int output = open((const char *)path, 577, 0644);
        usize length = strlen((const char *)text);
        if(output >= 0) {
            write(output, text, (usize)length);
            close(output);
        }
    }
    point.x += text == NULL ? 0 : (int32_t)strlen((char *)text) * 7;
    return point;
}
int stringwidth(DrawFont *font, int8_t *text) {
    (void)font;
    return text == NULL ? 0 : (int)strlen((char *)text) * 7;
}
int flushimage(DrawDisplay *display, int visible) {
    (void)display; (void)visible;
    return 1;
}
int unloadimage(DrawImage *image, DrawRectangle bounds, uint8_t *data,
                int count) {
    (void)image; (void)bounds;
    if(data == NULL || count < 4) return -1;
    for(int at = 0; at < count; at += 4) {
        data[at] = (uint8_t)fake_last_color;
        data[at + 1] = (uint8_t)(fake_last_color >> 8);
        data[at + 2] = (uint8_t)(fake_last_color >> 16);
        data[at + 3] = (uint8_t)(fake_last_color >> 24);
    }
    return count;
}
int freescreen(DrawScreen *screen) { (void)screen; return 0; }
void freefont(DrawFont *font) { (void)font; }
void freesubfont(DrawSubfont *font) { (void)font; }
void exits(const char *status) {
    if(status == NULL) exit(0);
    exit(status[0] == '4' && status[1] == '2' && status[2] == 0 ? 0 : 1);
}
int fprint(int fd, const char *format, ...) {
    (void)fd; (void)format;
    return 0;
}
int snprint(char *output, int capacity, const char *format, ...) {
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vsnprintf(output, (usize)capacity, format, arguments);
    va_end(arguments);
    return result;
}
int create(const char *path, int mode, int permissions) {
    (void)mode; (void)permissions;
    return open(path, 577, 0644);
}
H
CC=${CC:-cc}
"$CC" -std=c11 -I"$work/include" -I"$work/generated" \
    "$work/generated"/*.c "$work/fake.c" -o "$work/app"
KRYON_CAPTURE_PATH="$capture" FAKE_TEXT_PATH="$text" "$work/app"
python3 - "$capture" "$text" <<'PY'
from pathlib import Path
import sys
capture = Path(sys.argv[1]).read_bytes()
assert len(capture) == 96 * 64 * 4, len(capture)
assert capture[:4] == bytes([255, 0, 0, 255]), capture[:16]
assert Path(sys.argv[2]).read_text() == "Kryon"
PY
