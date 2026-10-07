# Repeatable performance checks

`python3 tools/performance.py` builds an optimized native, display-free runner
and measures four workloads at 960 × 600 pixels: a scrolling 10,000-item list,
an 8 KiB wrapped Unicode document, 144 images, and 20 idle controls. Each has
10 warmup frames followed by 60 measured frames. Reported values include
submission, combined commit/raster and total median/p95 times, nodes, paint
commands, raster emissions, damage pixels, workers and runner peak RSS.
The idle workload must emit zero unchanged paint commands.

Use a same-machine baseline for a regression gate:

```sh
python3 tools/performance.py --output backup/performance-baseline.json
python3 tools/performance.py --baseline backup/performance-baseline.json --max-regression 0.20
```

Comparison rejects mismatched hardware, compiler command, optimization,
viewport, warmup or requested worker count. A p95 regression fails when it
exceeds the percentage and adds more than 0.2 ms. `--budget-ms 64` provides an
absolute p95 ceiling for every workload; CI uses this generous ceiling to
catch large regressions across shared runners. Local baselines give a tighter
check on fixed hardware. The JSON retains check results and failures.

The build directory is stable and locked for the whole run. Desktop variables
are scrubbed. RSS measures the runner, excluding compilation. The timings
cover the pixmap pipeline, not native window presentation, asset decoding,
screen-reader overhead, process idle wakeups or GPU/browser rendering.
