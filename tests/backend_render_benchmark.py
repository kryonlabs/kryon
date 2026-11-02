#!/usr/bin/env python3
"""Private-display common-recorder benchmark; run through build-queue.

Smoke: build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY
-u DBUS_SESSION_BUS_ADDRESS python3 tests/backend_render_benchmark.py --smoke
--output work/backend-render-benchmark/smoke.json

Exit 0: all requested scenes passed; exit 2: explicit unsupported comparison.
No timing is collected for a backend/scene failing the CPU pixel oracle with exact checker-boundary tie equivalence.
--artifact-project points to an EXISTING consumer locking/overriding this Kryon;
this harness never creates or changes package manifests. The preparation fixture
work/sdl3-integration is the default; prepare it with build-queue -- make sdl3-test. Artifacts use the package API.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import random
import signal
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/scratch/backend-render-benchmark'
WORK = ROOT / 'work/backend-render-benchmark'
SCENES = ['text', 'images', 'shapes', 'scroll', 'zoom']
BACKENDS = ['sdl2', 'sdl3', 'raylib']
BINDINGS = ['raster:RasterLine=pixmap:RasterLine',
            'raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle',
            'raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline',
            'raster_text:RasterText=pixmap:RasterText',
            'raster_text:RasterTextClipped=pixmap:RasterTextClipped',
            'raster_triangles:RasterTriangles=pixmap:RasterTriangles']


def capture(command, *, env=None, cwd=ROOT):
    return subprocess.check_output(list(map(str, command)), cwd=cwd, env=env, text=True).strip()


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def request_artifact(artifacts, selection, project, ziran, env, log):
    # Package providers can emit large native build logs. Preserve stderr on
    # disk while request() captures only the descriptor protocol on stdout.
    saved = os.dup(2)
    try:
        with log.open('w') as stream:
            os.dup2(stream.fileno(), 2)
            return artifacts.request(selection, 'linux-x86_64-gnu',
                                     project=project, ziran=ziran, env=env)
    finally:
        os.dup2(saved, 2)
        os.close(saved)


def percentile(values, p):
    return sorted(values)[max(0, math.ceil(len(values) * p) - 1)]


def bounded_run(command, *, env, output, timeout):
    process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=output,
                               stderr=subprocess.STDOUT, start_new_session=True)
    try:
        return process.wait(timeout=timeout)
    except BaseException:
        # Timeout/interrupt cleanup touches only this invocation's process group.
        try:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
        except ProcessLookupError:
            pass
        raise


def run_host(executable, env, log):
    # xvfb-run owns its server and authentication; inherited desktop discovery
    # is removed before this invocation, never recovered or captured.
    command = ['xvfb-run', '-a', '-s', '-screen 0 800x600x24 -nolisten tcp', str(executable)]
    with log.open('w') as output:
        code = bounded_run(command, env=env, output=output, timeout=180)
    # Stream only the benchmark protocol. Driver diagnostics stay in the log.
    records = []
    with log.open() as stream:
        for line in stream:
            if line.startswith(('IDENTITY ', 'PARITY ', 'WALL ', 'SAMPLE ', 'UNSUPPORTED ')):
                records.append(line.rstrip())
    return code, records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--output', type=Path, default=WORK / 'results.json')
    parser.add_argument('--artifact-project', type=Path, default=ROOT / 'work/sdl3-integration')
    parser.add_argument('--sdl3-artifact', type=Path, help='explicit package descriptor, verified with Artifact API')
    parser.add_argument('--raylib-artifact', type=Path, help='explicit package descriptor, verified with Artifact API')
    parser.add_argument('--trials', type=int)
    parser.add_argument('--frames', type=int)
    parser.add_argument('--warmup', type=int)
    parser.add_argument('--seed', type=int, default=73129)
    args = parser.parse_args()
    trials = args.trials or (2 if args.smoke else 5)
    frames = args.frames or (6 if args.smoke else 240)
    warmup = args.warmup or (3 if args.smoke else 120)
    if not 2 <= trials <= 30 or not 1 <= frames <= 10000 or not 1 <= warmup <= 10000:
        parser.error('require 2..30 trials and 1..10000 frames/warmup')
    # Never allow private receipts to leak into tracked paths, including via
    # a user-specified output path. No ignore rules are edited by this test.
    output = args.output.resolve()
    output.relative_to(ROOT)
    for path in (WORK, output):
        subprocess.run(['git', 'check-ignore', '-q', str(path)], cwd=ROOT, check=True)
    WORK.mkdir(parents=True, exist_ok=True)
    BUILD.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    logs = WORK / (output.stem + '-logs')
    logs.mkdir(exist_ok=True)
    env = dict(os.environ)
    for key in list(env):
        if key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER') or key.startswith(('SDL_', 'VK_', 'MESA_', 'GALLIUM_', 'LIBGL_')):
            env.pop(key)
    env.update(YUE_DESKTOP_RECOVERY='0', LIBGL_ALWAYS_SOFTWARE='1',
               SDL_VIDEO_DRIVER='x11', SDL_VIDEODRIVER='x11', SDL_RENDER_DRIVER='opengl',
               SDL_GPU_DRIVER='vulkan', KRYON_PIXMAP_GPU='1',
               ZIRAN_PAR_THREADS='1', MAKEFLAGS='-j1', CMAKE_BUILD_PARALLEL_LEVEL='1',
               TMPDIR=str(WORK), CFLAGS='-O2')
    # Force the Vulkan software ICD as well as software GL. Fail explicitly
    # rather than accidentally comparing different hardware in Xvfb.
    icds = sorted(Path('/usr/share/vulkan/icd.d').glob('lvp_icd*.json'))
    if icds:
        env['VK_DRIVER_FILES'] = str(icds[0])
    toolchain = load('benchmark_toolchain', ROOT / 'tests/toolchain.py')
    artifacts = load('benchmark_artifacts', toolchain.ZIRAN_ROOT / 'scripts/ziran_artifacts.py')
    renderer = load('benchmark_renderer_metadata', ROOT / 'scripts/renderer_artifact.py')
    report = dict(schema=1, scope='common pixmap raster/geometry recorder; raylib is test-only rlgl sink, not existing high-level renderer',
                  status='pending', software_only=True, hardware_fps_claim=False,
                  size=[480, 320], period=120, vsync=0, window='hidden on private Xvfb; no owner compositor', smoke=args.smoke,
                  seed=args.seed, trials=trials, frames=frames, warmup=warmup,
                  parity_frames=list(range(120)),
                  parity_tolerance=dict(max_channel=3, all_pixels=True, byte_exact=False,
                      exceptions='zoom only: exact analytic checker texel-boundary centres can match adjacent CPU texel along tied axis within same image and clip; raw counts retained'),
                  timing_scope=dict(compose='CPU scene paint and recorder; includes existing raster/atlas caches',
                                    submit='CPU dirty atlas/image uploads and ordered replay combined; NOT GPU execution',
                                    present='CPU target copy and present call',
                                    wall='frame loop plus final synchronous target readback drain; final readback cost included',
                                    readback='parity and warmup readbacks outside timings; one final drain readback inside wall only',
                                    upload='not isolated from replay by runtime sink API'),
                  environment=dict(platform=platform.platform(), python=platform.python_version(),
                                   compiler=capture([env.get('CC', 'cc'), '--version']).splitlines()[0],
                                   build_flags={k: env.get(k, '') for k in ('CFLAGS', 'CPPFLAGS', 'LDFLAGS', 'CC', 'CXXFLAGS')},
                                   cflags=env['CFLAGS'], ziran=str(toolchain.ZIRAN),
                                   variables={k: env[k] for k in ('LIBGL_ALWAYS_SOFTWARE', 'VK_DRIVER_FILES', 'SDL_GPU_DRIVER', 'SDL_RENDER_DRIVER', 'YUE_DESKTOP_RECOVERY') if k in env}),
                  backends={}, order=[], samples=[], summaries=[])
    source_paths = sorted(set((ROOT / 'src/ui').rglob('*.zi')) | set((ROOT / 'src/backend').rglob('*.zi')) |
                          set((ROOT / 'tests').glob('backend_render_benchmark*')) |
                          {ROOT / 'tests/backend_comparison_scene.zi', ROOT / 'ziran.toml', ROOT / 'ziran.lock'})
    report['source_sha256'] = {str(p.relative_to(ROOT)): digest(p) for p in source_paths if p.is_file()}
    report['tool_sha256'] = digest(toolchain.ZIRAN.resolve())
    common = [str(toolchain.ZIRAN), 'build', '--project', '--root', str(ROOT / 'tests'),
              '--module-path', str(ROOT / 'tests'), '--target=c', '--exe']
    common += [part for binding in BINDINGS for part in ('--bind', binding)]
    executables = {}
    rng = random.Random(args.seed)
    backend_order = BACKENDS.copy(); rng.shuffle(backend_order)
    for backend in backend_order:
        info = report['backends'][backend] = dict(status='pending', parity=[], logs=[])
        try:
            build_env = dict(env)
            flags = []
            if backend == 'sdl2':
                flags = shlex.split(capture(['pkg-config', '--libs', 'sdl2']))
            else:
                recipe = 'native-sdl3' if backend == 'sdl3' else 'renderer-raylib'
                explicit = args.sdl3_artifact if backend == 'sdl3' else args.raylib_artifact
                info['artifact_selection'] = 'kryon:' + recipe
                info['artifact_project'] = str(args.artifact_project.resolve())
                info['artifact_resolution'] = 'explicit verified descriptor' if explicit else 'package request API'
                if not explicit and not args.artifact_project.is_dir():
                    raise ValueError('existing artifact consumer missing; supply --artifact-project')
                artifact_log = logs / (backend + '-artifact.log'); info['logs'].append(str(artifact_log))
                artifact = artifacts.Artifact(explicit.resolve(), artifact=recipe, platform='linux-x86_64-gnu') if explicit else request_artifact(artifacts, 'kryon:' + recipe, args.artifact_project, os.environ.get('ZIRAN_BIN', 'ziran'), env, artifact_log)
                info['artifact_descriptor'] = str(artifact.descriptor)
                info['artifact_descriptor_sha256'] = digest(artifact.descriptor)
                if backend == 'sdl3':
                    if not icds:
                        raise ValueError('software Vulkan ICD not available')
                    flags = ['-L' + str(artifact.root / 'lib'), *artifact.link_flags(env=env)]
                else:
                    flags = ['-L' + str(artifact.root / 'lib'), *renderer.renderer_link(renderer.renderer_metadata(artifact))]
            build_env['LDLIBS'] = shlex.join(flags)
            source = ROOT / 'tests' / ('backend_render_benchmark_' + backend + '.zi')
            target = BUILD / backend
            command = [*common, '--entry', source.stem + ':main', '-o', str(target), str(source)]
            info['build_command'] = command; info['ldlibs'] = flags
            log = logs / (backend + '-build.log'); info['logs'].append(str(log))
            with log.open('w') as stream:
                code = bounded_run(command, env=build_env, output=stream, timeout=600)
            if code:
                raise ValueError(f'build exit {code}; see {log}')
            executable = target / source.stem
            info['executable_sha256'] = digest(executable)
            log = logs / (backend + '-parity.log'); info['logs'].append(str(log))
            code, records = run_host(executable, dict(env, BENCH_MODE='parity',
                                    BENCH_PARITY_FRAMES='120'), log)
            info['parity_exit'] = code
            info['identity'] = [line for line in records if line.startswith('IDENTITY ')]
            if not info['identity'] or not any('llvmpipe' in line.lower() for line in info['identity']):
                raise ValueError('actual llvmpipe device not verified; no software comparison allowed')
            for line in records:
                if line.startswith('PARITY '):
                    scene, frame, pixels, exact, maximum, failures, ties, raw_failures = map(int, line.split()[1:])
                    info['parity'].append(dict(scene=scene, frame=frame, pixels=pixels,
                                               exact=exact, max_channel_delta=maximum, failures=failures, sampling_ties=ties, raw_pixels_over_channel3=raw_failures))
            qualified = []
            for scene in range(5):
                rows = [row for row in info['parity'] if row['scene'] == scene]
                if (len(rows) == len(report['parity_frames']) and
                        {r['frame'] for r in rows} == set(report['parity_frames']) and
                        all(r['pixels'] == 153600 and r['failures'] == 0 for r in rows)):
                    qualified.append(scene)
            info['qualified_scenes'] = qualified
            info['status'] = 'supported' if len(qualified) == 5 and code == 0 else 'partial/unsupported'
            executables[backend] = executable
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            info.update(status='unsupported', reason=str(error), qualified_scenes=[])
    # Randomize backend order separately in every trial/scene, after ALL parity
    # gates. No invalid renderer/scene can reach the measured host invocation.
    for trial in range(trials):
        for scene in range(5):
            order = [b for b in BACKENDS if scene in report['backends'][b]['qualified_scenes']]
            rng.shuffle(order); report['order'].append(dict(trial=trial, scene=scene, backends=order))
            for backend in order:
                info = report['backends'][backend]
                log = logs / f'{backend}-{scene}-{trial}.log'; info['logs'].append(str(log))
                code, records = run_host(executables[backend], dict(env, BENCH_MODE='timing', BENCH_SCENE=str(scene),
                                         BENCH_FRAMES=str(frames), BENCH_WARMUP=str(warmup)), log)
                identity = [line for line in records if line.startswith('IDENTITY ')]
                rows = [list(map(int, line.split()[1:])) for line in records if line.startswith('SAMPLE ')]
                wall = [list(map(int, line.split()[1:])) for line in records if line.startswith('WALL ')]
                if code or identity != info['identity'] or len(rows) != frames or len(wall) != 1:
                    info.update(status='unsupported', reason=f'timing/identity failure: {log}')
                    continue
                report['samples'].append(dict(backend=backend, scene=SCENES[scene], trial=trial,
                    columns=['frame', 'compose_ns', 'submit_ns', 'present_ns'], latency_ns=rows,
                    wall_ns=wall[0][1], drain_ns=wall[0][2], frames=frames,
                    wall_frames_per_second=frames * 1e9 / wall[0][1]))
    for backend in BACKENDS:
        for scene in SCENES:
            runs = [r for r in report['samples'] if r['backend'] == backend and r['scene'] == scene]
            if not runs:
                continue
            summary = dict(backend=backend, scene=scene, trials=len(runs))
            for index, metric in enumerate(('compose', 'submit', 'present'), 1):
                values = [row[index] / 1000 for run in runs for row in run['latency_ns']]
                summary[metric + '_us'] = dict(p50=percentile(values, .5), p95=percentile(values, .95))
            summary['wall_frames_per_second_p50'] = percentile([r['wall_frames_per_second'] for r in runs], .5)
            report['summaries'].append(summary)
    after = {str(p.relative_to(ROOT)): digest(p) for p in source_paths if p.is_file()}
    report['source_changed_during_run'] = [p for p in after if after[p] != report['source_sha256'][p]]
    report['status'] = 'passed' if (all(v['status'] == 'supported' for v in report['backends'].values())
                        and len(report['samples']) == 15 * trials and not report['source_changed_during_run']) else 'unsupported comparison'
    report['completed_unix_seconds'] = time.time()
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': ' + str(output))
    for backend, info in report['backends'].items():
        print(backend, info['status'], 'qualified scenes:', ','.join(SCENES[s] for s in info['qualified_scenes']), info.get('reason', ''))
    print('backend scene submit p50/p95 us; present p50/p95 us; drained wall frames/s')
    for row in report['summaries']:
        print(f"{row['backend']} {row['scene']} {row['submit_us']['p50']:.1f}/{row['submit_us']['p95']:.1f}; "
              f"{row['present_us']['p50']:.1f}/{row['present_us']['p95']:.1f}; {row['wall_frames_per_second_p50']:.1f}")
    return 0 if report['status'] == 'passed' else 2


if __name__ == '__main__':
    raise SystemExit(main())
