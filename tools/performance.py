#!/usr/bin/env python3
"""Build and measure Kryon's repeatable, display-free native UI workloads."""
import argparse
import csv
import fcntl
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = ('scrolling_list_10000', 'wrapped_document_8192_bytes', 'image_grid_144', 'idle_controls_20')


def summarize(rows):
    report = {}
    for index, name in enumerate(SCENARIOS):
        frames = [row for row in rows if int(row['scenario']) == index]
        if len(frames) != 60:
            raise ValueError(f'{name}: expected 60 measured frames, got {len(frames)}')
        entry = {'frames': len(frames)}
        for field in ('submission_ns', 'commit_raster_ns', 'total_ns'):
            values = sorted(int(row[field]) / 1e6 for row in frames)
            if any(value < 0 for value in values):
                raise ValueError('negative duration')
            entry[field.replace('_ns', '_ms')] = {
                'median': statistics.median(values), 'p95': values[math.ceil(len(values) * .95) - 1],
            }
        for field in ('nodes', 'paint', 'emitted', 'damage_pixels', 'workers'):
            entry[field] = {'min': min(int(row[field]) for row in frames),
                            'max': max(int(row[field]) for row in frames)}
        if index == 3 and entry['emitted']['max'] != 0:
            raise ValueError('idle workload emitted unchanged paint')
        report[name] = entry
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'backup/performance.json')
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--max-regression', type=float, default=.20)
    parser.add_argument('--budget-ms', type=float)
    args = parser.parse_args()
    if args.max_regression < 0 or not math.isfinite(args.max_regression):
        parser.error('--max-regression must be finite and nonnegative')
    if args.budget_ms is not None and (args.budget_ms <= 0 or not math.isfinite(args.budget_ms)):
        parser.error('--budget-ms must be finite and positive')
    build = ROOT / 'build/performance'
    build.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
        env.pop(key, None)
    env['YUE_DESKTOP_RECOVERY'] = '0'
    ziran = env.get('ZIRAN_BIN', 'ziran')
    toolchain = Path(subprocess.check_output([ziran, 'pkg', 'path', 'ziran'], cwd=ROOT, env=env, text=True).strip())
    ziran = str(toolchain / 'build/bin/ziran')
    # Keep stable build output exclusive for the entire build and run.
    with (build / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        subprocess.run([ziran, 'build', '--project', '--root', str(ROOT / 'tools'), '--target=c',
                        '--no-main', '--define', 'POSIX_THREADS', '--entry', 'performance:Run',
                        '-o', str(build / 'c'), str(ROOT / 'tools/performance.zi')], cwd=ROOT, env=env, check=True)
        (build / 'c/driver.c').write_text('#include "performance.h"\nint main(void) { return Run(); }\n')
        sources = sorted(str(path) for path in (build / 'c').glob('*.c'))
        compiler = env.get('CC', 'cc')
        subprocess.run([compiler, '-std=c11', '-O2', '-I' + str(toolchain / 'include'),
                        '-I' + str(build / 'c'), *sources, '-lm', '-lpthread', '-o', str(build / 'run')], env=env, check=True)
        # wait4 measures this runner only, excluding the compiler's peak RSS.
        raw = build / 'frames.csv'
        with raw.open('w') as stream:
            pid = os.posix_spawn(str(build / 'run'), [str(build / 'run')], env,
                                 file_actions=[(os.POSIX_SPAWN_DUP2, stream.fileno(), 1)])
            _, status, usage = os.wait4(pid, 0)
        if os.waitstatus_to_exitcode(status) != 0:
            raise RuntimeError('performance runner failed')
        output = raw.read_text()
        rows = list(csv.DictReader(output.splitlines()))
        hardware = {'machine': platform.machine(), 'system': platform.system(),
                    'cpu': next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                                 if line.startswith('model name')), platform.processor()),
                    'threads_requested': env.get('ZIRAN_PAR_THREADS', '4')}
        report = {'schema': 1, 'hardware': hardware, 'compiler': compiler, 'optimization': '-O2',
                  'viewport_pixels': [960, 600], 'warmup_frames': 10,
                  'runner_peak_rss_kib': usage.ru_maxrss, 'scenarios': summarize(rows)}
    failures = []
    if args.baseline:
        baseline = json.loads(args.baseline.read_text())
        for key in ('schema', 'hardware', 'compiler', 'optimization', 'viewport_pixels', 'warmup_frames'):
            if baseline[key] != report[key]:
                raise ValueError(f'baseline has a different {key}; compare only matching configurations')
        for name, entry in report['scenarios'].items():
            current = entry['total_ms']['p95']
            previous = baseline['scenarios'][name]['total_ms']['p95']
            if current > previous * (1 + args.max_regression) and current - previous > .2:
                failures.append(f'{name}: p95 {current:.3f} ms exceeds baseline {previous:.3f} ms')
    for name, entry in report['scenarios'].items():
        duration = entry['total_ms']['p95']
        print(f'{name}: median {entry["total_ms"]["median"]:.3f} ms, p95 {duration:.3f} ms')
        if args.budget_ms is not None and duration > args.budget_ms:
            failures.append(f'{name}: p95 exceeds {args.budget_ms} ms budget')
    report['checks'] = {'passed': not failures, 'failures': failures}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for failure in failures:
        print(failure, file=sys.stderr)
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
