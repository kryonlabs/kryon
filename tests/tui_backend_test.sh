#!/bin/sh
# TUI backend test - builds a full Kryon app with KRYON_BACKEND=tui, feeds
# terminal input, and verifies that the renderer emits truecolor ANSI cells.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
build=${BUILD_DIR:-build/linux-x86_64-tui}
bin="$root/$build/tests/tui_smoke_test"
work=${TMPDIR:-/tmp}/kryon-tui-test.$$

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

mkdir -p "$work"
make -C "$root" BUILD_DIR="$build" KRYON_BACKEND=tui "$build/tests/tui_smoke_test"

input=$(printf 'a\033[A\033[<0;5;4M\033[<0;5;4m\033[<64;5;4M')
out="$work/tui.out"

if command -v script >/dev/null 2>&1; then
    printf '%s' "$input" | script -q -c "$bin" "$out" >/dev/null
else
    printf '%s' "$input" | env KRYON_TUI_FORCE=1 "$bin" > "$out"
fi

test -s "$out"
grep "$(printf '\033\\[38;2;')" "$out" >/dev/null
grep "$(printf '\033\\[?1049h')" "$out" >/dev/null
grep "$(printf '\033\\[?1049l')" "$out" >/dev/null
echo "tui backend smoke ok"

