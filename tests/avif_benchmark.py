#!/usr/bin/env python3
"""AVIF engine benchmark receipt: ms/image, MP/s and peak RSS per real target.

Builds tests/avif_benchmark.zi against Kryon's own static decoder recipe
(cmake/AVIF.cmake) for the host and, with --serial, ARMv7 Android for the USB
Moto. Each configuration runs in a fresh process: the libaom oracle, dav1d at
1/2/4/8 threads and the automatic bounded count, and on x86-64 each dav1d SIMD
tier through dav1d_set_cpu_flags_mask. Optional --browser adds headless
Firefox's own AVIF decoder. Writes JSON and a Markdown table.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from image_packages import image_module_args

CORPUS = (('ui-icon-444', 200), ('photo-444', 30), ('jungle-clownfish', 20),
          ('jungle-title', 15), ('photo-420-large', 5))
# dav1d x86 flags: SSE2 1, SSSE3 2, SSE4.1 4, AVX2 8, AVX-512 (Ice Lake) 16.
X86_TIERS = (('c+sse2', 1), ('ssse3', 3), ('sse4.1', 7), ('avx2', 15), ('avx512icl', 31))
CMAKE = '''cmake_minimum_required(VERSION 3.22)
project(KryonAVIFBenchmark C CXX)
include("${KRYON_DIR}/cmake/AVIF.cmake")
kryon_avif_decoder("${AVIF_DIR}" "${AOM_DIR}")
file(GLOB sources "${GENERATED}/*.c")
add_executable(avif-benchmark ${sources})
target_include_directories(avif-benchmark PRIVATE "${GENERATED}" "${ZIRAN_INCLUDE}")
target_compile_options(avif-benchmark PRIVATE -O2 -Wno-main)
target_link_libraries(avif-benchmark PRIVATE ziran_avif::static m)
if(ANDROID)
  target_link_libraries(avif-benchmark PRIVATE log)
endif()
'''


def run(command, env=None, cwd=ROOT):
    result = subprocess.run(list(map(str, command)), cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{command} exited {result.returncode}\n{result.stdout[-5000:]}')
    return result.stdout


def build(work, env, packages, defines, cmake_args):
    generated = work/'generated'
    run([ZIRAN, 'build', *image_module_args(), '--target=c', *defines, '--root', ROOT/'tests',
         '--module-path', ROOT/'src/backend', '--module-path', ZIRAN_ROOT/'std', '-o', generated,
         ROOT/'tests/avif_benchmark.zi'], env)
    (work/'cmake').mkdir(parents=True, exist_ok=True)
    (work/'cmake/CMakeLists.txt').write_text(CMAKE)
    run(['cmake', '-S', work/'cmake', '-B', work/'build', '-DCMAKE_BUILD_TYPE=Release', f'-DKRYON_DIR={ROOT}',
         f'-DAVIF_DIR={packages["libavif"]}', f'-DAOM_DIR={packages["libaom"]}', f'-DDAV1D_DIR={packages["dav1d"]}',
         f'-DGENERATED={generated}', f'-DZIRAN_INCLUDE={ZIRAN_ROOT}/include', *cmake_args], env)
    run(['cmake', '--build', work/'build', '--target', 'avif-benchmark', '--parallel', '2'], env)
    return work/'build/avif-benchmark'


def configurations(x86):
    yield 'libaom', 1, -1, 'libaom'
    for threads in (1, 2, 4, 8, 0):
        yield 'dav1d', threads, -1, 'dav1d'
    if x86:
        for tier, mask in X86_TIERS:
            yield 'dav1d', 1, mask, f'dav1d {tier}'


def measure(target, invoke, x86, fixtures):
    rows = []
    for name, iterations in CORPUS:
        for engine, threads, mask, label in configurations(x86):
            line = invoke([f'{fixtures}/{name}.avif', engine, threads, mask, iterations]).strip().splitlines()[-1]
            row = json.loads(line)
            row.update(target=target, label=label, name=name,
                       ms=row['p50_ns'] / 1e6, mp_per_s=row['width'] * row['height'] / (row['p50_ns'] / 1e3))
            rows.append(row)
            print(f'{target} {name} {label} t{threads}->{row["effective_threads"]}: {row["ms"]:.2f} ms '
                  f'{row["mp_per_s"]:.1f} MP/s {row["peak_rss_kib"] / 1024:.1f} MiB', flush=True)
        digests = {row['digest'] for row in rows if row['name'] == name and row['target'] == target}
        assert len(digests) == 1, f'{target} {name}: engines/threads/tiers disagree {digests}'
    return rows


def browser(rows_out, work):
    # Firefox's own decoder; private headless profile, display environment scrubbed.
    import avif_firefox_test
    for row in avif_firefox_test.benchmark([(name, ROOT/f'tests/fixtures/avif/{name}.avif', iterations)
                                            for name, iterations in CORPUS], work):
        row.update(target='firefox-headless', label='browser', mp_per_s=row['width'] * row['height'] / (row['ms'] * 1e3))
        rows_out.append(row)
        print(f'firefox {row["name"]}: {row["ms"]:.2f} ms {row["mp_per_s"]:.1f} MP/s', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path)
    parser.add_argument('--serial')
    parser.add_argument('--browser', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT/'build/scratch/avif-benchmark/receipt.json')
    args = parser.parse_args()
    if args.serial and args.serial != 'ZE2223BQZT':
        parser.error('Android benchmarks are reserved for the USB Moto ZE2223BQZT')
    env = {k: v for k, v in os.environ.items()
           if k not in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER')}
    packages = {name: run([ZIRAN, 'pkg', 'path', name]).strip() for name in ('libavif', 'libaom', 'dav1d')}
    work = args.output.parent
    work.mkdir(parents=True, exist_ok=True)
    rows = []
    host = build(work/'host', env, packages, [], [])
    x86 = platform.machine() in ('x86_64', 'AMD64')
    flags = set(next((line.split(':', 1)[1].split() for line in open('/proc/cpuinfo') if line.startswith('flags')), []))
    # Only tiers this CPU really has: a mask can clear flags, never add them.
    global X86_TIERS
    present = {'ssse3': 'ssse3' in flags, 'sse4.1': 'sse4_1' in flags, 'avx2': 'avx2' in flags,
               'avx512icl': {'avx512vl', 'avx512bw', 'avx512_vbmi2', 'avx512_vnni', 'avx512_bitalg', 'gfni', 'vpclmulqdq'} <= flags}
    X86_TIERS = tuple(tier for tier in X86_TIERS if present.get(tier[0], True))
    model = next((line.split(':', 1)[1].strip() for line in open('/proc/cpuinfo') if line.startswith('model name')), platform.machine())
    target = f'linux-{platform.machine()} ({model}, {len(os.sched_getaffinity(0))} usable cores)'
    rows += measure(target, lambda argv: run([host, *argv], env), x86, ROOT/'tests/fixtures/avif')
    if args.serial:
        if not args.ndk:
            parser.error('--serial needs --ndk')
        android = build(work/'armeabi-v7a', env, packages, ['--define', 'ANDROID_BUILD', '--define', '__arm__'],
                        [f'-DCMAKE_TOOLCHAIN_FILE={args.ndk}/build/cmake/android.toolchain.cmake',
                         '-DANDROID_ABI=armeabi-v7a', '-DANDROID_PLATFORM=android-21'])
        adb = ['adb', '-s', args.serial]
        assert run(adb + ['get-state']).strip() == 'device'
        remote = '/data/local/tmp/kryon-avif-benchmark'
        run(adb + ['shell', 'mkdir', '-p', remote + '/fixtures'])
        run(adb + ['push', android, remote + '/avif-benchmark'])
        run(adb + ['push', *[ROOT/f'tests/fixtures/avif/{name}.avif' for name, _ in CORPUS], remote + '/fixtures/'])
        run(adb + ['shell', 'chmod', '700', remote + '/avif-benchmark'])
        cores = run(adb + ['shell', 'cat', '/sys/devices/system/cpu/online']).strip()
        hardware = run(adb + ['shell', 'getprop', 'ro.board.platform']).strip()
        model = run(adb + ['shell', 'getprop', 'ro.product.model']).strip()
        target = f'android-armeabi-v7a ({model}, {hardware}, cpus {cores})'
        rows += measure(target, lambda argv: run(adb + ['shell', remote + '/avif-benchmark', *map(str, argv)]),
                        False, remote + '/fixtures')
    if args.browser:
        browser(rows, work)
    args.output.write_text(json.dumps(rows, indent=1) + '\n')
    table = ['| target | image | decoder | threads | ms/image | MP/s | peak RSS MiB |', '|---|---|---|---|---|---|---|']
    for row in rows:
        rss = f'{row["peak_rss_kib"] / 1024:.1f}' if row.get('peak_rss_kib', -1) >= 0 else 'n/a'
        threads = f'{row["threads"]}→{row["effective_threads"]}' if 'threads' in row else 'browser'
        table.append(f'| {row["target"]} | {row["name"]} | {row["label"]} | {threads} | {row["ms"]:.2f} | {row["mp_per_s"]:.1f} | {rss} |')
    args.output.with_suffix('.md').write_text('\n'.join(table) + '\n')
    print(f'AVIF benchmark receipt: {args.output} and {args.output.with_suffix(".md")}')


if __name__ == '__main__':
    sys.exit(main())
