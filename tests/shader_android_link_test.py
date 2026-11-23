#!/usr/bin/env python3
"""Generate scene GPU code with the Android C99 pipeline and link API21
NativeActivity libraries through Kryon's shared CMake helper.
"""
import os
from pathlib import Path
import subprocess

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT


def run(arguments):
    subprocess.run(list(map(str, arguments)), cwd=ROOT, check=True)


def main():
    ndk = Path(os.environ.get('ANDROID_NDK', Path.home() / 'Android/Sdk/ndk/28.2.13676358'))
    output = ROOT / 'build/scratch/shader-android-link'
    for abi, arch, cpu in (('armeabi-v7a', '__arm__', 'none'),
                           ('arm64-v8a', '__aarch64__', 'arm64')):
        project = output / abi
        generated = project / 'c'
        ir = project / 'ir'
        generated.mkdir(parents=True, exist_ok=True)
        ir.mkdir(parents=True, exist_ok=True)
        for old in ir.glob('*.zir'):
            old.unlink()
        for pattern in ('*.c', '*.h'):
            for old in generated.rglob(pattern):
                old.unlink()
        target = ['--target-os', 'android', '--target-cpu', cpu]
        defines = ['--define', 'KRYON_ANDROID_HOST', '--define', 'ANDROID_BUILD',
                   '--define', 'POSIX_THREADS', '--define', arch]
        entry = ['--entry', 'shader_android_link_test:android_main']
        # The app generator checks source in IR mode and then feeds the saved
        # graph to zi2c. A single `ziran build --target=c` folds guards earlier.
        run([ZIRAN, 'ir', '--project', *entry, *target, '--root', ROOT / 'tests',
             *defines, '-o', ir,
             ROOT / 'tests/shader_android_link_test.zi'])
        run([ZIRAN.with_name('zi2c'), '--no-main', *target, *defines, '--root', ir,
             '-o', generated, *sorted(ir.glob('*.zir'))])
        scene_render, = generated.rglob('*scene_render.c')
        with scene_render.open() as source:
            if not any('std_resample_ResamplePlanRun(' in line for line in source):
                raise AssertionError(abi + ' Android C99 resize omitted the native Lanczos resampler')
        sources = '\n'.join('"' + str(p) + '"' for p in sorted(generated.rglob('*.c')))
        headers = '\n'.join('"' + str(p) + '"' for p in sorted({p.parent for p in generated.rglob('*.h')}))
        (project / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.22)
project(ShaderAndroidLink C)
set(ZI2C_BIN "{ZIRAN_ROOT}/build/bin/zi2c")
include("{ROOT}/cmake/KryonAndroid.cmake")
add_library(main SHARED {sources})
target_include_directories(main PRIVATE "{ZIRAN_ROOT}/include")
foreach(header_dir {headers})
    target_compile_options(main PRIVATE "-iquote${{header_dir}}")
endforeach()
target_link_options(main PRIVATE -Wl,--no-undefined)
kryon_android_app(main)
''')
        run(['cmake', '-S', project, '-B', project / 'link', '-G', 'Ninja',
             '-DCMAKE_TOOLCHAIN_FILE=' + str(ndk / 'build/cmake/android.toolchain.cmake'),
             '-DANDROID_ABI=' + abi, '-DANDROID_PLATFORM=android-21'])
        run(['cmake', '--build', project / 'link', '--parallel', '1'])
        symbols = subprocess.check_output([
            str(ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm'),
            '-D', str(project / 'link/libmain.so')], text=True)
        if 'glX' in symbols or 'eglGetPlatformDisplay' in symbols:
            raise AssertionError(abi + ' retained a desktop or newer EGL import')
        for required in ('eglGetDisplay', 'eglMakeCurrent', 'ANativeActivity_onCreate'):
            if required not in symbols:
                raise AssertionError(abi + ' missing ' + required)
        print(abi + ': Android C99 native resize generation and API21 NativeActivity link passed', flush=True)


if __name__ == '__main__':
    main()
