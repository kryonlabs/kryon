"""Native style-resolution cost only; no application or timing pass threshold."""
import ctypes
import json
import os
from pathlib import Path
import statistics
import time

lib = ctypes.CDLL(os.environ['KRYON_STYLE_COST_LIBRARY'])
lib.Cost.argtypes = [ctypes.c_bool, ctypes.c_int32, ctypes.c_int32]
lib.Cost.restype = ctypes.c_uint64
calls = 25000
runs = []
for count in (8, 32, 320):
    expected = None
    for index, cached in enumerate((False, True, True, False, True, False, False, True)):
        start = time.process_time_ns()
        digest = lib.Cost(cached, calls, count)
        elapsed = time.process_time_ns() - start
        assert digest != 0
        if expected is None: expected = digest
        assert digest == expected, 'Cached results differ from fresh cascades'
        runs.append(dict(rules=count, index=index, cached=cached, calls=calls,
                         cpu_ns=elapsed, digest=digest))
summary = {}
for count in (8, 32, 320):
    means = {mode: statistics.median(r['cpu_ns'] for r in runs
                                    if r['rules'] == count and r['cached'] == mode)
             for mode in (False, True)}
    summary[count] = dict(fresh_cpu_ns=means[False], cached_cpu_ns=means[True],
                          ratio=means[False] / means[True])
result = dict(scope='Linux native style resolution only; not whole-application responsiveness or RSS.',
              order='ABBA then BAAB', calls_per_run=calls, all_digests_equal=True,
              runs=runs, summary=summary)
destination = os.environ.get('KRYON_STYLE_COST_OUTPUT')
if destination: Path(destination).write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(summary, indent=2))
