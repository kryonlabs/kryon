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
