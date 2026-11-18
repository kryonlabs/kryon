#!/usr/bin/env python3
"""Build on the development host; measure desktop on the designated benchmark host.

The caller owns a build-queue slot. --self-test is correctness only; its tiny
frames are never published as benchmark results. Raw records are streamed to disk.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shlex
import socket
import statistics
import subprocess

ROOT = Path(os.environ.get('KRYON_SHADER_BENCH_ROOT', Path(__file__).resolve().parents[1])).resolve()
OUT = ROOT / 'build/scratch/shader-benchmark'
NAMES = ('uv', 'plasma', 'value-noise-fbm', 'sdf-raymarch', 'gaussian-blur', 'crt', 'ripple', 'particles')
VISUAL_NAMES = ('lit-meshes', 'skinned-character', 'textured-room', 'particle-field')
MODES = {'interpreter': 0, 'scene-cpu': 1, 'scene-gpu': 2}
DEVICES = {'moto': ('ZE2223BQZT', None, 'armv7a-linux-androideabi21'),
           'pixel': ('09231JEC206210', 'thinkpad.local', 'aarch64-linux-android21')}


def run(args, **kwargs):
    return subprocess.run(list(map(str, args)), cwd=ROOT, check=True, **kwargs)


def binary(device):
    return OUT / device / ('shader_benchmark.js' if device == 'browser' else 'shader_benchmark.exe' if device == 'wine' else 'shader_benchmark')


def build(device, prepared=False):
    from toolchain import ZIRAN, ZIRAN_ROOT
    directory = binary(device).parent
    directory.mkdir(parents=True, exist_ok=True)
    generated = directory / 'c'
    # Remove stale generated units, preserving the stable executable until link.
    if generated.exists():
        for pattern in ('*.c', '*.h'):
            for old in generated.rglob(pattern):
                old.unlink()
    defines = [] if device in ('browser', 'wine') else ['--define', 'POSIX_THREADS']
    if device == 'browser':
        defines += ['--target-os', 'web', '--target-cpu', 'wasm32', '--define', 'PLATFORM_WEB']
    if prepared:
        defines += ['--define', 'SHADER_BENCH_PREPARED']
    if device == 'wine':
        defines += ['--target-os', 'windows', '--target-cpu', 'x64', '--define', '_WIN32']
    if device == 'desktop' or device in DEVICES:
        defines += ['--define', 'SHADER_BENCH_GPU']
    if device in DEVICES:
        defines += ['--target-os', 'android', '--target-cpu', 'none' if device == 'moto' else 'arm64',
                    '--define', 'ANDROID_BUILD', '--define', '__arm__' if device == 'moto' else '__aarch64__']
    run([ZIRAN, 'build', '--project', '--root', ROOT / 'tests', '--target=c',
         '--no-main', *defines, '-o', generated, ROOT / 'tests/shader_benchmark.zi'])
    if device in DEVICES:
        ndk = Path(os.environ.get('ANDROID_NDK', Path.home() / 'Android/Sdk/ndk/28.2.13676358'))
        compiler = [ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin' / (DEVICES[device][2] + '-clang')]
        libraries = ['-lEGL', '-lGLESv3', '-lm', '-pthread']
    elif device == 'wine':
        compiler = shlex.split(os.environ.get('WINDOWS_CC', 'x86_64-w64-mingw32-gcc'))
        libraries = ['-static-libgcc', '-lm', '-lkernel32']
    elif device == 'browser':
        compiler = [os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc'))]
        libraries = ['-lm', '-sALLOW_MEMORY_GROWTH=1', '-sSTACK_SIZE=16777216',
                     '-sENVIRONMENT=web', '-sINVOKE_RUN=0', '-sEXPORTED_RUNTIME_METHODS=callMain',
                     '--js-library', str(ZIRAN_ROOT / 'web/ziran_web.js')]
    else:
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        libraries = ['-lEGL', '-lGLESv2', '-lGLX', '-lX11', '-lm', '-pthread']
    run([*compiler, '-std=c11', '-O2', '-ffunction-sections', '-fdata-sections',
         '-I' + str(ZIRAN_ROOT / 'include'),
         *[arg for folder in sorted({p.parent for p in generated.rglob('*.h')}) for arg in ('-iquote', folder)],
         *sorted(generated.rglob('*.c')), '-Wl,--gc-sections', *libraries, '-o', binary(device)])
    fingerprint = hashlib.sha256()
    for source in sorted(generated.rglob('*.c')):
        fingerprint.update(source.name.encode())
        with source.open('rb') as stream:
            for block in iter(lambda: stream.read(65536), b''):
                fingerprint.update(block)
    with binary(device).open('rb') as stream:
        executable_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    wasm_hash = None
    if device == 'browser':
        with binary(device).with_suffix('.wasm').open('rb') as stream:
            wasm_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    tool_hashes = {}
    for label, path in (('ziran_sha256', Path(ZIRAN)), ('lock_sha256', ROOT / 'ziran.lock')):
        with path.open('rb') as stream:
            tool_hashes[label] = hashlib.file_digest(stream, 'sha256').hexdigest()
    version = subprocess.run([str(compiler[0]), '--version'], check=True, text=True, stdout=subprocess.PIPE).stdout[:4096]
    dirty = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=normal'], cwd=ROOT) != b''
    provenance = {**tool_hashes, 'compiler_version': version, 'wasm_sha256': wasm_hash, 'generated_c_sha256': fingerprint.hexdigest(), 'binary_sha256': executable_hash, 'optimization': '-O2',
                  'source_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  'source_dirty': dirty, 'device': device, 'prepared': prepared, 'compiler': list(map(str, compiler)), 'libraries': libraries}
    binary(device).with_suffix('.build.json').write_text(json.dumps(provenance, indent=2) + '\n')


def command(device, args):
    if device == 'desktop':
        return [binary(device), *map(str, args)]
    if device == 'wine':
        return ['xvfb-run', '-a', '-s', '-screen 0 640x480x24 -nolisten tcp', os.environ.get('WINE', 'wine'), binary(device), *map(str, args)]
    if device == 'browser':
        return [os.environ.get('JS_RUNTIME', 'node'),
                os.environ.get('KRYON_SHADER_BENCH_BROWSER', str(ROOT / 'tests/shader_benchmark_browser.mjs')),
                binary(device), *map(str, args)]
    serial, host, _ = DEVICES[device]
    remote = '/data/local/tmp/kryon-shader-benchmark'
    if host:
        # Disposable executable, no app installation or profile changes.
        destination = 'Projects/kryonlabs/kryon/build/scratch/shader-benchmark/pixel'
        run(['ssh', host, 'mkdir -p ' + shlex.quote(destination)])
        run(['scp', binary(device), host + ':' + destination + '/shader_benchmark'])
        run(['ssh', host, 'adb -s ' + serial + ' push ' + destination + '/shader_benchmark ' + remote])
        run(['ssh', host, 'adb -s ' + serial + ' shell chmod 700 ' + remote])
        return ['ssh', host, 'adb -s ' + serial + ' shell ' + shlex.join([remote, *map(str, args)])]
    run(['adb', '-s', serial, 'push', binary(device), remote])
    run(['adb', '-s', serial, 'shell', 'chmod', '700', remote])
    return ['adb', '-s', serial, 'shell', remote, *map(str, args)]


def parse(path, expected, allow_software=False, allow_zero=False):
    groups, workloads, identities = {}, {}, []
    with path.open() as stream:
        for line in stream:
            fields = line.split()
            if not fields:
                continue
            if fields[0] == 'identity':
                identities.append(line.strip())
                continue
            if fields[0] == 'workload':
                if len(fields) != 9:
                    raise ValueError('Malformed workload record: ' + line[:160])
                mode, width, height, kind, instructions, compile_ns, warmup, frames = map(int, fields[1:])
                key = (mode, width, height, kind)
                if kind < len(NAMES):
                    compiled = 1 <= instructions <= 128 and compile_ns >= 0
                else:
                    compiled = instructions == 0 and compile_ns == 0
                if key in workloads or frames != expected.get(key) or not compiled or warmup < 1:
                    raise ValueError('Invalid or duplicate workload record: ' + line[:160])
                workloads[key] = (instructions, compile_ns, warmup)
                continue
            if fields[0] != 'sample':
                continue
            if len(fields) != 12:
                raise ValueError('Malformed timing record: ' + line[:160])
            mode, width, height, kind, frame, ns = map(int, fields[1:7])
            stages = list(map(float, fields[7:11]))
            elapsed_ms = ns / 1e6
            if (ns < 0 or (ns == 0 and not allow_zero)
                    or any(not math.isfinite(v) or v < 0 or v > elapsed_ms + 0.001 for v in stages)):
                raise ValueError('Invalid frame timing')
            key = (mode, width, height, kind)
            rows = groups.setdefault(key, [])
            if frame != len(rows):
                raise ValueError('Duplicate or missing frame')
            rows.append((elapsed_ms, *stages, int(fields[11])))
    if set(groups) != set(expected) or set(workloads) != set(expected):
        raise ValueError('Missing or unexpected workloads')
    if any(len(groups[key]) != count for key, count in expected.items()):
        raise ValueError('Incomplete sample count')
    gpu_workloads = sum(key[0] == MODES['scene-gpu'] for key in groups)
    if gpu_workloads:
        if len(identities) != gpu_workloads or len(set(identities)) != 1 or not identities[0].startswith('identity true '):
            raise ValueError('GPU identity unavailable')
        if not allow_software and any(word in ' '.join(identities).lower() for word in ('llvmpipe', 'softpipe', 'swrast', 'swiftshader', 'software rasterizer')):
            raise ValueError('Software GL cannot be published as hardware GPU')
    results = []
    for (mode, width, height, kind), rows in sorted(groups.items()):
        values = [r[0] for r in rows]
        instructions, compile_ns, warmup = workloads[mode, width, height, kind]
        item = dict(mode=list(MODES)[mode], resolution=f'{width}x{height}', shader=(NAMES + VISUAL_NAMES)[kind],
                    instructions=instructions, compile_ms=compile_ns / 1e6, warmup_frames=warmup,
                    samples=len(rows), mean_ms=statistics.mean(values), p95_ms=sorted(values)[math.ceil(.95 * len(values)) - 1],
                    stages_mean_ms=dict(zip(('prepare', 'draw', 'readback', 'conversion'),
                                            (statistics.mean(r[col] for r in rows) for col in range(1, 5)))),
                    hashes=[r[-1] for r in rows])
        results.append(item)
    return results, identities


def measure(args):
    if args.device in ('desktop', 'browser', 'wine') and not args.self_test and socket.gethostname().split('.')[0] != 'alpha':
        raise RuntimeError('Desktop benchmark measurements must run on the designated physical benchmark host')
    if args.device not in ('desktop', *DEVICES) and args.mode == 'scene-gpu':
        raise RuntimeError('scene_gpu benchmark requires the native EGL/GLES3 backend')
    env = os.environ.copy()
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        env.pop(name, None)
    env.update(HARMONY_DESKTOP_RECOVERY='0', YUE_DESKTOP_RECOVERY='0', EGL_PLATFORM='surfaceless')
    if args.device == 'wine':
        env.update(WINEPREFIX=str(OUT / 'wine/prefix'), WINEDEBUG='-all')
        env.setdefault('WINEDLLOVERRIDES', 'winemenubuilder.exe=d;mscoree=d;mshtml=d')
    visual = args.suite == 'visual'
    tiny = (64, 36) if visual else (8, 8)
    resolutions = [tiny] if args.self_test else [(640, 360)] if args.probe else [(640, 360), (1920, 1080)]
    first = len(NAMES) if visual else 0
    last = first + (len(VISUAL_NAMES) if visual else len(NAMES)) - 1
    frames, warmup = (2, 1) if args.self_test else (args.frames, args.warmup)
    label = 'self-test' if args.self_test else args.mode
    output = args.output or OUT / args.device / ('visual-' + label if visual else label)
    output.parent.mkdir(parents=True, exist_ok=True)
    expected = {}
    with output.with_suffix('.log').open('w') as log:
        for width, height in resolutions:
            for kind in range(first, last + 1):
                expected[MODES[args.mode], width, height, kind] = frames
            with subprocess.Popen(command(args.device, [MODES[args.mode], width, height, warmup, frames, first, last]),
                                  cwd=ROOT, env=env, stdout=subprocess.PIPE, text=True) as process:
                for count, line in enumerate(process.stdout):
                    if count > 10000 or len(line) > 8192:
                        process.terminate()
                        raise RuntimeError('Unbounded benchmark output')
                    log.write(line)
                    log.flush()
                    if line.startswith(('workload ', 'unsupported ', 'gpu failure ')):
                        print(line.strip(), flush=True)
                if process.wait():
                    raise RuntimeError('Shader benchmark failed; see ' + str(output.with_suffix('.log')))
    results, identities = parse(output.with_suffix('.log'), expected, allow_software=args.self_test, allow_zero=args.self_test)
    if any(row['warmup_frames'] != warmup for row in results):
        raise ValueError('Reported workload warmup differs from the requested count')
    provenance = json.loads(binary(args.device).with_suffix('.build.json').read_text())
    with binary(args.device).open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != provenance['binary_sha256']:
            raise ValueError('Executable differs from build receipt')
    if args.device == 'browser':
        with binary(args.device).with_suffix('.wasm').open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != provenance['wasm_sha256']:
                raise ValueError('Wasm differs from build receipt')
    report = dict(schema=1, purpose='correctness' if args.self_test else 'diagnostic probe' if args.probe else 'benchmark',
                  provenance=provenance, warmup_frames=warmup, identities=identities,
                  timing='completed frame wall time; construction, compile, warmup and hash excluded',
                  p95='nearest rank', results=results)
    if visual:
        report['suite'] = 'visual'
    if args.compare:
        previous = json.loads(args.compare.read_text())
        if previous['provenance']['device'] != args.device:
            raise ValueError('Comparison benchmark device differs')
        if args.mode == 'scene-gpu' and previous['identities'] != identities:
            raise ValueError('Comparison hardware renderer identity differs')
        if previous['warmup_frames'] != warmup:
            raise ValueError('Comparison warmup differs')
        old = {(r['mode'], r['resolution'], r['shader']): r for r in previous['results']}
        if len(old) != len(results):
            raise ValueError('Comparison workload matrix differs')
        for row in results:
            key = (row['mode'], row['resolution'], row['shader'])
            if key not in old or row['hashes'] != old[key]['hashes']:
                raise ValueError('Comparison output or input frame sequence differs: ' + str(key))
            row['speedup'] = old[key]['mean_ms'] / row['mean_ms']
            row['p95_speedup'] = old[key]['p95_ms'] / row['p95_ms']
        report['comparison'] = str(args.compare)
    output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    if args.self_test:
        assert len(results) == last - first + 1 and all(r['samples'] == 2 for r in results)
        if visual:
            assert all(r['hashes'][0] != r['hashes'][1] for r in results), 'Visual workload did not animate'
        if args.mode == 'interpreter':
            # Independent UV-image oracle verifies coordinate orientation,
            # pixel centres, channel conversion and actual full-frame output.
            reference = 14695981039346656037
            for y in range(8):
                for x in range(8):
                    for byte in (int((x + .5) / 8 * 255 + .5), int((1 - (y + .5) / 8) * 255 + .5), 0, 255):
                        reference = ((reference ^ byte) * 1099511628211) & ((1 << 64) - 1)
            assert results[0]['hashes'] == [reference, reference], 'UV oracle differs'
            assert results[1]['hashes'][0] != results[1]['hashes'][1], 'Plasma animation missing'
            assert results[7]['hashes'][0] != results[7]['hashes'][1], 'Particle animation missing'
        print(f'Local {args.suite} benchmark correctness smoke: {len(results)} workloads, all frames completed')
    else:
        if args.probe:
            print('Diagnostic probe: one resolution; not a full benchmark matrix')
        print('| Shader | Resolution | Mean (ms) | p95 (ms) |')
        print('|---|---|---:|---:|')
        for row in results:
            print(f"| {row['shader']} | {row['resolution']} | {row['mean_ms']:,.3f} | {row['p95_ms']:,.3f} |")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', choices=('desktop', 'moto', 'pixel', 'browser', 'wine'), default='desktop')
    parser.add_argument('--mode', choices=MODES, default='interpreter')
    parser.add_argument('--suite', choices=('shader', 'visual'), default='shader')
    parser.add_argument('--prepared', action='store_true', help='Build the prepared CPU execution variant')
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--run-only', action='store_true')
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--probe', action='store_true', help='Diagnostic 640x360 run; use --frames/--warmup for short tuning runs')
    parser.add_argument('--frames', type=int, default=60)
    parser.add_argument('--warmup', type=int, default=8)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--compare', type=Path, help='Baseline JSON; require identical frame hashes and report speedup')
    args = parser.parse_args()
    if args.suite == 'visual' and args.mode == 'interpreter' and not args.build_only:
        parser.error('The visual suite requires --mode scene-cpu or --mode scene-gpu')
    if args.build_only and args.run_only or args.self_test and args.probe or not 1 <= args.frames <= 1000 or not 1 <= args.warmup <= 120:
        parser.error('Invalid build/run mode or frame count')
    if not args.run_only:
        build(args.device, args.prepared)
    if not args.build_only:
        measure(args)


if __name__ == '__main__':
    main()
