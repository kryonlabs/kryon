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
are scrubbed. RSS comes from the native runner's `/proc/self/status` VmHWM
after the measured frames. Unlike a launcher's `wait4` high-water mark, it
excludes the Python process's memory before exec. Schema 2 baselines use this
measurement; capture a new baseline when migrating from schema 1. The timings
exclude warmup; the runner CPU-seconds counter includes initialization and all
warmup/measured frames, and excludes compilation and the Python launcher. Timings
cover the pixmap pipeline, not native window presentation, asset decoding,
screen-reader overhead, process idle wakeups or GPU/browser rendering.

`python3 tools/widget-review.py before` captures actual core widgets and their
resting, disabled, pointer-hover and keyboard-focus states where applicable.
Run it before changing the library, then run `after` and `compare`. Each build
preserves its own executable under `build/scratch/widget-review/`; captures,
per-widget CSV/JSON measurements and labeled comparison PNGs go to
`backup/widget-improvement/`. Open that folder's `comparison/` in Gwenview.
Capture uses private Xvfb displays with inherited desktop variables scrubbed.
The per-widget timings cover 10 warmup and 60 measured idle frames at 480 × 320;
they complement the active workloads above rather than measuring input latency.
The text-pool reservation counter covers frame-owned label/path storage only,
not total application memory. Containers and nonvisual helpers are shown with
children; image and shape pixels are preserved by the renderer optimization.
