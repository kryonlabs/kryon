#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# This list only shrinks as the remaining platform bridges move to current Ziran.
expected='src/platform/android/java/com/kryonlabs/kryon/KryonActivity.java
src/platform/android/java/com/kryonlabs/kryon/SecureStore.java
src/platform/android/java/com/kryonlabs/kryon/TextInputBridge.java'

actual=$(cd "$repo" &&
    rg --files src examples tools -g '*.c' -g '*.cc' -g '*.cpp' -g '*.go' -g '*.java' |
    LC_ALL=C sort)

if test "$actual" != "$expected"; then
    printf 'Non-Ziran source inventory changed. Remove migrated paths from the\n' >&2
    printf 'expected list; do not add new handwritten implementation files.\n' >&2
    printf 'Expected:\n%s\nActual:\n%s\n' "$expected" "$actual" >&2
    exit 1
fi
