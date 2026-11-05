#!/usr/bin/env python3
"""Cross-build Kryon's static AVIF decoder and run the strict parity test.

Linux AArch64 runs under qemu-user, once as a Cortex-A53 (NEON only) and once
with every CPU feature qemu emulates (dotprod, i8mm), so dav1d's runtime
dispatch takes each assembly tier. With --ndk, static Android arm64-v8a runs
the same way, and Android x86_64/x86 run directly on an x86-64 Linux host
(bionic static executables need only the Linux kernel). qemu proves
correctness, not speed.

--toolchain-root is a prefix holding usr/bin/aarch64-linux-gnu-gcc (the
installed system root /, or Debian's cross packages extracted elsewhere).
"""
import argparse
import os
from pathlib import Path
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from image_packages import image_module_args

CMAKE = '''cmake_minimum_required(VERSION 3.22)
project(KryonAVIFCross C CXX)
include("${KRYON_DIR}/cmake/AVIF.cmake")
kryon_avif_decoder("${AVIF_DIR}" "${AOM_DIR}")
file(GLOB sources "${GENERATED}/*.c")
add_executable(avif-parity ${sources})
target_include_directories(avif-parity PRIVATE "${GENERATED}" "${ZIRAN_INCLUDE}")
target_compile_options(avif-parity PRIVATE -O2 -Wno-main)
target_link_options(avif-parity PRIVATE -static)
target_link_libraries(avif-parity PRIVATE ziran_avif::static m)
'''
TOOLCHAIN = '''set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER "{bin}/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "{bin}/aarch64-linux-gnu-g++")
set(CMAKE_AR "{bin}/aarch64-linux-gnu-ar")
set(CMAKE_RANLIB "{bin}/aarch64-linux-gnu-ranlib")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
{sysroot}'''
# dav1d AArch64 assembly symbol suffixes libavif.a must contain.
TIERS = ('_neon', '_neon_dotprod', '_neon_i8mm')


def run(command, env):
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{command} exited {result.returncode}\n{result.stdout[-5000:]}')
    return result.stdout


def build(work, env, packages, defines, cmake_args, label):
    generated = work/'generated'
    run([ZIRAN, 'build', *image_module_args(), '--target=c', *defines, '--root', ROOT/'tests',
         '--module-path', ROOT/'src/backend', '--module-path', ZIRAN_ROOT/'std', '-o', generated,
         ROOT/'tests/avif_parity_test.zi'], env)
    (work/'cmake').mkdir(parents=True, exist_ok=True)
    (work/'cmake/CMakeLists.txt').write_text(CMAKE)
    run(['cmake', '-S', work/'cmake', '-B', work/'build', '-DCMAKE_BUILD_TYPE=Release', f'-DKRYON_DIR={ROOT}',
         f'-DAVIF_DIR={packages["libavif"]}', f'-DAOM_DIR={packages["libaom"]}', f'-DDAV1D_DIR={packages["dav1d"]}',
         f'-DGENERATED={generated}', f'-DZIRAN_INCLUDE={ZIRAN_ROOT}/include', *cmake_args], env)
    run(['cmake', '--build', work/'build', '--target', 'avif-parity', '--parallel', '2'], env)
    print(f'{label}: static AVIF parity test built', flush=True)
    return work/'build/avif-parity'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain-root', type=Path, default=Path('/'))
    parser.add_argument('--qemu', type=Path)
    parser.add_argument('--ndk', type=Path)
    args = parser.parse_args()
    bin = args.toolchain_root/'usr/bin'
    qemu = args.qemu or bin/'qemu-aarch64'
    fixtures = ROOT/'tests/fixtures/avif'
    env = {k: v for k, v in os.environ.items()
           if k not in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER')}
    # Extracted cross binutils keep their shared libraries beside the prefix.
    libraries = args.toolchain_root/'usr/lib/x86_64-linux-gnu'
    if args.toolchain_root != Path('/') and libraries.is_dir():
        env['LD_LIBRARY_PATH'] = ':'.join(filter(None, (str(libraries), env.get('LD_LIBRARY_PATH'))))
    packages = {name: run([ZIRAN, 'pkg', 'path', name], env).strip() for name in ('libavif', 'libaom', 'dav1d')}
    work = ROOT/'build/scratch/avif-cross/linux-aarch64'
    work.mkdir(parents=True, exist_ok=True)
    # An extracted root is the sysroot; the recipe forwards it to dav1d's Meson.
    sysroot = f'set(CMAKE_SYSROOT "{args.toolchain_root}")\n' if args.toolchain_root != Path('/') else ''
    (work/'toolchain.cmake').write_text(TOOLCHAIN.format(bin=bin, sysroot=sysroot))
    program = build(work, env, packages, ['--define', '__aarch64__'],
                    [f'-DCMAKE_TOOLCHAIN_FILE={work}/toolchain.cmake'], 'Linux AArch64')
    names = [line.split()[-1] for line in run([bin/'aarch64-linux-gnu-nm', '--defined-only', work/'build/avif/libavif.a'], env).splitlines()
             if line[:1] in '0123456789abcdef' and line[:1] and 'dav1d_' in line]
    counts = {tier[1:]: sum(1 for name in names if tier in name) for tier in TIERS}
    assert all(counts.values()), f'dav1d AArch64 assembly missing {counts}'
    tiers = ', '.join(f'{tier} {count}' for tier, count in counts.items())
    cpus = (('cortex-a53', 'Cortex-A53 NEON'), ('max', 'all emulated features (dotprod, i8mm)'))
    for cpu, label in cpus:
        print(run([qemu, '-cpu', cpu, program, fixtures], env).strip(), flush=True)
        print(f'Linux AArch64 qemu-user {label}: dav1d ({tiers}) and libaom strict parity passed', flush=True)
    if not args.ndk:
        return
    for abi, define in (('arm64-v8a', '__aarch64__'), ('x86_64', '__x86_64__'), ('x86', '__i386__')):
        program = build(ROOT/f'build/scratch/avif-cross/android-{abi}', env, packages,
                        ['--define', 'ANDROID_BUILD', '--define', define],
                        [f'-DCMAKE_TOOLCHAIN_FILE={args.ndk}/build/cmake/android.toolchain.cmake',
                         f'-DANDROID_ABI={abi}', '-DANDROID_PLATFORM=android-21'], f'Android {abi}')
        # 32-bit bionic mutexes need pids <= 65535: run x86 in a private pid namespace.
        native = ['unshare', '--user', '--map-root-user', '--pid', '--fork'] if abi == 'x86' else []
        runs = [([qemu, '-cpu', cpu], f'qemu-user {label}') for cpu, label in cpus] if abi == 'arm64-v8a' else [(native, 'native on this x86-64 host')]
        for prefix, label in runs:
            print(run([*prefix, program, fixtures], env).strip(), flush=True)
            print(f'Android {abi} static, {label}: dav1d and libaom strict parity passed', flush=True)


if __name__ == '__main__':
    main()
