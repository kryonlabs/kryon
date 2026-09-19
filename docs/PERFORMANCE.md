# Runtime Workloads

Run `make perf-frame-workloads` to measure native Go CPU-side frame construction.
The workloads cover 24 wrapped multilingual paragraphs, a 128-field form, and
navigation in a 500-line multilingual editor. Each reports time, allocation
counts, allocated bytes, and recorded operations per frame. Font loading and
the first frame are excluded; these are warm, repeated-workload measurements.
They do not measure rasterization, GPU submission, presentation, or input-to-display
latency. Use the existing `make perf-text-input` harness for native editing
latency distributions and its environment-specific regression gates.

## Bounded Text Layout Cache

Native Go `Text` caches wrapped lines and their measured widths per runtime.
The key includes the complete text, available width, resolved font size, font
identity, and letter spacing. Registering/replacing a font or changing the active
font invalidates the cache. Layout policy still comes from `runtime/paragraph.kry`;
the cache only retains its measured result. Painting, clipping, color, selection,
and alignment continue to run each frame.

FIFO eviction limits each runtime to 64 entries and 1 MiB of accounted text/line
storage, plus bounded map/entry overhead. Text exceeding 64 KiB is not retained.
Cached source and line strings are copied so small substrings cannot retain a
large caller-owned document. Tests check cold/warm equivalence, every key field,
font replacement with the same ID, and repeated memory-budget eviction.

## Recorded Comparison

Measured on 2026-09-19, Linux amd64, AMD Ryzen 9 9950X, using three runs of 100
frames. The table gives the median per-frame mean from those runs. The baseline
is the Unicode-editing milestone (`af4c0907`) with the workload test added, before
the cache. These values are evidence for this workload, not portable timing gates.

| Workload | Before | Cached | Allocations Before / After | Bytes Before / After |
| --- | ---: | ---: | ---: | ---: |
| Wrapped document | 1.607 ms | 0.013 ms | 13,540 / 4 | 343,329 / 432 |
| 128-field form | 0.121 ms | 0.116 ms | 132 / 132 | 2,704 / 2,704 |
| Multilingual editor | 0.016 ms | 0.015 ms | 8 / 8 | 41,504 / 41,504 |

The document improvement comes from avoiding repeat layout and measurement of
unchanged paragraphs. The form and editable-buffer paths do not use this cache;
their small timing differences are within run-to-run variation. First-frame,
constantly changing text, and cache-thrashing workloads still pay layout cost.

## Bounded Surface Raster Cache

The native Go renderer caches sampled material layers, including rounded panel
fills and borders. Sampling policy remains in the shared Kryon runtime; cached
opaque spans use row copies, and translucent spans retain the same source-over
blending and rounding as uncached rendering. The key includes the complete
surface command and pixel clip, so changes to position, scale, colors, segments,
or material parameters cannot reuse an incompatible raster.

An LRU retains at most 128 entries and 32 MiB of accounted pixel/span storage,
plus bounded map/entry overhead. Entries over 8 MiB are not retained. Cold frames,
resizing, and changing material parameters still incur sampling work.

On the same machine and date as above, a synthetic 1600×900 dashboard with
rounded panels and 30 recommendations took 250 ms per warm repaint before the
cache and a median 4.85 ms afterward (three runs of 30 frames). This includes
frame construction and CPU rasterization, but excludes presentation and network
requests. Pixel comparisons cover flat, lightfield, and glass materials with
clipping, fractional coordinates, scaling, gradients, and translucent targets.
