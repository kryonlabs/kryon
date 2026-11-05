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

The widget review harness measures actual public controls, rather than mock
faces. It includes all 54 core gallery scenes plus light and dark system-palette
compositions. Each interactive scene captures resting, disabled, hover, keyboard
focus and held-press states, using the same 480 × 320 viewport.

```sh
build-queue -- taskset -c 2 python3 tools/widget-review.py before --backend pixmap
build-queue -- taskset -c 2 python3 tools/widget-review.py before --backend desktop
# Change the shared library only after both baselines finish.
build-queue -- taskset -c 2 python3 tools/widget-review.py after --backend pixmap
build-queue -- taskset -c 2 python3 tools/widget-review.py after --backend desktop
build-queue -- taskset -c 2 python3 tools/widget-review.py compare --backend pixmap
build-queue -- taskset -c 2 python3 tools/widget-review.py compare --backend desktop
build-queue -- taskset -c 2 python3 tools/widget-review.py recheck --backend pixmap --trials 7
build-queue -- taskset -c 2 python3 tools/widget-review.py recheck --backend desktop --trials 7
```

The stable executables and disposable generated C live under
`build/scratch/widget-review/`. Captures, bounded per-frame CSV, JSON metrics and
comparison PNGs live under ignored `backup/material-widgets/`. Each backend has
its own `comparison-BACKEND/grid-overview.png` and full-size state comparisons.
A second baseline replaces the previous baseline, so preserve any earlier study
before starting another. Builds must run through the shared build queue.

Use `--output-dir backup/material-widgets/navbar` or
`--output-dir backup/material-widgets/perf` for an independent study; this also
selects that study's fixture image. `--scenes 20` limits a capture to the
NavigationBar. Keep previous studies in place instead of reusing their output
directory. The normal executable paths stay stable.

Cairo retains up to eight owned Pango layouts for alternating labels and font
sizes; long editor runs share one slot. Layouts are released when the raster
closes or resizes. Retained paint text uses 512-byte snapshot blocks, so a short
frame initially reserves 4 KiB instead of 32 KiB while larger frames still grow
without truncation. Pixmap fills opaque spans in bounded groups of four pixels;
the translucent rounding and coverage remain unchanged. Validate memory with
both peak heap allocation and RSS: small heap savings need not reduce the
process's resident high-water mark on every run.

Both hosts run 10 warmup and 120 measured frames for **each** state and mode.
Retained mode tests that unchanged frames emit no raster commands. Forced repaint
invalidates the paint snapshot before every frame and includes widget composition,
commit, rasterization and, for desktop, SDL texture upload/presentation. Timings
include monotonic wall time and process CPU time. They are CPU-side measurements,
not GPU timestamps or input latency. The desktop uses SDL's software presentation
on private Xvfb; the pixmap uses its actual CPU raster and bundled font outlines.
The fixture image is identical on both hosts.

Runner `/proc/self/status` supplies peak and steady RSS after warmup/measurement;
it excludes the Python launcher. Steady RSS can exceed an earlier high-water
sample by a page when the status reader itself faults a page. Startup is the
in-process interval from runner entry through its first complete frame, including
host setup and font work, but excluding the OS executable loader and compilation.
Binary size and its SHA-256 are recorded with the samples. The text reservation
counter covers reusable frame-owned label/path storage, not total memory.
The comparison JSON gives medians across scene/state cases and the largest
observed process high-water RSS across both modes. It flags per-case timing
increases above both 5% and 0.025 ms for investigation; repeat flagged cases to
distinguish reproducible costs from shared-machine variation. `recheck` runs each
flagged case three more times with the recorded after executable (verified by
SHA-256), preserves the initial samples, and fails for a confirmed increase in
the median of the repeats. Raw repeats and their summary stay beside the captures.
Use `recheck --backend desktop --trials 7` for a longer investigation on a noisy
shared machine; this keeps the same threshold and records all seven trials.

For close comparisons pin both phases to the same single allowed CPU core, as
above, to avoid migrations between differently loaded cores.

Idle CPU covers a 200 ms quiet interval with five 40 ms sleeps and, on desktop,
its ordinary event poll; no frames are submitted in that interval. Together with
zero retained raster emissions this tests the harness's idle behavior, rather
than claiming that every downstream application's event loop sleeps. No renderer
thread, timer or animation is added by the default style. CPU affinity is recorded.
All captures use private Xvfb with inherited display/auth/session variables scrubbed
and desktop recovery disabled. This does not measure Android hardware, a physical
GPU, a desktop compositor, browser behavior or terminal-cell fidelity.
