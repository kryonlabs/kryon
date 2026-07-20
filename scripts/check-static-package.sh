#!/bin/sh
set -eu

archive=${1:?usage: check-static-package.sh kryon-static.tar.gz}
if [ -n "${CC:-}" ]; then
    cc=$CC
elif command -v clang >/dev/null 2>&1; then
    cc=clang
else
    cc=cc
fi
work=${TMPDIR:-/tmp}/kryon-static-package-check.$$
trap 'rm -rf "$work"' EXIT INT TERM

mkdir -p "$work"
tar -xzf "$archive" -C "$work"
root=$(find "$work" -maxdepth 1 -type d -name 'kryon-*-static' | head -n 1)
if [ -z "$root" ]; then
    echo "could not find extracted Kryon package root" >&2
    exit 1
fi

for path in \
    include/kryon.h \
    include/markdown.h \
    lib/libkryon.a \
    lib/libraylib.a \
    lib/liboqs.a \
    lib/libcurl.a \
    lib/libcmark-gfm.a \
    lib/libcmark-gfm-extensions.a \
    lib/pkgconfig/kryon.pc \
    lib/cmake/kryon/KryonConfig.cmake \
    manifest.json \
    share/licenses/kryon/THIRD_PARTY_NOTICES.md \
    examples/minimal.c \
    examples/markdown.c; do
    if [ ! -e "$root/$path" ]; then
        echo "missing package file: $path" >&2
        exit 1
    fi
done

if ar -t "$root/lib/libkryon.a" | grep -Eq '(^|/)(lib.*\.a|kryon_compat\.generated\.h)$'; then
    echo "libkryon.a contains nested archives or generated headers" >&2
    exit 1
fi

pkgdir=$root/lib/pkgconfig
PKG_CONFIG_PATH=$pkgdir pkg-config --cflags --libs kryon >/dev/null
$cc "$root/examples/markdown.c" $(PKG_CONFIG_PATH=$pkgdir pkg-config --cflags --libs kryon) -o "$work/markdown"
"$work/markdown"
$cc "$root/examples/minimal.c" $(PKG_CONFIG_PATH=$pkgdir pkg-config --cflags --libs kryon) -o "$work/minimal"
