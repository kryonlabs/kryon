#!/usr/bin/env python3
"""Render real controls and measure identical frames for a Gwenview review.

Run capture before changing the library, preserve that executable, then capture
after. The comparison is never reconstructed with an imitation old theme.
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

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/scratch/widget-review'
OUTPUT = ROOT / 'backup/widget-improvement'
# Scene labels and implementations have one source for desktop, web and review.
import re
SCENES = re.findall(r'if index == \d+ \{ return "([^"]+)" \}',
                    (ROOT / 'examples/gallery/src/scenes.zi').read_text())

INTERACTIVE = set(range(8)) | {9, 10, 11, 12, 13, 14, 17, 18, 19, 20, 24, 26, 35, 44, 46, 50, 52, 53}
DISABLED = INTERACTIVE | {21, 27, 33, 36, 37, 38, 39, 40, 48, 49}
STATE_NAMES = ['Resting', 'Disabled', 'Pointer hover', 'Keyboard focus']


def states(scene):
    return [0] + ([1] if scene in DISABLED else []) + ([2, 3] if scene in INTERACTIVE else [])


def environment():
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    return env


def build(label, env):
    toolchain = Path(env.get('ZIRAN_ROOT') or subprocess.check_output(
        ['ziran', 'pkg', 'path', 'ziran'], cwd=ROOT, env=env, text=True).strip())
    generated = BUILD / f'{label}-c'
    for pattern in ('*.c', '*.h'):
        for path in generated.glob(pattern):
            path.unlink()
    storage = (ROOT / 'src/ui/paint_queue.zi').read_text()
    defines = []
    if 'pending_short_text:' in storage:
        defines = ['--define', 'REVIEW_SIZED_TEXT']
    elif 'pending_text_first:' in storage:
        defines = ['--define', 'REVIEW_INLINE_TEXT']
    subprocess.run([str(toolchain / 'build/bin/ziran'), 'build', '--root', 'tools',
                    '--target=c', '--no-main', '--define', 'POSIX_THREADS',
                    '--module-path', 'examples/gallery/src',
                    '--module-path', str(ROOT / 'src/ui'),
                    '--module-path', str(ROOT / 'src/backend'),
                    '--module-path', 'kryon=' + str(ROOT / 'src/ui'),
                    '--module-path', 'kryon=' + str(ROOT / 'src/backend'),
                    '--module-path', str(toolchain / 'std'),
                    *defines,
                    '--entry', 'widget_review:Run', '-o', str(generated),
                    'tools/widget_review.zi'], cwd=ROOT, env=env, check=True)
    (generated / 'driver.c').write_text('#include "widget_review.h"\nint main(void) { return Run(); }\n')
    binary = BUILD / label
    subprocess.run([env.get('CC', 'cc'), '-std=c11', '-O2', '-I' + str(toolchain / 'include'),
                    '-I' + str(generated), *map(str, sorted(generated.glob('*.c'))),
                    '-lm', '-lpthread', '-o', str(binary)], env=env, check=True)
    return binary


def capture(label, binary, env, limit=None):
    directory = OUTPUT / label
    directory.mkdir(parents=True, exist_ok=True)
    report = {'viewport': [480, 320], 'warmup_frames': 10, 'measured_frames': 60,
              'binary_bytes': binary.stat().st_size, 'widgets': {}}
    for scene, name in enumerate(SCENES[:limit]):
        variants = {}
        for state in states(scene):
            child_env = {**env, 'KRYON_REVIEW_SCENE': str(scene), 'KRYON_REVIEW_STATE': str(state)}
            result = subprocess.run(['xvfb-run', '-a', str(binary)], env=child_env,
                                    stdout=subprocess.PIPE, stderr=None, check=True)
            image = Image.open(io.BytesIO(result.stdout)).convert('RGB')
            assert image.size == (480, 320)
            image.save(directory / f'{scene:02d}-{state}.png')
            raw = directory / f'{scene:02d}-{state}.csv'
            with raw.open('wb') as stream:
                pid = os.posix_spawn(str(binary), [str(binary)],
                                     {**child_env, 'KRYON_REVIEW_METRICS': '1'},
                                     file_actions=[(os.POSIX_SPAWN_DUP2, stream.fileno(), 1)])
                _, status, usage = os.wait4(pid, 0)
            if os.waitstatus_to_exitcode(status) != 0:
                raise RuntimeError(f'{name}: metrics process failed')
            lines = raw.read_text().splitlines()
            peak_rss = int(next(line.split(',')[1] for line in lines if line.startswith('# runner_peak_rss_kib,')))
            rows = list(csv.DictReader(line for line in lines if not line.startswith('#')))
            assert len(rows) == 60
            values = sorted(int(row['total_ns']) / 1e6 for row in rows)
            variants[str(state)] = {'median_ms': statistics.median(values),
                                    'p95_ms': values[math.ceil(len(values) * .95) - 1],
                                    'peak_rss_kib': peak_rss,
                                    'paint_commands': max(int(row['paint']) for row in rows),
                                    'frame_text_reserve_bytes': max(int(row['frame_text_reserve_bytes']) for row in rows),
                                    'idle_emissions': max(int(row['emitted']) for row in rows)}
        variants['states'] = {str(state): STATE_NAMES[state] for state in states(scene)}
        report['widgets'][name] = variants
        print(f'{label}: {name}', flush=True)
    (OUTPUT / f'{label}-widgets.json').write_text(json.dumps(report, indent=2) + '\n')


def compare():
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 17)
    small = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 12)
    reports = {label: json.loads((OUTPUT / f'{label}-widgets.json').read_text())
               for label in ('before', 'after')}
    directory = OUTPUT / 'comparison'
    directory.mkdir(parents=True, exist_ok=True)
    thumbs = []
    overview = Image.new('RGB', (1968, 64 + math.ceil(len(SCENES) / 4) * 220), '#edf3f4')
    overview_draw = ImageDraw.Draw(overview)
    overview_draw.text((16, 10), 'Kryon: every review scene, before and after', font=font, fill='#17313e')
    overview_draw.text((16, 36), 'Resting states | detailed state comparisons: grid-01 through grid-09 | zoom for labels and metrics', font=small, fill='#42616d')
    for scene, name in enumerate(SCENES):
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
                shot = Image.open(OUTPUT / label / f'{scene:02d}-{state}.png')
                sheet.paste(shot, (x, y))
                metrics = reports[label]['widgets'][name][str(state)]
                draw.text((x + 12, y + 326),
                          f'{STATE_NAMES[state]} | median {metrics["median_ms"]:.3f} ms | p95 {metrics["p95_ms"]:.3f} ms',
                          font=small, fill='#42616d')
                draw.text((x + 12, y + 348),
                          f'Text pool {metrics["frame_text_reserve_bytes"] / 1024:.0f} KiB | peak RSS {metrics["peak_rss_kib"] / 1024:.2f} MiB | max raster {metrics["idle_emissions"]}',
                          font=small, fill='#42616d')
        draw.text((20, height - 24), 'Actual Kryon pixmap renders | same machine, -O2 | 10 warmup + 60 frames | ms/frame | RSS varies per run', font=small, fill='#42616d')
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('before', 'after', 'compare'))
    parser.add_argument('--reuse-binary', action='store_true')
    args = parser.parse_args()
    BUILD.mkdir(parents=True, exist_ok=True); OUTPUT.mkdir(parents=True, exist_ok=True)
    with (BUILD / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if args.action == 'compare':
            compare()
        else:
            env = environment()
            binary = BUILD / args.action if args.reuse_binary else build(args.action, env)
            capture(args.action, binary, env)


if __name__ == '__main__':
    main()
