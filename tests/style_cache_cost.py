"""Installed resolution: identical baseline/candidate workloads, independent oracle."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import resource
import statistics
import time

lib = ctypes.CDLL(os.environ['KRYON_STYLE_COST_LIBRARY'])
lib.Prepare.argtypes = [ctypes.c_int32]
lib.Prepare.restype = ctypes.c_bool
lib.Query.argtypes = [ctypes.c_int32, ctypes.c_int32]
lib.Query.restype = ctypes.c_uint64
lib.Updates.argtypes = [ctypes.c_int32, ctypes.c_int32]
lib.Updates.restype = ctypes.c_uint64
MASK = (1 << 64) - 1

def oracle(count, mode, calls):
    digest = 1
    for i in range(calls):
        q = 2 if mode == 0 else i
        eligible = [r for r in range(count)
                    if (r % 3 != 0 or q % 2 == 0)
                    and (r % 5 != 0 or q % 997 == 2)
                    and (r % 4 != 0 or q % 4 >= 2)]
        winner = max(eligible, key=lambda r: (r % 3, (r % 3 == 0) + 20 * (r % 5 == 0), r), default=None)
        total = i if winner is None else winner * 2 + 300 + 12 + winner % 9
        digest = ((digest ^ (total + i % 11)) * 1099511628211) & MASK
    return digest

runs = []
for count in (8, 32, 320, 512):
    assert lib.Prepare(count)
    for mode, label in ((0, 'hot'), (1, 'varied')):
        calls = 25000
        expected = oracle(count, mode, calls)
        for sample in range(-1, 7):
            start = time.process_time_ns()
            value = lib.Query(mode, calls)
            elapsed = time.process_time_ns() - start
            assert value == expected, (count, label, value, expected)
            if sample >= 0:
                runs.append(dict(rules=count, workload=label, sample=sample, calls=calls, cpu_ns=elapsed, digest=value))
    for label in ('install', 'updates'):
        calls = 51
        expected = 0
        for i in range(calls):
            expected ^= oracle(count - i % 2, 1, 13)
        for sample in range(-1, 7):
            start = time.process_time_ns()
            if label == 'install':
                for _ in range(calls): assert lib.Prepare(count)
            else:
                assert lib.Updates(calls, count) == expected
            elapsed = time.process_time_ns() - start
            if sample >= 0:
                runs.append(dict(rules=count, workload=label, sample=sample, calls=calls, cpu_ns=elapsed))
summary = {}
for r in runs:
    key = f"{r['rules']}/{r['workload']}"
    values = [v['cpu_ns'] / v['calls'] for v in runs if f"{v['rules']}/{v['workload']}" == key]
    summary[key] = dict(median_ns=statistics.median(values), min_ns=min(values), max_ns=max(values))
result = dict(scope='Linux native C -O2 installed style resolution; updates include installation and 13 queries; install includes ctypes call overhead. RSS is whole Python process peak, not library-only.',
              warmups=1, samples=7, peak_rss_kib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
              benchmark_sha256=hashlib.sha256(Path(__file__).with_suffix('.zi').read_bytes()).hexdigest(),
              runs=runs, summary=summary)
destination = os.environ.get('KRYON_STYLE_COST_OUTPUT')
if destination: Path(destination).write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(summary=summary, peak_rss_kib=result['peak_rss_kib']), indent=2))
