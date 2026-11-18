#!/usr/bin/env python3
"""Link real scene GPU code as API21 NativeActivity libraries through Kryon's
shared CMake helper. Invoke with build-queue --; no device or desktop launch.
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
    for abi, cpu in (('armeabi-v7a', '__arm__'), ('arm64-v8a', '__aarch64__')):
        project = output / abi
        generated = project / 'c'
        generated.mkdir(parents=True, exist_ok=True)
        for pattern in ('*.c', '*.h'):
            for old in generated.rglob(pattern):
                old.unlink()
        run([ZIRAN, 'build', '--project', '--root', ROOT / 'tests', '--target=c',
             '--target-os', 'android', '--target-cpu', 'none',
             '--no-main', '--entry', 'shader_android_link_test:android_main',
             '--define', 'ANDROID_BUILD', '--define', cpu, '-o', generated,
             ROOT / 'tests/shader_android_link_test.zi'])
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
        print(abi + ': API21 scene GPU NativeActivity link passed', flush=True)


if __name__ == '__main__':
    main()
