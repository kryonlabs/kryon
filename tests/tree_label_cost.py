"""Identical label baseline/candidate workload, with independent digest oracle."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import statistics
import time

lib = ctypes.CDLL(os.environ['TREE_LABEL_COST_LIBRARY'])
lib.Prepare.restype = None
lib.Measure.argtypes = [ctypes.c_int32, ctypes.c_int32]
lib.Measure.restype = ctypes.c_uint64
lib.Prepare()
runs = []
for mode, name in enumerate(('repeated-8', 'unique-128', 'long-paths-8', 'composition-256')):
    frames = 120
    count = 128 if mode in (1, 3) else 8
    length = 2048 if mode == 2 else 32
    calls = 256 if mode == 3 else 512
    digest = 1
    for _ in range(frames):
        for i in range(calls):
            at = i % count
            value = length + 65 + at % 26 + 65 + at // 26 + 47 + (length - 1) % 73
            digest = ((digest ^ value) * 1099511628211) & ((1 << 64) - 1)
    for sample in range(-1, 7):
        start = time.process_time_ns()
        result = lib.Measure(mode, frames)
        elapsed = time.process_time_ns() - start
        assert result == digest, (name, result, digest)
        if sample >= 0:
            runs.append(dict(workload=name, sample=sample, frames=frames, calls=frames*calls, cpu_ns=elapsed, digest=result))
summary = {}
for row in runs:
    name = row['workload']
    times = [r['cpu_ns']/r['frames'] for r in runs if r['workload'] == name]
    summary[name] = dict(median_ns_per_frame=statistics.median(times), min_ns_per_frame=min(times), max_ns_per_frame=max(times))
result = dict(warmups=1, samples=7, flags='native C -O2 shared -fPIC -Wl,-Bsymbolic',
              harness_sha256=hashlib.sha256(Path(__file__).with_suffix('.zi').read_bytes()).hexdigest(), runs=runs, summary=summary)
Path(os.environ['TREE_LABEL_COST_OUTPUT']).write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(summary, indent=2))
