#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Kryon source and checked test programs are Ziran; this list stays empty.
expected=''

actual=$(cd "$repo" &&
    rg --files src examples tests tools -g '*.c' -g '*.cc' -g '*.cpp' -g '*.go' -g '*.java' |
    LC_ALL=C sort)

if test "$actual" != "$expected"; then
    printf 'Non-Ziran source inventory changed. Kryon source and tests must\n' >&2
    printf 'use .zi instead of handwritten implementation files.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected" "$actual" >&2
    exit 1
fi
