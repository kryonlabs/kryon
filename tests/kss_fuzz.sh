#!/bin/sh
# Build tests/kss_fuzz.zi with AddressSanitizer and UndefinedBehaviorSanitizer
# and run it over the shipped style packs.
#
#   sh tests/kss_fuzz.sh SEED COUNT      COUNT cases from SEED
#   sh tests/kss_fuzz.sh check FILE      the checks on one saved sheet
#   sh tests/kss_fuzz.sh shrink FILE     the smallest part of FILE that still
#                                        fails, kept as build/kss-fuzz/shrunk.kss
#
# A failing sheet is kept as build/kss-fuzz/failure-SEED.kss.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran_root=${ZIRAN_DIR:-"$repo/../ziran"}
ziran=${ZIRAN_BIN:-"$ziran_root/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$ziran_root/build/libziran.a"}
timeout=${FUZZ_TIMEOUT:-600}
if test "$#" -ne 2; then
    echo "usage: $0 SEED COUNT | check FILE | shrink FILE" >&2
    exit 2
fi

out=$repo/build/kss-fuzz
mkdir -p "$out"
build=$(mktemp -d "$out/build.XXXXXX")
trap 'rm -rf "$build"' EXIT HUP INT TERM
"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/kss" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --entry kss_fuzz:main \
    -o "$build/c" "$repo/tests/kss_fuzz.zi"
"${CC:-cc}" -std=c11 -O1 -g -fno-omit-frame-pointer \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -I"$ziran_root/include" -I"$build/c" "$build/c"/*.c "$ziran_lib" \
    -o "$build/kss_fuzz"

# The parser passes its large state by value; give the sanitized stack room.
ulimit -s unlimited 2>/dev/null || ulimit -s 1048576 2>/dev/null || true
current=$build/current.kss
if test "$1" = check; then
    env -u DISPLAY -u WAYLAND_DISPLAY ASAN_OPTIONS=detect_leaks=0 \
        "$build/kss_fuzz" "$current" check "$2"
    exit
fi
if test "$1" = shrink; then
    env -u DISPLAY -u WAYLAND_DISPLAY ASAN_OPTIONS=detect_leaks=0 \
        "$build/kss_fuzz" "$current" shrink "$2"
    if test -f "$current"; then cp "$current" "$out/shrunk.kss"; fi
    exit
fi
status=0
# Sheets that once failed come first; see tests/kss_corpus.
env -u DISPLAY -u WAYLAND_DISPLAY ASAN_OPTIONS=detect_leaks=0 \
    timeout "$timeout" "$build/kss_fuzz" "$current" corpus \
    "$repo"/tests/kss_corpus/*.kss || status=$?
if test "$status" -ne 0; then
    echo "kss fuzz: a corpus sheet failed; it is kept in build/kss-fuzz/failure-corpus.kss" >&2
    cp "$current" "$out/failure-corpus.kss"
    exit 1
fi
env -u DISPLAY -u WAYLAND_DISPLAY ASAN_OPTIONS=detect_leaks=0 \
    timeout "$timeout" "$build/kss_fuzz" "$current" "$1" "$2" \
    "$repo"/styles/kryon/*.kss || status=$?
if test "$status" -ne 0; then
    cp "$current" "$out/failure-$1.kss"
    if test "$status" -eq 124; then
        echo "kss fuzz: seed $1 ran longer than $timeout seconds" >&2
    fi
    echo "kss fuzz: failing sheet kept in build/kss-fuzz/failure-$1.kss" >&2
    echo "kss fuzz: rerun it with sh tests/kss_fuzz.sh check build/kss-fuzz/failure-$1.kss" >&2
    exit 1
fi
