#!/usr/bin/env python3
"""Render real controls and measure identical frames for a Gwenview review.

Run capture before changing the library, preserve its measurements and images,
then capture after. The comparison never imitates an old theme in current code.
"""
import argparse
import csv
import fcntl
import io
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import sys
import shlex
import hashlib

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/scratch/widget-review'
OUTPUT = ROOT / 'backup/material-widgets'
BACKEND = 'pixmap'
SELECTED_SCENES = None
# Scene labels and implementations have one source for desktop, web and review.
import re
SCENES = re.findall(r'if index == \d+ \{ return "([^"]+)" \}',
                    (ROOT / 'examples/gallery/src/scenes.zi').read_text())

# Preserve recorded review indexes when the gallery gains new controls.
SCENES = SCENES[:54] + ['System palette light', 'System palette dark'] + SCENES[54:] + ['Card dark']

INTERACTIVE = set(range(8)) | {9, 10, 11, 12, 13, 14, 17, 18, 19, 20, 24, 26, 35, 44, 46, 50, 52, 53, 54, 55}
DISABLED = INTERACTIVE | {21, 27, 33, 36, 37, 38, 39, 40, 48, 49}
STATE_NAMES = ['Resting', 'Disabled', 'Pointer hover', 'Keyboard focus', 'Pointer press']


def states(scene):
    return [0] + ([1] if scene in DISABLED else []) + ([2, 3, 4] if scene in INTERACTIVE else [])


def environment():
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'WAYLAND_SOCKET', 'DESKTOP_STARTUP_ID'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    env['KRYON_REVIEW_FIXTURE'] = str(OUTPUT / 'fixture.png')
    env.update(TMPDIR=str(OUTPUT), ZIRAN_PAR_THREADS='1', SDL_RENDER_DRIVER='software', LIBGL_ALWAYS_SOFTWARE='1')
    return env


def build(label, env):
    toolchain = Path(env.get('ZIRAN_ROOT') or subprocess.check_output(
        ['ziran', 'pkg', 'path', 'ziran'], cwd=ROOT, env=env, text=True).strip())
    generated = BUILD / f'{BACKEND}-c'
    for pattern in ('*.c', '*.h'):
        for path in generated.glob(pattern):
            path.unlink()
    storage = (ROOT / 'src/ui/paint_queue.zi').read_text()
    defines = []
    if 'pending_short_text:' in storage:
        defines = ['--define', 'REVIEW_SIZED_TEXT']
    elif 'pending_text_first:' in storage:
        defines = ['--define', 'REVIEW_INLINE_TEXT']
    if BACKEND == 'desktop': defines += ['--define', 'REVIEW_DESKTOP']
    subprocess.run([str(toolchain / 'build/bin/ziran'), 'build', '--project', '--root', 'tools',
                    '--target=c', '--no-main', '--define', 'POSIX_THREADS',
                    '--module-path', 'examples/gallery/src',
                    *defines,
                    '--entry', 'widget_review:Run', '-o', str(generated),
                    'tools/widget_review.zi'], cwd=ROOT, env=env, check=True)
    (generated / 'driver.c').write_text('#include "widget_review.h"\nint main(void) { return Run(); }\n')
    binary = BUILD / BACKEND
    subprocess.run([env.get('CC', 'cc'), '-std=c11', '-O2', '-I' + str(toolchain / 'include'),
                    '-I' + str(generated), *map(str, sorted(generated.glob('*.c'))),
                    *(['-lm', '-lpthread'] if BACKEND == 'pixmap' else subprocess.check_output(['pkg-config', '--libs', 'sdl2', 'cairo', 'freetype2', 'pangocairo', 'fontconfig', 'glib-2.0', 'x11'], text=True).split() + ['-lm', '-lpthread', '-ldl']), '-o', str(binary)], env=env, check=True)
    return binary


