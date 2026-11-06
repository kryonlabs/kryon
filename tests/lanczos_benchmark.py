#!/usr/bin/env python3
"""Lanczos-3 resizes: Kryon's std/resample, as the shared cache runs it,
against Pillow, Pillow-SIMD, libvips and stb_image_resize2, on x86-64 and on
the ARMv7 Moto (ZE2223BQZT) over ADB. Comparison libraries are test-only
oracles.

Each tool runs in its own process; on x86-64 single-threaded runs are pinned
to one core and four-thread runs to four (LANCZOS_BENCH_CPU, LANCZOS_BENCH_CPUS:
4 and 2-5, the build cores). ms/Mpx is the median time per output
megapixel; on a shared machine the fastest of the samples is the steadier
figure. Extra MiB is the peak resident memory (VmHWM) above the same
process with the picture loaded and no resize. PSNR is against a float64
Lanczos-3 with Pillow's sampling (pixel centers, a 3 * scale support when
shrinking), clipped to bytes; every picture is opaque.

    lanczos_benchmark.py [--quick] [--no-phone]

Environment: ORACLE_PYTHON (a Python with Pillow-SIMD and pyvips), STB_DIR
(holding stb_image_resize2.h), ANDROID_NDK_HOME, ADB_SERIAL. Writes
build/scratch/lanczos-bench/receipt.json and receipt.md.
"""
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image

from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/lanczos-bench'
ORACLE = ROOT / 'tests/lanczos_benchmark_oracle.py'
PHONE_DIR = '/data/local/tmp/kryon-lanczos-bench'
SERIAL = os.environ.get('ADB_SERIAL', 'ZE2223BQZT')
QUICK = '--quick' in sys.argv


def run(command, env=None, **options):
    return subprocess.run(list(map(str, command)), env=env, check=True, **options)


