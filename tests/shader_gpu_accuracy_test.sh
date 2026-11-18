#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
device=${SHADER_BENCH_DEVICE:-desktop}
out="$repo/build/scratch/shader-gpu-accuracy/$device"
mkdir -p "$out"
case "$device" in
    desktop) defines='--define POSIX_THREADS'; compiler=${CC:-cc}; libraries='-lEGL -lGLESv2 -lGLX -lX11 -lm -pthread' ;;
    moto|pixel)
        defines='--target-os android --target-cpu none --define POSIX_THREADS --define ANDROID_BUILD'
        arch=armv7a-linux-androideabi21
        test "$device" != pixel || arch=aarch64-linux-android21
        compiler="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/28.2.13676358}/toolchains/llvm/prebuilt/linux-x86_64/bin/$arch-clang"
        libraries='-lEGL -lGLESv3 -lm'
        ;;
    *) printf 'Unsupported GPU accuracy device: %s\n' "$device" >&2; exit 2 ;;
esac
"$ziran" build --project --root "$repo/tests" --target=c --no-main --entry shader_gpu_accuracy_test:main $defines -o "$out/c" "$repo/tests/shader_gpu_accuracy_test.zi"
python3 - "$out" "$ziran_root" "$compiler" "$libraries" <<'PY'
from pathlib import Path
import shlex, subprocess, sys
out, compiler_root, compiler, libraries = sys.argv[1:]
out = Path(out)
headers = sorted({p.parent for p in (out / 'c').rglob('*.h')})
subprocess.run([*shlex.split(compiler), '-std=c11', '-O2', '-I' + compiler_root + '/include',
                *[a for p in headers for a in ('-iquote', str(p))],
                *map(str, sorted((out / 'c').rglob('*.c'))), *shlex.split(libraries),
                '-o', str(out / 'shader_gpu_accuracy_test')], check=True)
PY
case "$device" in
    desktop)
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0 EGL_PLATFORM=surfaceless xvfb-run -a "$out/shader_gpu_accuracy_test"
        ;;
    moto)
        adb -s ZE2223BQZT push "$out/shader_gpu_accuracy_test" /data/local/tmp/kryon-shader-gpu-accuracy
        adb -s ZE2223BQZT shell chmod 700 /data/local/tmp/kryon-shader-gpu-accuracy
        adb -s ZE2223BQZT shell /data/local/tmp/kryon-shader-gpu-accuracy
        ;;
    pixel)
        destination=Projects/kryonlabs/kryon/build/scratch/shader-gpu-accuracy/pixel
        ssh thinkpad.local "mkdir -p $destination"
        scp "$out/shader_gpu_accuracy_test" "thinkpad.local:$destination/"
        ssh thinkpad.local "adb -s 09231JEC206210 push $destination/shader_gpu_accuracy_test /data/local/tmp/kryon-shader-gpu-accuracy && adb -s 09231JEC206210 shell chmod 700 /data/local/tmp/kryon-shader-gpu-accuracy && adb -s 09231JEC206210 shell /data/local/tmp/kryon-shader-gpu-accuracy"
        ;;
esac