def binary_digest(binary):
    with binary.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def capture(label, binary, env, limit=None):
    label = f"{label}-{BACKEND}"
    directory = OUTPUT / label
    directory.mkdir(parents=True, exist_ok=True)
    report = {'viewport': [480, 320], 'warmup_frames': 10, 'measured_frames': 120, 'backend': BACKEND,
              'command': shlex.join(sys.argv), 'cpu_affinity': sorted(os.sched_getaffinity(0)),
              'binary_sha256': binary_digest(binary),
              'binary_bytes': binary.stat().st_size, 'widgets': {}}
    for scene, name in enumerate(SCENES[:limit]):
        if SELECTED_SCENES is not None and scene not in SELECTED_SCENES: continue
        variants = {}
        for state in states(scene):
            child_env = {**env, 'KRYON_REVIEW_SCENE': str(scene), 'KRYON_REVIEW_STATE': str(state)}
            shot = directory / f'{scene:02d}-{state}.png'
            child_env['KRYON_CAPTURE_PATH'] = str(shot)
            result = subprocess.run(['xvfb-run', '-a', str(binary)], env=child_env,
                                    stdout=subprocess.PIPE, stderr=None, cwd=ROOT, check=True)
            image = Image.open(shot if BACKEND == 'desktop' else io.BytesIO(result.stdout)).convert('RGB')
            assert image.size == (480, 320)
            image.save(directory / f'{scene:02d}-{state}.png')
            modes = {}
            for mode in ('retained', 'repaint'):
                raw = directory / f'{scene:02d}-{state}-{mode}.csv'
                with raw.open('wb') as stream:
                    subprocess.run(['xvfb-run', '-a', str(binary)],
                                   env={**child_env, 'KRYON_REVIEW_METRICS': '1',
                                        'KRYON_REVIEW_ACTIVE': str(int(mode == 'repaint'))},
                                   stdout=stream, cwd=ROOT, check=True)
                with raw.open() as stream:
                    lines = list(stream)  # bounded: 120 samples and six metadata lines
                meta = dict(line[2:].strip().split(',', 1) for line in lines if line.startswith('# '))
                rows = list(csv.DictReader(line for line in lines if not line.startswith('#')))
                assert len(rows) == 120
                values = sorted(int(row['total_ns']) / 1e6 for row in rows)
                modes[mode] = {'median_ms': statistics.median(values),
                              'p95_ms': values[math.ceil(len(values) * .95) - 1],
                              'cpu_ms': statistics.median(int(row['cpu_ns']) / 1e6 for row in rows),
                              'peak_rss_kib': int(meta['runner_peak_rss_kib']),
                              'steady_rss_kib': int(meta['steady_rss_kib']),
                              'startup_ms': int(meta['startup_ns']) / 1e6,
                              'idle_cpu_pct': int(meta['idle_cpu_ns']) / int(meta['idle_wall_ns']) * 100,
                              'paint_commands': max(int(row['paint']) for row in rows),
                              'frame_text_reserve_bytes': max(int(row['frame_text_reserve_bytes']) for row in rows),
                              'idle_emissions': max(int(row['emitted']) for row in rows)}
            variants[str(state)] = {**modes['retained'], 'repaint': modes['repaint']}
        variants['states'] = {str(state): STATE_NAMES[state] for state in states(scene)}
        report['widgets'][name] = variants
        print(f'{label}: {name}', flush=True)
    (OUTPUT / f'{label}-widgets.json').write_text(json.dumps(report, indent=2) + '\n')


