#!/bin/sh
# 9c compatibility check for C users consuming Kryon's clean public surface.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
plan9=${PLAN9PORT_DIR:-/mnt/storage/Projects/plan9port}
build=${BUILD_DIR:-build/linux-x86_64}
work=${TMPDIR:-/tmp}/kryon-libdraw-9c.$$

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -x "$plan9/bin/9c" ] || [ ! -x "$plan9/bin/9l" ]; then
    echo "libdraw 9c test: plan9port 9c/9l not found at $plan9 - skipping" >&2
    exit 0
fi

mkdir -p "$work"
cat > "$work/main.c" <<'SRC'
#include "kryon.h"

#include <stdio.h>

#define BeginFrame BeginDrawing
#define EndFrame EndDrawing

int
main(int argc, char **argv)
{
    Color color = RED;
    Rectangle rect = {10, 20, 30, 40};
    Vector2 point = {1, 2};

    (void)argv;
    if(color.a != 255 || rect.width != 30 || point.y != 2)
        return 1;
    if(argc == 12345) {
        InitWindow(240, 160, "9c clean surface");
        BeginFrame();
        ClearBackground(BLACK);
        DrawRectangle(8, 8, 40, 24, RAYWHITE);
        EndFrame();
        CloseWindow();
    }
    printf("libdraw 9c clean surface ok\n");
    return 0;
}
SRC

make -C "$root" KRYON_BACKEND=libdraw PLAN9PORT_DIR="$plan9" "$build/libkryon.a"
PLAN9="$plan9" PATH="$plan9/bin:$PATH" "$plan9/bin/9c" \
    -I"$root/include" -o "$work/main.o" "$work/main.c"
PLAN9="$plan9" PATH="$plan9/bin:$PATH" "$plan9/bin/9l" \
    -o "$work/libdraw_9c_smoke" "$work/main.o" "$root/$build/libkryon.a" \
    -L"$plan9/lib" -ldraw -lmemdraw -lmux -lthread -l9 -lpthread -lm -ldl -lrt
"$work/libdraw_9c_smoke"
