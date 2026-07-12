#!/bin/sh
set -eu

archive=${1:?usage: check-static-package.sh flint-static.tar.gz}
if [ -n "${CC:-}" ]; then
    cc=$CC
elif command -v clang >/dev/null 2>&1; then
    cc=clang
else
    cc=cc
fi
work=${TMPDIR:-/tmp}/flint-static-package-check.$$
trap 'rm -rf "$work"' EXIT INT TERM

mkdir -p "$work"
tar -xzf "$archive" -C "$work"
root=$(find "$work" -maxdepth 1 -type d -name 'flint-*-static' | head -n 1)
if [ -z "$root" ]; then
    echo "could not find extracted Flint package root" >&2
    exit 1
fi

for path in \
    include/flint.h \
    include/markdown.h \
    lib/libflint.a \
    lib/libraylib.a \
    lib/liboqs.a \
    lib/libcurl.a \
    lib/libcmark-gfm.a \
    lib/libcmark-gfm-extensions.a \
    lib/pkgconfig/flint.pc \
    lib/cmake/flint/FlintConfig.cmake \
    manifest.json \
    share/licenses/flint/THIRD_PARTY_NOTICES.md \
    examples/minimal.c \
    examples/markdown.c; do
    if [ ! -e "$root/$path" ]; then
        echo "missing package file: $path" >&2
        exit 1
    fi
done

if ar -t "$root/lib/libflint.a" | grep -Eq '(^|/)(lib.*\.a|flint_compat\.generated\.h)$'; then
    echo "libflint.a contains nested archives or generated headers" >&2
    exit 1
fi

pkgdir=$root/lib/pkgconfig
PKG_CONFIG_PATH=$pkgdir pkg-config --cflags --libs flint >/dev/null
$cc "$root/examples/markdown.c" $(PKG_CONFIG_PATH=$pkgdir pkg-config --cflags --libs flint) -o "$work/markdown"
"$work/markdown"
$cc "$root/examples/minimal.c" $(PKG_CONFIG_PATH=$pkgdir pkg-config --cflags --libs flint) -o "$work/minimal"