def compare():
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 17)
    small = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 12)
    reports = {label: json.loads((OUTPUT / f'{label}-{BACKEND}-widgets.json').read_text())
               for label in ('before', 'after')}
    summary = {'backend': BACKEND, 'statistic': 'median across identical scene/state cases',
               'cases': sum(len(widget) - 1 for widget in reports['before']['widgets'].values()),
               'results': {}, 'timing_regressions': []}
    for label, report in reports.items():
        values = [value for widget in report['widgets'].values()
                  for key, value in widget.items() if key != 'states']
        summary['results'][label] = {'binary_bytes': report['binary_bytes']}
        summary['results'][label]['max_peak_rss_kib'] = max(
            sample['peak_rss_kib'] for value in values for sample in (value, value['repaint']))
        for mode in ('retained', 'repaint'):
            samples = values if mode == 'retained' else [v['repaint'] for v in values]
            summary['results'][label][mode] = {
                metric: statistics.median(v[metric] for v in samples)
                for metric in ('median_ms', 'cpu_ms', 'peak_rss_kib', 'steady_rss_kib', 'idle_cpu_pct', 'startup_ms')}
        summary['results'][label]['max_retained_emissions'] = max(v['idle_emissions'] for v in values)
    for name, widget in reports['before']['widgets'].items():
        for state, first in widget.items():
            if state == 'states': continue
            second = reports['after']['widgets'][name][state]
            for mode in ('retained', 'repaint'):
                before = first if mode == 'retained' else first[mode]
                after = second if mode == 'retained' else second[mode]
                for metric in ('median_ms', 'cpu_ms'):
                    if after[metric] > before[metric] * 1.05 and after[metric] - before[metric] > .025:
                        summary['timing_regressions'].append({'widget': name, 'state': state,
                            'mode': mode, 'metric': metric, 'before': before[metric], 'after': after[metric]})
    (OUTPUT / f'comparison-{BACKEND}.json').write_text(json.dumps(summary, indent=2) + '\n')
    directory = OUTPUT / f'comparison-{BACKEND}'
    directory.mkdir(parents=True, exist_ok=True)
    thumbs = []
    overview = Image.new('RGB', (1968, 64 + math.ceil(len(SCENES) / 4) * 220), '#edf3f4')
    overview_draw = ImageDraw.Draw(overview)
    overview_draw.text((16, 10), 'Kryon: every review scene, before and after', font=font, fill='#17313e')
    overview_draw.text((16, 36), 'Resting states | detailed state comparisons: grid-01 through grid-10 | zoom for labels and metrics', font=small, fill='#42616d')
    for scene, name in enumerate(SCENES):
        if name not in reports['before']['widgets']: continue
        scene_states = states(scene)
        height = 76 + len(scene_states) * 372 + 24
        sheet = Image.new('RGB', (984, height), '#edf3f4')
        draw = ImageDraw.Draw(sheet)
        draw.text((16, 10), name, font=font, fill='#17313e')
        for column, label in enumerate(('before', 'after')):
            x = 8 + column * 488
            draw.text((x + 12, 42), label.upper(), font=small, fill='#42616d')
            for row, state in enumerate(scene_states):
                y = 68 + row * 372
                shot = Image.open(OUTPUT / f'{label}-{BACKEND}' / f'{scene:02d}-{state}.png')
                sheet.paste(shot, (x, y))
                metrics = reports[label]['widgets'][name][str(state)]
                draw.text((x + 12, y + 326),
                          f'{STATE_NAMES[state]} | median {metrics["median_ms"]:.3f} ms | p95 {metrics["p95_ms"]:.3f} ms',
                          font=small, fill='#42616d')
                draw.text((x + 12, y + 348),
                          f'Text pool {metrics["frame_text_reserve_bytes"] / 1024:.0f} KiB | peak RSS {metrics["peak_rss_kib"] / 1024:.2f} MiB | max raster {metrics["idle_emissions"]}',
                          font=small, fill='#42616d')
        draw.text((20, height - 24), f'Actual Kryon {BACKEND} renders | same machine, -O2 | 10 warmup + 120 frames | retained ms/frame | RSS varies per run', font=small, fill='#42616d')
        sheet.save(directory / f'{scene:02d}-{name.replace(" ", "-")}.png')
        resting = sheet.crop((0, 0, 984, 440)).resize((492, 220), Image.Resampling.LANCZOS)
        overview.paste(resting, ((scene % 4) * 492, 64 + (scene // 4) * 220))
        thumb = sheet.copy(); thumb.thumbnail((492, 794)); thumbs.append(thumb)
    overview.save(directory / 'grid-overview.png')
    for page in range(math.ceil(len(thumbs) / 6)):
        grid = Image.new('RGB', (1476, 1588), '#edf3f4')
        for index, thumb in enumerate(thumbs[page * 6:page * 6 + 6]):
            grid.paste(thumb, ((index % 3) * 492, (index // 3) * 794))
        grid.save(directory / f'grid-{page + 1:02d}.png')


def recheck(trials):
    """Repeat flagged cases without replacing the original comparison samples."""
    comparison = json.loads((OUTPUT / f'comparison-{BACKEND}.json').read_text())
    before = json.loads((OUTPUT / f'before-{BACKEND}-widgets.json').read_text())
    after = json.loads((OUTPUT / f'after-{BACKEND}-widgets.json').read_text())
    binary = BUILD / BACKEND
    digest = binary_digest(binary)
    if digest != after['binary_sha256']:
        raise RuntimeError('Recheck executable differs from the recorded after build')
    cases = sorted({(item['widget'], item['state'], item['mode'])
                    for item in comparison['timing_regressions']})
    directory = OUTPUT / f'recheck-{BACKEND}'
    directory.mkdir(exist_ok=True)
    result = {'backend': BACKEND, 'binary_sha256': digest, 'trials': trials,
              'cpu_affinity': sorted(os.sched_getaffinity(0)), 'cases': [], 'confirmed_regressions': []}
    env = environment()
    for name, state, mode in cases:
        scene = SCENES.index(name)
        original = before['widgets'][name][state]
        if mode == 'repaint': original = original[mode]
        samples = []
        for trial in range(trials):
            path = directory / f'{scene:02d}-{state}-{mode}-{trial}.csv'
            with path.open('wb') as stream:
                subprocess.run(['xvfb-run', '-a', str(binary)], cwd=ROOT, check=True, stdout=stream,
                               env={**env, 'KRYON_REVIEW_SCENE': str(scene), 'KRYON_REVIEW_STATE': state,
                                    'KRYON_REVIEW_METRICS': '1', 'KRYON_REVIEW_ACTIVE': str(int(mode == 'repaint'))})
            with path.open() as stream:
                rows = list(csv.DictReader(line for line in stream if not line.startswith('#')))
            assert len(rows) == 120
            samples.append({'median_ms': statistics.median(int(row['total_ns']) / 1e6 for row in rows),
                            'cpu_ms': statistics.median(int(row['cpu_ns']) / 1e6 for row in rows)})
        case = {'widget': name, 'state': state, 'mode': mode,
                'before': {key: original[key] for key in ('median_ms', 'cpu_ms')}, 'after_trials': samples,
                'after_median': {key: statistics.median(sample[key] for sample in samples)
                                 for key in ('median_ms', 'cpu_ms')}}
        result['cases'].append(case)
        for metric, value in case['after_median'].items():
            if value > original[metric] * 1.05 and value - original[metric] > .025:
                result['confirmed_regressions'].append({**case, 'metric': metric})
        print(f'{name} ({STATE_NAMES[int(state)]}, {mode}): {case["before"]} -> {case["after_median"]}', flush=True)
    (OUTPUT / f'recheck-{BACKEND}.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(cases)} cases repeated; {len(result["confirmed_regressions"])} confirmed timing regressions')
    return int(bool(result['confirmed_regressions']))


def main():
    global BACKEND, OUTPUT, SELECTED_SCENES
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('before', 'after', 'compare', 'recheck'))
    parser.add_argument('--backend', choices=('pixmap', 'desktop'), default='pixmap')
    parser.add_argument('--output-dir', type=Path, default=OUTPUT)
    parser.add_argument('--scenes', help='comma-separated scene indexes; default: all')
    parser.add_argument('--reuse-binary', action='store_true')
    parser.add_argument('--trials', type=int, choices=(3, 7), default=3,
                        help='repeat count for recheck; use seven for noisy shared machines')
    args = parser.parse_args()
    BACKEND = args.backend
    OUTPUT = args.output_dir.resolve()
    SELECTED_SCENES = set(map(int, args.scenes.split(','))) if args.scenes else None
    BUILD.mkdir(parents=True, exist_ok=True); OUTPUT.mkdir(parents=True, exist_ok=True)
    with (BUILD / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if args.action == 'compare':
            compare()
        elif args.action == 'recheck':
            sys.exit(recheck(args.trials))
        else:
            fixture = Image.new('RGB', (64, 64))
            for y in range(64):
                for x in range(64): fixture.putpixel((x, y), (40, 125, 145) if (y // 8 + x // 8) % 2 == 0 else (131, 181, 160))
            fixture.save(OUTPUT / 'fixture.png')
            env = environment()
            binary = BUILD / BACKEND if args.reuse_binary else build(args.action, env)
            capture(args.action, binary, env)


if __name__ == '__main__':
    main()
