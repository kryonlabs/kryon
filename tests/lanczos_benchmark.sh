#!/bin/sh
# Benchmarks Kryon's Lanczos (std/resample, as the shared image cache runs
# it) against Pillow, Pillow-SIMD, libvips and stb_image_resize2 on x86-64
# and on the ARMv7 Moto over ADB: ms/Mpx, extra peak memory and PSNR against
# a float64 Lanczos-3. See tests/lanczos_benchmark.py. The test-only oracles
# live in build/scratch/lanczos-bench/oracles: a venv with Pillow-SIMD, built
# for AVX2, and pyvips with its bundled libvips, and stb_image_resize2.h.
# Run it through build-queue; --quick skips the large shrink.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work="$repo/build/scratch/lanczos-bench"
oracles="$work/oracles"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
stb="$oracles/stb"
if ! test -f "$stb/stb_image_resize2.h"; then
    mkdir -p "$stb"
    curl -fsSL -o "$stb/stb_image_resize2.h.part" \
        https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h
    mv "$stb/stb_image_resize2.h.part" "$stb/stb_image_resize2.h"
fi
# stb_image_resize2 v2.18.
echo "771c1eb7193fe0682f7cadd6040e906ce625d32cba311e2274c6b09a2875179e  $stb/stb_image_resize2.h" | sha256sum -c --quiet -
venv="$oracles/venv"
if ! "$venv/bin/python" -c 'import PIL, pyvips' 2>/dev/null; then
    python3 -m venv "$venv"
    CC="${CC:-cc} -mavx2" "$venv/bin/pip" install --quiet --no-binary pillow-simd pillow-simd 'pyvips[binary]'
fi
ndk=${ANDROID_NDK_HOME:-$(ls -d "$HOME"/Android/Sdk/ndk/* 2>/dev/null | sort -V | tail -n 1)}
ORACLE_PYTHON="$venv/bin/python" STB_DIR="$stb" ANDROID_NDK_HOME="$ndk" \
    python3 "$repo/tests/lanczos_benchmark.py" "$@"
