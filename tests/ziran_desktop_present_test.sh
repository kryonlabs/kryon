#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-presentation"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
# Generate only the test interposer, following the native harness convention.
cat > "$work/probe.c" <<'C'
#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdbool.h>

static int updates, copies, presents;
static bool reject_update, reject_copy;

void desktop_test_reset(void) { updates = copies = presents = 0; }
bool desktop_test_counts(int uploads, int draws, int shown) {
    return updates == uploads && copies == draws && presents == shown;
}
void desktop_test_reject_update(bool reject) { reject_update = reject; }
void desktop_test_reject_copy(bool reject) { reject_copy = reject; }
bool desktop_test_pixel(SDL_Window *window, Uint32 expected) {
    SDL_Rect rect = {50, 50, 1, 1};
    Uint32 actual = 0;
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    return renderer != NULL && SDL_RenderReadPixels(renderer, &rect,
        SDL_PIXELFORMAT_ARGB8888, &actual, sizeof(actual)) == 0 && actual == expected;
}

int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect,
                      const void *pixels, int pitch) {
    int (*next)(SDL_Texture *, const SDL_Rect *, const void *, int);
    *(void **)(&next) = dlsym(RTLD_NEXT, "SDL_UpdateTexture");
    assert(next != NULL);
    updates++;
    return reject_update ? -1 : next(texture, rect, pixels, pitch);
}
int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
                   const SDL_Rect *source, const SDL_Rect *destination) {
    int (*next)(SDL_Renderer *, SDL_Texture *, const SDL_Rect *, const SDL_Rect *);
    *(void **)(&next) = dlsym(RTLD_NEXT, "SDL_RenderCopy");
    assert(next != NULL);
    copies++;
    return reject_copy ? -1 : next(renderer, texture, source, destination);
}
void SDL_RenderPresent(SDL_Renderer *renderer) {
    void (*next)(SDL_Renderer *);
    *(void **)(&next) = dlsym(RTLD_NEXT, "SDL_RenderPresent");
    assert(next != NULL);
    presents++;
    next(renderer);
}
C
"${CC:-cc}" -std=c11 $(pkg-config --cflags sdl2) -c "$work/probe.c" -o "$work/probe.o"
libs="$work/probe.o $(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -ldl -lm -lpthread"
source=$repo/tests/desktop_present_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests
    if test "$form" = saved; then
        input=$work/ir/desktop_present_test.zir
        root=$work/ir
    fi
    for target in c cpp; do
        executable=--exe; if test "$target" = cpp; then executable=; fi
        output=$work/$target-$form
        LDLIBS="$libs" "$ziran" build --root "$root" --module-path "$root" \
            --module-path "$repo/src/backend" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
            --target="$target" --entry desktop_present_test:main $executable \
            -o "$output" "$input"
        if test "$target" = cpp; then
            printf '#include "desktop_present_test.hpp"\nint main() { return desktop_present_test_main(); }\n' > "$output/entry.cpp"
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$output"/*.cpp $libs \
                -o "$output/desktop_present_test"
        fi
        xvfb-run -a "$output/desktop_present_test" > "$work/$target-$form.log"
        tail -1 "$work/$target-$form.log"
    done
done
