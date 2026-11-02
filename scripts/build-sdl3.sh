#!/bin/sh
# Native support only: the package driver owns source resolution and publication.
set -eu
platform=$1
stage=$2
source=$3
[ "$platform" = linux-x86_64-gnu ] || { echo "unsupported SDL3 platform: $platform" >&2; exit 2; }
case "$("${CC:-cc}" -dumpmachine)" in
    x86_64*linux*gnu*) ;;
    *) echo 'SDL3 requires a Linux x86_64 GNU compiler' >&2; exit 2 ;;
esac
work=$(mktemp -d "$stage/.sdl3-build.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cmake -S "$source" -B "$work" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$stage" -DCMAKE_INSTALL_LIBDIR=lib \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
    -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=ON \
    -DSDL_INSTALL_DOCS=OFF -DSDL_INSTALL_CPACK=OFF \
    -DSDL_GPU=ON -DSDL_RENDER_GPU=ON -DSDL_VULKAN=ON
# Fail if CMake silently disabled the requested GPU renderer/backend.
config="$work/include-config-release/build_config/SDL_build_config.h"
for feature in SDL_GPU_VULKAN SDL_VIDEO_RENDER_GPU; do
    grep -q "^#define $feature 1" "$config" || { echo "SDL3 missing $feature" >&2; exit 2; }
done
cmake --build "$work" --parallel 1
cmake --install "$work"
mkdir -p "$stage/resources"
cp "$config" "$stage/resources/SDL_build_config.h"
cp "$source/LICENSE.txt" "$stage/resources/LICENSE-SDL3"
# Keep a declared digest for the complete SDK header set (Ziran recipes allow
# at most 64 output rows, while SDL3 installs more than 64 public headers).
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
    -cf "$stage/resources/SDL3-headers.tar" -C "$stage/include" SDL3
# SDL's pkg-config file is relocatable; produce Ziran's ordered link metadata
# from its actual static dependency flags rather than guessing system libraries.
python3 - "$stage" <<'PY'
from pathlib import Path
import shlex
import subprocess
import sys
stage = Path(sys.argv[1])
pc = stage / 'lib/pkgconfig/sdl3.pc'
text = pc.read_text()
text = '\n'.join('prefix=${pcfiledir}/../..' if line.startswith('prefix=') else line
                 for line in text.splitlines()) + '\n'
pc.write_text(text)
flags = shlex.split(subprocess.check_output(
    ['pkg-config', '--static', '--libs', str(pc)], text=True))
flags = [flag for flag in flags if flag != '-lSDL3' and
         not (flag.startswith('-L') and Path(flag[2:]).resolve() == stage / 'lib')]
(stage / 'link.txt').write_text('archive\tlib/libSDL3.a\n' +
                              ''.join('flag\t' + flag + '\n' for flag in flags))
PY
