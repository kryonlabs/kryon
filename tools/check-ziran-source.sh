#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Kryon implementation source is 100% Ziran; this list stays empty.
expected=''

actual=$(cd "$repo" &&
    rg --files src examples tools -g '*.c' -g '*.cc' -g '*.cpp' -g '*.go' -g '*.java' |
    LC_ALL=C sort)

if test "$actual" != "$expected"; then
    printf 'Non-Ziran source inventory changed. Implementation source must be\n' >&2
    printf '.zi; do not add handwritten implementation files.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected" "$actual" >&2
    exit 1
fi
