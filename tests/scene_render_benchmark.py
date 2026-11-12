#!/usr/bin/env python3
"""Fixed full-resolution CPU renderer workloads. Run through build-queue."""
import argparse
import os
from pathlib import Path
import shlex
from statistics import median
from threading import Event, Thread
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
OUT = ROOT / 'build/scratch/scene-render-benchmark'
KINDS = ('clear/resolve', 'unlit', 'textured', '12-instruction shader', 'lit', 'lit programmable soft shadows', '16 opaque lit programmable layers')
def run(args):
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)
def records(lines):
    result = {}
    for line in lines:
        if not line.startswith('scene '): continue
        fields = list(map(int, line.split()[1:]))
        if len(fields) not in (6, 7): raise ValueError('Invalid benchmark row')
        key = tuple(fields[:4])
        if key in result: raise ValueError('Duplicate benchmark row')
        result[key] = (fields[4], fields[5] if len(fields) == 7 else None, fields[-1])
    if len(result) != 30: raise ValueError('Expected three frames at both sizes for all five workloads')
    return result
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compare', type=Path, help='Earlier raw output; compare every full-frame pixel hash')
    parser.add_argument('--output', type=Path, help='Save the 30 raw timing/hash rows')
    parser.add_argument('--motion', action='store_true', help='Also measure 120 changing-camera frames at three full resolutions')
    parser.add_argument('--shadows', action='store_true', help='Measure 120 full-quality programmable soft-shadow frames at three resolutions')
    parser.add_argument('--overdraw', action='store_true', help='Measure 12 changing-camera frames of 16 opaque lit programmable layers at three resolutions')
    args = parser.parse_args()
    gen = OUT / 'c'; ir = OUT / '.ir'
    OUT.mkdir(parents=True, exist_ok=True)
    entry = 'scene_render_benchmark:main'
    run([ZIRAN, 'ir', '--project', '--define', 'POSIX_THREADS', '--root', ROOT / 'tests', '--entry', entry, '-o', ir, ROOT / 'tests/scene_render_benchmark.zi'])
    run([ZIRAN.with_name('zi2c'), '--entry', entry, '--root', ir, '-o', gen, *sorted(ir.glob('*.zir'))])
    executable = OUT / 'scene_render_benchmark'
    run([*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-O2', '-ffunction-sections', '-fdata-sections', '-I' + str(ZIRAN_ROOT / 'include'), '-iquote', gen, *sorted(gen.glob('*.c')), '-Wl,--gc-sections', '-pthread', '-lm', '-o', executable])
    mode = ['--motion', '--shadows', '--overdraw'] if args.overdraw else ['--motion', '--shadows'] if args.shadows else ['--motion'] if args.motion else []
    process = subprocess.Popen([str(executable), *mode], cwd=ROOT, text=True, stdout=subprocess.PIPE)
    stopped = Event()
    memory = {'VmRSS': 0, 'VmHWM': 0}
    def sample_memory():
        while not stopped.is_set():
            try:
                for row in Path(f'/proc/{process.pid}/status').read_text().splitlines():
                    key, _, value = row.partition(':')
                    if key in memory: memory[key] = max(memory[key], int(value.split()[0]))
            except FileNotFoundError: break
            stopped.wait(0.01)
    sampler = Thread(target=sample_memory)
    sampler.start()
    lines = []
    motion = []
    metadata = []
    for line in process.stdout:
        print(line, end='', flush=True)
        if line.startswith('scene '):
            if len(lines) >= 30:
                process.terminate(); process.wait()
                raise RuntimeError('Unexpected extra benchmark output')
            lines.append(line)
        elif line.startswith('motion '): motion.append(line)
        elif line.startswith(('cold ', 'workers ', 'memory ', 'statistics ')): metadata.append(line)
    code = process.wait()
    stopped.set(); sampler.join()
    if code: raise RuntimeError('Scene benchmark failed')
    current = records(lines)
    if args.output:
        args.output.write_text(''.join(metadata + lines + motion) + f'process_peak_rss_kib {memory["VmRSS"]}\nprocess_peak_hwm_kib {memory["VmHWM"]}\n')
    print('Renderer process sampled peak RSS/HWM (KiB):', memory)
    previous = None
    if args.compare:
        with args.compare.open() as source: previous = records(source)
        if current.keys() != previous.keys(): raise RuntimeError('Benchmark workload keys differ')
        for key, value in current.items():
            if value[-1] != previous[key][-1]: raise RuntimeError('Full-frame pixel hash differs at ' + str(key))
        print('All 30 full-frame pixel hashes match')
        if args.motion or args.shadows or args.overdraw:
            def motion_hashes(rows):
                result = {}
                for row in rows:
                    if row.startswith('motion '):
                        fields = list(map(int, row.split()[1:]))
                        key = tuple(fields[:4])
                        if key in result: raise ValueError('Duplicate motion frame')
                        result[key] = fields[-1]
                return result
            with args.compare.open() as source: earlier_motion = motion_hashes(source)
            current_motion = motion_hashes(motion)
            workloads = {key[:3] for key in current_motion}
            earlier_motion = {key: value for key, value in earlier_motion.items() if key[:3] in workloads}
            if earlier_motion and current_motion != earlier_motion:
                raise RuntimeError('Sustained motion full-frame pixel hashes differ')
            if earlier_motion: print(f'All {len(earlier_motion)} motion full-frame pixel hashes match')
    for width, height in ((960, 600), (420, 900)):
        for kind, label in enumerate(KINDS[:5]):
            keys = [(width, height, kind, frame) for frame in range(3)]
            wall = median(current[key][0] for key in keys) / 1e6
            cpu = median(current[key][1] for key in keys) / 1e6
            text = f'{width}x{height} {label}: median {wall:.2f} ms wall, {cpu:.2f} ms process CPU'
            if previous:
                ratio = median(previous[key][0] for key in keys) / median(current[key][0] for key in keys)
                text += f', {ratio:.2f}x wall speedup'
                if all(previous[key][1] is not None for key in keys):
                    ratio = median(previous[key][1] for key in keys) / median(current[key][1] for key in keys)
                    text += f', {ratio:.2f}x CPU work reduction'
            print(text)
    if args.motion or args.shadows or args.overdraw:
        groups = {}
        for row in motion:
            width, height, kind, frame, wall, cpu, pixel_hash = map(int, row.split()[1:])
            groups.setdefault((width, height, kind), []).append(wall / 1e6)
        if len(groups) != (3 if args.shadows or args.overdraw else 15) or any(len(values) != (12 if args.overdraw else 120) for values in groups.values()):
            raise RuntimeError('Incomplete sustained motion benchmark')
        for (width, height, kind), values in sorted(groups.items()):
            ordered = sorted(values)
            def percentile(q):
                at = (len(ordered)-1)*q; low = int(at)
                return ordered[low] + (ordered[min(low+1,len(ordered)-1)]-ordered[low])*(at-low)
            print(f'Motion {width}x{height} {KINDS[kind]} n={len(values)}: p50={percentile(.5):.2f} p95={percentile(.95):.2f} p99={percentile(.99):.2f} ms; {sum(v<=1000/60 for v in values)}/{len(values)} within 16.67ms')
if __name__ == '__main__': main()