def photo(width, height, seed):
    """A deterministic photo-like picture: soft shading, texture, noise and
    hard edges that ring."""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:height, 0:width].astype(np.float64)
    picture = np.empty((height, width, 3))
    for channel in range(3):
        picture[..., channel] = (128 + 70 * np.sin(x / (53.0 + 11 * channel)) * np.cos(y / 41.0)
                                 + 25 * np.sin((x * 0.7 + y) / (4.1 + channel)))
    picture += rng.normal(0, 9, picture.shape)
    stripe = ((x // 7 + y // 7) % 2 == 0) & (x > width * 0.6) & (y < height * 0.3)
    picture[stripe] = 250
    picture[(x // 3) % 41 == 0] = 10
    return np.clip(picture + 0.5, 0, 255).astype(np.uint8)


def jungle():
    path = ROOT / 'tests/fixtures/avif/jungle-clownfish.avif'
    try:
        return np.asarray(Image.open(path).convert('RGB'))
    except Exception as error:  # Pillow without AVIF: the case is skipped.
        print(f'skipping the Jungle case: {error}')
        return None


def weights(size, out):
    """Pillow's Lanczos-3 coefficients in float64, one row per output."""
    scale = size / out
    filter_scale = max(scale, 1.0)
    support = 3.0 * filter_scale
    matrix = np.zeros((out, size))
    for i in range(out):
        center = (i + 0.5) * scale
        low = max(int(center - support + 0.5), 0)
        high = min(int(center + support + 0.5), size)
        taps = (np.arange(low, high) - center + 0.5) / filter_scale
        kernel = np.where(np.abs(taps) < 3.0, np.sinc(taps) * np.sinc(taps / 3.0), 0.0)
        matrix[i, low:high] = kernel / kernel.sum()
    return matrix


def reference(picture, out_width, out_height):
    height, width = picture.shape[:2]
    horizontal = weights(width, out_width)
    vertical = weights(height, out_height)
    result = np.empty((out_height, out_width, 3))
    for channel in range(3):
        result[..., channel] = vertical @ (picture[..., channel].astype(np.float64) @ horizontal.T)
    return np.clip(result, 0.0, 255.0)


def psnr(path, expected):
    actual = np.frombuffer(Path(path).read_bytes(), np.uint8).reshape(expected.shape[0], expected.shape[1], 4)
    error = np.mean((actual[..., :3].astype(np.float64) - expected) ** 2)
    return float('inf') if error == 0 else 10 * np.log10(255.0 ** 2 / error)


def build_native():
    generated = WORK / 'c'
    shutil.rmtree(generated, ignore_errors=True)
    run([ZIRAN, 'build', '--target=c', '--define', 'LANCZOS_BENCH_STB', '--entry', 'lanczos_benchmark:main',
         '--root', ROOT / 'tests', '--module-path', f'std={ZIRAN_ROOT}/std', '--module-path', ZIRAN_ROOT / 'std',
         '-o', generated, ROOT / 'tests/lanczos_benchmark.zi'])
    sources = sorted(generated.glob('*.c'))
    cc = os.environ.get('CC', 'cc')
    # stb_image_resize2's implementation, from its header compiled as C.
    stb = Path(os.environ.get('STB_DIR', WORK / 'oracles/stb')) / 'stb_image_resize2.h'
    run([cc, '-O3', '-march=x86-64-v3', '-DSTB_IMAGE_RESIZE_IMPLEMENTATION', '-x', 'c', '-c', stb,
         '-o', WORK / 'stb-native.o'])
    run([cc, '-std=c99', '-O2', f'-I{ZIRAN_ROOT}/include', f'-I{generated}', *sources, WORK / 'stb-native.o',
         '-lm', '-lpthread', '-o', WORK / 'ziran-native'])
    ndk = os.environ.get('ANDROID_NDK_HOME')
    if ndk:
        # Defined as Kryon's armeabi-v7a builds are: ANDROID_BUILD picks
        # bionic's 64-bit file offsets.
        android = WORK / 'c-android'
        shutil.rmtree(android, ignore_errors=True)
        run([ZIRAN, 'build', '--target=c', '--define', 'ANDROID_BUILD', '--define', '__arm__',
             '--define', 'LANCZOS_BENCH_STB', '--entry', 'lanczos_benchmark:main',
             '--root', ROOT / 'tests', '--module-path', f'std={ZIRAN_ROOT}/std', '--module-path', ZIRAN_ROOT / 'std',
             '-o', android, ROOT / 'tests/lanczos_benchmark.zi'])
        clang = Path(ndk) / 'toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi24-clang'
        run([clang, '-O3', '-DSTB_IMAGE_RESIZE_IMPLEMENTATION', '-x', 'c', '-c', stb, '-o', WORK / 'stb-armv7.o'])
        run([clang, '-std=c99', '-O2', '-static', '-Wno-parentheses-equality', f'-I{ZIRAN_ROOT}/include',
             f'-I{android}', *sorted(android.glob('*.c')), WORK / 'stb-armv7.o', '-lm', '-o', WORK / 'ziran-armv7'])


def parse(text):
    name, median, fastest, peak = text.split()
    return name, int(median), int(fastest), int(peak)


def measure(label, threads, command, size, out_path, iterations, expected, env=None, phone=False):
    """Runs COMMAND with no resize and then ITERATIONS resizes; the
    {iterations} and {out} fields are filled in."""
    def once(count):
        filled = [str(part).format(iterations=count, out=out_path) for part in command]
        if phone:
            prefix = ' '.join(f'{key}={value}' for key, value in (env or {}).items())
            shell = f'cd {PHONE_DIR} && {prefix} ' + ' '.join(filled)
            text = subprocess.run(['adb', '-s', SERIAL, 'shell', shell], check=True,
                                  capture_output=True, text=True).stdout
        else:
            environment = dict(os.environ, **(env or {}))
            cpus = os.environ.get('LANCZOS_BENCH_CPU', '4') if threads == 1 else os.environ.get('LANCZOS_BENCH_CPUS', '2-5')
            text = subprocess.run(['taskset', '-c', cpus, *filled], check=True, capture_output=True, text=True,
                                  env=environment).stdout
        return parse(text.strip().splitlines()[-1])
    _, _, _, idle = once(0)
    name, median, fastest, peak = once(iterations)
    local = out_path
    if phone:
        local = WORK / ('phone-' + Path(out_path).name)
        run(['adb', '-s', SERIAL, 'pull', f'{PHONE_DIR}/{out_path}', local], capture_output=True)
    out_width, out_height = size
    mpx = out_width * out_height / 1e6
    return {'library': label, 'reported': name, 'threads': threads, 'ms': median / 1000.0,
            'fastest_ms': fastest / 1000.0, 'ms_per_mpx': median / 1000.0 / mpx,
            'extra_mib': (peak - idle) / 1024.0, 'peak_mib': peak / 1024.0,
            'psnr_db': psnr(local, expected)}


def native_tools(oracle_python):
    ziran = WORK / 'ziran-native'
    case = ['{in}', '{w}', '{h}', '{ow}', '{oh}', '{iterations}']
    tools = []
    for level in ('scalar', 'sse2', 'avx2'):
        tools.append((f'Kryon std/resample {level}', 1, [ziran, *case, level, 1, '{out}'], None))
    tools.append(('Kryon std/resample best', 4, [ziran, *case, 'best', 4, '{out}'], {'ZIRAN_PAR_THREADS': '4'}))
    tools.append(('Pillow', 1, [sys.executable, ORACLE, 'pillow', *case, '{out}'], None))
    if oracle_python:
        tools.append(('Pillow-SIMD', 1, [oracle_python, ORACLE, 'pillow', *case, '{out}'], None))
        tools.append(('libvips', 1, [oracle_python, ORACLE, 'vips', *case, '{out}'], {'VIPS_CONCURRENCY': '1'}))
        tools.append(('libvips', 4, [oracle_python, ORACLE, 'vips', *case, '{out}'], {'VIPS_CONCURRENCY': '4'}))
    tools.append(('stb_image_resize2 (AVX2 build)', 1, [ziran, *case, 'stb', 1, '{out}'], None))
    return tools


def phone_tools():
    case = ['{in}', '{w}', '{h}', '{ow}', '{oh}', '{iterations}']
    return [('Kryon std/resample scalar', 1, ['./ziran-armv7', *case, 'scalar', 1, '{out}'], None),
            ('Kryon std/resample neon', 1, ['./ziran-armv7', *case, 'neon', 1, '{out}'], None),
            ('Kryon std/resample best', 4, ['./ziran-armv7', *case, 'best', 4, '{out}'], {'ZIRAN_PAR_THREADS': '4'}),
            ('stb_image_resize2 (NEON build)', 1, ['./ziran-armv7', *case, 'stb', 1, '{out}'], None)]


def fill(command, picture_path, picture, size):
    height, width = picture.shape[:2]
    values = {'in': picture_path, 'w': width, 'h': height, 'ow': size[0], 'oh': size[1]}
    return [part.replace('{in}', str(values['in'])).replace('{w}', str(values['w'])).replace('{h}', str(values['h']))
            .replace('{ow}', str(values['ow'])).replace('{oh}', str(values['oh'])) if isinstance(part, str) else part
            for part in command]


def table(rows):
    lines = ['| Platform | Case | Library | Threads | Median ms | Fastest ms | ms/Mpx | Extra MiB | Peak MiB | PSNR dB |',
             '|---|---|---|---:|---:|---:|---:|---:|---:|---:|']
    for row in rows:
        lines.append(f"| {row['platform']} | {row['case']} | {row['library']} | {row['threads']} | {row['ms']:.2f} | "
                     f"{row['fastest_ms']:.2f} | {row['ms_per_mpx']:.2f} | {row['extra_mib']:.1f} | {row['peak_mib']:.1f} | "
                     f"{row['psnr_db']:.2f} |")
    return '\n'.join(lines)


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    build_native()
    cases = []
    fish = jungle()
    if fish is not None:
        side = fish.shape[1] * 4 // 3
        cases.append((f'Jungle {fish.shape[1]}x{fish.shape[0]} to {side}x{fish.shape[0] * 4 // 3}', 'jungle', fish,
                      (side, fish.shape[0] * 4 // 3)))
    cases.append(('photo 1440x810 to 1920x1080', 'up', photo(1440, 810, 1), (1920, 1080)))
    if not QUICK:
        cases.append(('photo 4000x3000 to 800x600', 'down', photo(4000, 3000, 2), (800, 600)))
    oracle_python = os.environ.get('ORACLE_PYTHON')
    phone = '--no-phone' not in sys.argv and (WORK / 'ziran-armv7').exists() and SERIAL in subprocess.run(
        ['adb', 'devices'], capture_output=True, text=True).stdout
    if phone:
        run(['adb', '-s', SERIAL, 'shell', f'mkdir -p {PHONE_DIR}'])
        run(['adb', '-s', SERIAL, 'push', WORK / 'ziran-armv7', f'{PHONE_DIR}/ziran-armv7'], capture_output=True)
        run(['adb', '-s', SERIAL, 'shell', f'chmod 755 {PHONE_DIR}/ziran-armv7'])
    rows = []
    for title, key, picture, size in cases:
        rgba = np.dstack([picture, np.full(picture.shape[:2], 255, np.uint8)])
        source = WORK / f'{key}.rgba'
        source.write_bytes(rgba.tobytes())
        expected = reference(picture, *size)
        iterations = 3 if QUICK else 15
        for label, threads, command, env in native_tools(oracle_python):
            out = WORK / f'{key}-{len(rows)}.out'
            row = measure(label, threads, fill(command, source, picture, size), size, out, iterations, expected, env)
            row.update(platform=f'x86-64 {platform.processor() or platform.machine()}', case=title)
            rows.append(row)
            print(json.dumps(row), flush=True)
        if phone:
            run(['adb', '-s', SERIAL, 'push', source, f'{PHONE_DIR}/{key}.rgba'], capture_output=True)
            for label, threads, command, env in phone_tools():
                out = f'{key}-{len(rows)}.out'
                # The phone's first resize runs while its clock ramps up.
                row = measure(label, threads, fill(command, f'{key}.rgba', picture, size), size, out, 5, expected,
                              env, phone=True)
                row.update(platform='ARMv7 Moto e6 play (ZE2223BQZT)', case=title)
                rows.append(row)
                print(json.dumps(row), flush=True)
            run(['adb', '-s', SERIAL, 'shell', f'rm -f {PHONE_DIR}/{key}.rgba {PHONE_DIR}/*.out'])
    if phone:
        run(['adb', '-s', SERIAL, 'shell', f'rm -rf {PHONE_DIR}'])
    cpu = next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                if line.startswith('model name')), platform.machine())
    receipt = {'cpu': cpu, 'rows': rows}
    (WORK / 'receipt.json').write_text(json.dumps(receipt, indent=1) + '\n')
    (WORK / 'receipt.md').write_text(f'x86-64 CPU: {cpu}\n\n' + table(rows) + '\n')
    print(table(rows))
    # Kryon's bytes are Pillow's: the same PSNR, to rounding.
    for title in {row['case'] for row in rows}:
        kryon = [row['psnr_db'] for row in rows if row['case'] == title and row['library'].startswith('Kryon')]
        pillow = [row['psnr_db'] for row in rows if row['case'] == title and row['library'] == 'Pillow']
        if pillow and any(abs(value - pillow[0]) > 0.01 for value in kryon):
            sys.exit(f'{title}: Kryon PSNR {kryon} differs from Pillow {pillow[0]:.2f}')
    print(f'receipt: {WORK / "receipt.md"}')


if __name__ == '__main__':
    main()
