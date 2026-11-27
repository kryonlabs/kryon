#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)

# Native implementations remain Ziran. Typed UI bindings live under src/tsx;
# the module inventory and versioned API example sidecars are metadata.
# Handwritten target-language implementations and retired Kry formats stay banned.
expected_src=''
expected_programs=''

actual_src=$(cd "$repo" && rg --files -uu src -g '!*.zi' -g '!modules.txt' |
    while IFS= read -r path; do
        case "$path" in
            src/tsx/*.ts|src/tsx/*.tsx) continue ;;
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
# `build/` and the generated Android project (`droid/`, written by
# `kryon build --profile android`) hold generated C and Java, not sources.
actual_programs=$(cd "$repo" &&
    rg --files -uu src examples tests tools \
        -g '*.c' -g '*.cc' -g '*.cpp' -g '*.go' -g '*.java' \
        -g '*.kry' -g '*.kir' -g '*.krb' -g '!**/build/**' -g '!**/droid/**' |
    LC_ALL=C sort)


if test "$actual_src" != "$expected_src"; then
    printf 'Unexpected files under src. Use Ziran for native implementation, TS/TSX under src/tsx for UI bindings, modules.txt, or a versioned API example sidecar with matching .zi source.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected_src" "$actual_src" >&2
    exit 1
fi

if test "$actual_programs" != "$expected_programs"; then
    printf 'Handwritten target-language implementation files or retired formats remain.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected_programs" "$actual_programs" >&2
    exit 1
fi
