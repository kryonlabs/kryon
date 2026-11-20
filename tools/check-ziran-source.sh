#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)

# Maintained src files are Ziran, the module inventory, or versioned API
# example sidecars. Checked programs cannot reintroduce a handwritten
# implementation language or a retired Kry format.
expected_src=''
expected_programs=''

actual_src=$(cd "$repo" && rg --files -uu src -g '!*.zi' -g '!modules.txt' |
    while IFS= read -r path; do
        case "$path" in
            *.zi.examples)
                if test -f "${path%.examples}" &&
                    IFS= read -r header < "$path" &&
                    test "$header" = 'ziran-api-examples-v1'; then
                    continue
                fi
                ;;
        esac
        printf '%s\n' "$path"
    done |
    LC_ALL=C sort)
actual_programs=$(cd "$repo" &&
    rg --files -uu src examples tests tools \
        -g '*.c' -g '*.cc' -g '*.cpp' -g '*.go' -g '*.java' \
        -g '*.kry' -g '*.kir' -g '*.krb' -g '!**/build/**' |
    LC_ALL=C sort)


if test "$actual_src" != "$expected_src"; then
    printf 'Unexpected files under src. Use .zi, modules.txt, or a versioned API example sidecar with matching .zi source.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected_src" "$actual_src" >&2
    exit 1
fi

if test "$actual_programs" != "$expected_programs"; then
    printf 'Non-Ziran implementation files or retired formats remain.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected_programs" "$actual_programs" >&2
    exit 1
fi
