#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)

# Every maintained src file is Ziran, except the module inventory. Checked
# programs cannot reintroduce a handwritten implementation language or one of
# the retired Kry formats. Both inventories stay empty.
expected_src=''
expected_programs=''

actual_src=$(cd "$repo" && rg --files -uu src -g '!*.zi' -g '!modules.txt' |
    LC_ALL=C sort)
actual_programs=$(cd "$repo" &&
    rg --files -uu src examples tests tools \
        -g '*.c' -g '*.cc' -g '*.cpp' -g '*.go' -g '*.java' \
        -g '*.kry' -g '*.kir' -g '*.krb' |
    LC_ALL=C sort)

if test "$actual_src" != "$expected_src"; then
    printf 'Non-Ziran files remain under src. Maintained source must be .zi.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected_src" "$actual_src" >&2
    exit 1
fi

if test "$actual_programs" != "$expected_programs"; then
    printf 'Non-Ziran implementation files or retired formats remain.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected_programs" "$actual_programs" >&2
    exit 1
fi
