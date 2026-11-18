# Shader performance — 2026-11-17

The benchmark covers eight shader workloads at 640×360 and 1920×1080 and
four animated scene fixtures. The baseline and follow-up reports record
complete frame timings, mean, p95, output hashes and GPU phase timings.

## Workloads

The maintained programs are in `tests/shader_benchmark_workloads.zi`, using
Kryon's `ShaderCompile` instruction API. These are original implementations of
standard mathematical formulas; no third-party shader source is incorporated.
All programs fit the public 128-instruction, 32-register API.

| Workload | Fixed work |
|---|---|
| UV | UV mapped to RGB, opaque alpha |
| Plasma | Directional sinusoidal waves, scaled and offset into colour |
| Value noise fbm | Three octaves (8, 16, 32 cells), Hermite interpolation of four deterministic lattice samples; weights 4/7, 2/7, 1/7 |
| SDF raymarch | Eight sphere-tracing steps, radius 0.65; continuous final-distance visibility |
| Gaussian blur | Nine horizontal texture taps, normalized Gaussian weights, sigma 2 texels |
| CRT | Texture sampling, sinusoidal scanline attenuation, radial vignette |
| Ripple | Radial travelling sinusoid displacing a texture lookup |
| Particles | Eight animated inverse-quadratic splats |

The noise/blur/CRT/ripple input is a deterministic 256×256 grayscale texture.
Gaussian blur is one horizontal pass, not a complete two-pass bloom pipeline.
Sphere tracing uses a fixed iteration budget without early termination. These
choices are held fixed between baseline and optimized runs.

## Measurement contract

Build on the development host with `build-queue -- make shader-bench-build`.
Add `SHADER_BENCH_ARGS=--prepared` to benchmark the batched interpreter.
Set `SHADER_BENCH_DEVICE=moto` or `pixel` for Android NDK builds. Copy the desktop
binary and its `.build.json` provenance to the physical benchmark host; do not
build a second source tree for desktop timings.

Run `build-queue -- make shader-bench` with `SHADER_BENCH_MODE=interpreter`,
`scene-cpu`, or `scene-gpu`. Desktop timing is refused on any host other than
the designated physical benchmark host. Android runs select the exact serial;
the Pixel is reached through the explicitly selected ThinkPad ADB host.

Each workload runs at 640×360 and 1920×1080, with eight excluded warmup frames
and 60 measured frames. Mean and nearest-rank p95 are reported in ms. Compile,
construction and output hashes are outside frame timing. Measured frames use
identical time values on every run; warmup uses the same sequence from its
beginning. Raw per-frame records and generated-C fingerprints are retained.
Reports also record the program's instruction count and compile time, and
matched comparisons record speedups for both mean and p95. The build records
its source revision and whether the checkout has local changes.

For the visual suite, set `SHADER_BENCH_ARGS="--suite visual"` and select
`SHADER_BENCH_MODE=scene-cpu` or `scene-gpu`. It uses the same warmup,
resolutions, clocks and per-renderer RGBA frame hashes. The four procedural
fixtures cover lit meshes, an articulated character with weighted joint
deformation, a textured room and transparent particle sorting. Scene
construction stays outside timing, while camera, skin and particle updates
run inside the frame. Visual JSON reports have `suite: visual`. The default
shader suite retains its existing names, command line and result fields.

`interpreter` measures one `ShaderRun` per pixel including input construction
and RGBA8 conversion, with no rasterizer or presentation. `scene-cpu` measures
`RenderScene`, including its four samples per pixel and resolve. `scene-gpu`
measures strict `SceneGpuRender`, including context handling, retained-cache
maintenance, rendering, readback and conversion. These are separate workloads,
not interchangeable speedup denominators. GPU phase timings are owner-thread
wall times; readback includes the GPU completion wait, not just transfer time.

The scene GPU renderer requires EGL/GLES3, highp, four-sample MSAA and
15 texture units. The Android path now uses `eglGetDisplay` and GLESv3 without
GLX or the newer `eglGetPlatformDisplay`. Both armv7 and aarch64 benchmark
executables link against API21. The API21-built executable passed the Pixel
smoke and full GPU matrix on Adreno 618, OpenGL ES 3.2 V@0490.0. The physical
phone runs a newer Android version; this is no claim of testing an API21 OS.
The Android CMake helper links EGL/GLESv3. The API21 NativeActivity link
regression covers both ABIs.
Web currently uses the CPU scene renderer. The SDL2/SDL3 pixmap GPU
backends draw atlas/image geometry and do not execute `ShaderProgram` directly.
Host presentation measurements still need integration; no SDL, Wine, browser,
or mobile GPU number may be inferred from the interpreter or GLES result.

For hardware desktop timings, use the approved native application launch route
and verify the returned renderer identity. A shell or private-Xvfb software GL
run is a correctness test, not hardware evidence. The collector rejects known
software renderer identities. Development-host graphics tests use private
Xvfb with display/session variables scrubbed and recovery disabled.

The desktop GPU run uses headless EGL with a verified NVIDIA GeForce
RTX 5060 Ti, OpenGL ES 3.2 NVIDIA 595.99.02. All eight shaders completed the
8×8 GPU smoke before the timed run.

## Baseline

### Android armv7 interpreter

Eight excluded warmups; 60 measured frames. Times in ms.

| Shader | Resolution | Mean | p95 |
|---|---|---:|---:|
| uv | 640x360 | 130.219 | 132.332 |
| plasma | 640x360 | 414.881 | 437.235 |
| value-noise-fbm | 640x360 | 1776.217 | 1800.787 |
| sdf-raymarch | 640x360 | 811.168 | 816.058 |
| gaussian-blur | 640x360 | 1067.807 | 1068.630 |
| crt | 640x360 | 411.519 | 414.196 |
| ripple | 640x360 | 405.226 | 412.065 |
| particles | 640x360 | 1759.924 | 1917.126 |
| uv | 1920x1080 | 1159.770 | 1167.279 |
| plasma | 1920x1080 | 3696.665 | 3870.728 |
| value-noise-fbm | 1920x1080 | 15788.198 | 15811.106 |
| sdf-raymarch | 1920x1080 | 7296.176 | 7307.827 |
| gaussian-blur | 1920x1080 | 9606.541 | 9617.873 |
| crt | 1920x1080 | 3700.086 | 3705.079 |
| ripple | 1920x1080 | 3616.021 | 3629.424 |
| particles | 1920x1080 | 16344.677 | 17988.428 |

[Raw records](shader-performance/baseline-moto-interpreter.log) · [Report](shader-performance/baseline-moto-interpreter.json)

### Android aarch64 interpreter

Eight excluded warmups; 60 measured frames. Times in ms.

| Shader | Resolution | Mean | p95 |
|---|---|---:|---:|
| uv | 640x360 | 46.986 | 47.133 |
| plasma | 640x360 | 148.805 | 155.445 |
| value-noise-fbm | 640x360 | 596.989 | 598.170 |
| sdf-raymarch | 640x360 | 294.661 | 295.125 |
| gaussian-blur | 640x360 | 385.627 | 386.268 |
| crt | 640x360 | 139.730 | 141.154 |
| ripple | 640x360 | 149.221 | 149.431 |
| particles | 640x360 | 620.849 | 679.798 |
| uv | 1920x1080 | 395.912 | 396.451 |
| plasma | 1920x1080 | 1311.197 | 1372.556 |
| value-noise-fbm | 1920x1080 | 5350.014 | 5407.636 |
| sdf-raymarch | 1920x1080 | 2619.504 | 2625.547 |
| gaussian-blur | 1920x1080 | 3446.661 | 3449.992 |
| crt | 1920x1080 | 1261.121 | 1264.612 |
| ripple | 1920x1080 | 1322.627 | 1326.279 |
| particles | 1920x1080 | 5516.638 | 6054.591 |

[Raw records](shader-performance/baseline-pixel-interpreter.log) · [Report](shader-performance/baseline-pixel-interpreter.json)

### Desktop interpreter

Eight excluded warmups; 60 measured frames. Times in ms.

| Shader | Resolution | Mean | p95 |
|---|---|---:|---:|
| uv | 640x360 | 12.432 | 12.764 |
| plasma | 640x360 | 41.027 | 44.358 |
| value-noise-fbm | 640x360 | 146.661 | 154.633 |
| sdf-raymarch | 640x360 | 94.074 | 95.357 |
| gaussian-blur | 640x360 | 83.648 | 85.732 |
| crt | 640x360 | 39.556 | 40.872 |
| ripple | 640x360 | 37.687 | 38.576 |
| particles | 640x360 | 170.981 | 188.499 |
| uv | 1920x1080 | 110.331 | 114.402 |
| plasma | 1920x1080 | 373.088 | 393.966 |
| value-noise-fbm | 1920x1080 | 1325.481 | 1367.129 |
| sdf-raymarch | 1920x1080 | 842.775 | 849.350 |
| gaussian-blur | 1920x1080 | 760.222 | 767.678 |
| crt | 1920x1080 | 364.270 | 367.208 |
| ripple | 1920x1080 | 337.356 | 346.837 |
| particles | 1920x1080 | 1586.460 | 1710.965 |

[Raw records](shader-performance/baseline-desktop-interpreter.log) · [Report](shader-performance/baseline-desktop-interpreter.json)

### Desktop hardware GLES scene

Eight excluded warmups; 60 measured frames. Times in ms.

| Shader | Resolution | Mean | p95 |
|---|---|---:|---:|
| uv | 640x360 | 0.476 | 0.499 |
| plasma | 640x360 | 0.461 | 0.477 |
| value-noise-fbm | 640x360 | 0.481 | 0.490 |
| sdf-raymarch | 640x360 | 0.460 | 0.473 |
| gaussian-blur | 640x360 | 0.492 | 0.500 |
| crt | 640x360 | 0.466 | 0.471 |
| ripple | 640x360 | 0.479 | 0.511 |
| particles | 640x360 | 0.459 | 0.468 |
| uv | 1920x1080 | 3.211 | 3.394 |
| plasma | 1920x1080 | 3.225 | 3.456 |
| value-noise-fbm | 1920x1080 | 3.309 | 3.525 |
| sdf-raymarch | 1920x1080 | 3.410 | 3.874 |
| gaussian-blur | 1920x1080 | 3.354 | 3.587 |
| crt | 1920x1080 | 3.221 | 3.335 |
| ripple | 1920x1080 | 3.237 | 3.494 |
| particles | 1920x1080 | 3.298 | 3.446 |

[Raw records](shader-performance/baseline-desktop-scene-gpu.log) · [Report](shader-performance/baseline-desktop-scene-gpu.json)

Renderer: identity true NVIDIA GeForce RTX 5060 Ti/PCIe/SSE2 OpenGL ES 3.2 NVIDIA 595.99.02

### Desktop CPU scene

Eight excluded warmups; 60 measured frames. Times in ms.

| Shader | Resolution | Mean | p95 |
|---|---|---:|---:|
| uv | 640x360 | 9.207 | 9.636 |
| plasma | 640x360 | 16.835 | 18.535 |
| value-noise-fbm | 640x360 | 44.193 | 44.628 |
| sdf-raymarch | 640x360 | 30.099 | 31.792 |
| gaussian-blur | 640x360 | 27.373 | 28.140 |
| crt | 640x360 | 16.222 | 17.347 |
| ripple | 640x360 | 15.710 | 16.599 |
| particles | 640x360 | 50.749 | 54.958 |
| uv | 1920x1080 | 82.636 | 85.478 |
| plasma | 1920x1080 | 146.451 | 150.903 |
| value-noise-fbm | 1920x1080 | 401.018 | 407.675 |
| sdf-raymarch | 1920x1080 | 269.511 | 272.339 |
| gaussian-blur | 1920x1080 | 251.783 | 293.214 |
| crt | 1920x1080 | 146.955 | 166.659 |
| ripple | 1920x1080 | 140.911 | 142.532 |
| particles | 1920x1080 | 457.404 | 491.198 |

[Raw records](shader-performance/baseline-desktop-scene-cpu.log) · [Report](shader-performance/baseline-desktop-scene-cpu.json)

### Browser Wasm interpreter

Headless Chromium, eight excluded warmups and 60 measured frames. Times in ms.
The browser clock has 0.1 ms granularity; these full frames exceed that interval.

| Shader | Resolution | Mean | p95 |
|---|---|---:|---:|
| uv | 640x360 | 19.457 | 21.500 |
| plasma | 640x360 | 62.392 | 65.700 |
| value-noise-fbm | 640x360 | 242.305 | 247.500 |
| sdf-raymarch | 640x360 | 140.973 | 143.200 |
| gaussian-blur | 640x360 | 144.845 | 148.100 |
| crt | 640x360 | 58.525 | 59.600 |
| ripple | 640x360 | 58.530 | 59.600 |
| particles | 640x360 | 297.840 | 327.000 |
| uv | 1920x1080 | 179.222 | 181.200 |
| plasma | 1920x1080 | 567.148 | 591.700 |
| value-noise-fbm | 1920x1080 | 2215.667 | 2239.800 |
| sdf-raymarch | 1920x1080 | 1268.973 | 1277.800 |
| gaussian-blur | 1920x1080 | 1302.627 | 1312.500 |
| crt | 1920x1080 | 528.292 | 536.500 |
| ripple | 1920x1080 | 524.868 | 530.100 |
| particles | 1920x1080 | 2710.798 | 2941.500 |

[Raw records](shader-performance/baseline-browser-interpreter.log) · [Report](shader-performance/baseline-browser-interpreter.json)

### Browser Wasm CPU scene

Headless Chromium, eight excluded warmups and 60 measured frames. Times in ms.

| Shader | Resolution | Mean | p95 |
|---|---|---:|---:|
| uv | 640x360 | 62.112 | 62.800 |
| plasma | 640x360 | 106.352 | 108.800 |
| value-noise-fbm | 640x360 | 291.583 | 294.600 |
| sdf-raymarch | 640x360 | 185.063 | 186.400 |
| gaussian-blur | 640x360 | 191.433 | 193.700 |
| crt | 640x360 | 104.313 | 105.800 |
| ripple | 640x360 | 104.290 | 106.000 |
| particles | 640x360 | 349.517 | 370.800 |
| uv | 1920x1080 | 554.882 | 558.000 |
| plasma | 1920x1080 | 955.493 | 984.700 |
| value-noise-fbm | 1920x1080 | 2623.547 | 2655.700 |
| sdf-raymarch | 1920x1080 | 1686.467 | 1695.300 |
| gaussian-blur | 1920x1080 | 1723.902 | 1743.700 |
| crt | 1920x1080 | 942.432 | 948.200 |
| ripple | 1920x1080 | 942.378 | 949.600 |
| particles | 1920x1080 | 3178.005 | 3371.300 |

[Raw records](shader-performance/baseline-browser-scene-cpu.log) · [Report](shader-performance/baseline-browser-scene-cpu.json)

### Windows under Wine interpreter

Eight excluded warmups; 60 measured frames at each resolution.

| Shader | Resolution | Mean (ms) | p95 (ms) |
|---|---|---:|---:|
| uv | 640x360 | 18.387 | 18.501 |
| plasma | 640x360 | 61.350 | 62.853 |
| value-noise-fbm | 640x360 | 262.353 | 267.309 |
| sdf-raymarch | 640x360 | 179.343 | 183.593 |
| gaussian-blur | 640x360 | 155.510 | 159.515 |
| crt | 640x360 | 59.055 | 60.302 |
| ripple | 640x360 | 57.524 | 58.971 |
| particles | 640x360 | 347.770 | 365.469 |
| uv | 1920x1080 | 165.579 | 170.300 |
| plasma | 1920x1080 | 561.586 | 593.186 |
| value-noise-fbm | 1920x1080 | 2448.866 | 2510.789 |
| sdf-raymarch | 1920x1080 | 1627.948 | 1670.275 |
| gaussian-blur | 1920x1080 | 1403.808 | 1421.022 |
| crt | 1920x1080 | 530.253 | 539.159 |
| ripple | 1920x1080 | 518.728 | 527.622 |
| particles | 1920x1080 | 3133.734 | 3290.426 |

Report: `shader-performance/baseline-wine-interpreter.json`; raw frames: `baseline-wine-interpreter.log`.

### Windows under Wine CPU scene

Eight excluded warmups; 60 measured frames. This build has no EGL scene path.

| Shader | Resolution | Mean (ms) | p95 (ms) |
|---|---|---:|---:|
| uv | 640x360 | 10.576 | 10.911 |
| plasma | 640x360 | 21.663 | 22.171 |
| value-noise-fbm | 640x360 | 75.427 | 85.321 |
| sdf-raymarch | 640x360 | 53.386 | 60.819 |
| gaussian-blur | 640x360 | 46.581 | 47.492 |
| crt | 640x360 | 22.051 | 26.331 |
| ripple | 640x360 | 20.884 | 21.888 |
| particles | 640x360 | 95.297 | 100.013 |
| uv | 1920x1080 | 96.040 | 109.591 |
| plasma | 1920x1080 | 320.509 | 364.470 |
| value-noise-fbm | 1920x1080 | 705.398 | 829.878 |
| sdf-raymarch | 1920x1080 | 546.396 | 636.566 |
| gaussian-blur | 1920x1080 | 447.050 | 487.805 |
| crt | 1920x1080 | 210.295 | 259.794 |
| ripple | 1920x1080 | 218.938 | 376.925 |
| particles | 1920x1080 | 903.244 | 1075.316 |

Report: `shader-performance/baseline-wine-scene-cpu.json`; raw frames: `baseline-wine-scene-cpu.log`.

### Android aarch64 GPU, newly enabled path

Eight excluded warmups; 60 measured frames. Adreno 618 renderer identity verified.
This is a distinct path, not an interpreter speedup denominator. Pixel accuracy
against the CPU scene reference is still being tested.

| Shader | Resolution | Mean (ms) | p95 (ms) |
|---|---|---:|---:|
| uv | 640x360 | 4.082 | 6.489 |
| plasma | 640x360 | 5.078 | 7.474 |
| value-noise-fbm | 640x360 | 10.473 | 12.024 |
| sdf-raymarch | 640x360 | 7.772 | 10.146 |
| gaussian-blur | 640x360 | 6.962 | 10.045 |
| crt | 640x360 | 5.048 | 8.123 |
| ripple | 640x360 | 5.153 | 7.155 |
| particles | 640x360 | 6.658 | 8.933 |
| uv | 1920x1080 | 15.338 | 22.043 |
| plasma | 1920x1080 | 17.538 | 21.152 |
| value-noise-fbm | 1920x1080 | 54.511 | 92.120 |
| sdf-raymarch | 1920x1080 | 61.084 | 62.809 |
| gaussian-blur | 1920x1080 | 82.469 | 87.279 |
| crt | 1920x1080 | 34.850 | 36.536 |
| ripple | 1920x1080 | 36.377 | 38.475 |
| particles | 1920x1080 | 49.264 | 51.608 |

Report: `shader-performance/pixel-scene-gpu.json`; raw frames: `pixel-scene-gpu.log`.

## Profiles and optimization decisions

GPU phase timings are saved in the hardware report. At 1920×1080, readback
is 1.35–1.46 ms and conversion 1.73–1.87 ms, compared with 0.04–0.07 ms
draw submission. Hardware-counter sampled profiles could not be collected: the benchmark host
requires authentication for perf (perf_event_paranoid=4; sudo -n refused).
An instrumented gprof build completed on the development host and ran each
workload separately on the physical desktop benchmark host at 640×360.
The uninstrumented executable produced the baseline timing tables. Instrumented
profiles are diagnostic only and cannot be used as performance denominators.

| Shader | ShaderRun self time | Other large self-time contributors |
|---|---:|---|
| UV | 73.69% | Output conversion and kernel loop |
| Plasma | 61.54% | Sin 28.85% |
| Value noise fbm | 59.65% | ShaderSample 14.04% |
| SDF raymarch | 93.70% | Interpreter dispatch/vector operations |
| Gaussian blur | 51.61% | ShaderSample 15.05% |
| CRT | 45.46% | Sin 9.09%, ShaderSample 7.27% |
| Ripple | 48.72% | Sin 20.51%, ShaderSample 7.69% |
| Particles | 67.49% | Sin 26.11% |

Flat profiles are retained as `shader-performance/profile-0.txt` through
`profile-7.txt`. These are desktop profiles; no phone CPU profile is claimed.
The first CPU optimization validates bindings and folds draw constants once,
removes dead dynamic instructions, and reuses per-lane register scratch rather
than zeroing registers and validating bindings at every pixel. The existing
`ShaderRun` stays as the parity oracle. The scene renderer prepares each mesh's
shader once per frame, so uniform, program and sampler changes remain visible.
Preparation stays inside the timed frame. The first GLES optimization skips
per-pixel unpremultiplication work in groups of eight known-opaque pixels;
transparent pixels retain the original conversion.

Record per-shader compile cost, instruction
count, full-frame mean/p95 and GPU prepare/draw/readback/conversion costs.
CPU profiles must come from the benchmark host or the phones. An instruction
count is workload metadata, not a sampled CPU profile.

## Rejected first CPU attempt

The first prepared executor retained every SSA version in per-pixel scratch.
The full armv7 640x360 set completed with all 60 frame hashes matching baseline,
but six workloads regressed. The longer 1080p run was stopped; it is incomplete.
These are diagnostic results, not a successful full-matrix optimization.

| Shader | Baseline mean ms | Prepared v1 mean ms | Speedup |
|---|---:|---:|---:|
| UV | 130.219 | 98.036 | 1.33x |
| Plasma | 414.881 | 450.986 | 0.92x |
| Value noise fbm | 1776.217 | 2130.745 | 0.83x |
| SDF raymarch | 811.168 | 1163.396 | 0.70x |
| Gaussian blur | 1067.807 | 1248.520 | 0.86x |
| CRT | 411.519 | 429.636 | 0.96x |
| Ripple | 405.226 | 445.568 | 0.91x |
| Particles | 1759.924 | 1128.268 | 1.56x |

The next implementation retains SSA only during liveness analysis and remaps
execution to the original 32 physical registers. This reduces per-lane scratch
from 4224 to 1024 bytes without changing arithmetic. Future phone run-only collectors use the
queue from the Projects directory, with explicit project paths and no builds
of those phone artifacts during collection; this retains resource accounting
without holding the source checkout throughout remote execution.

## Validation

- `build-queue -- ziran check --project tests/shader_scene_test.zi`: passed
  before implementation.
- `build-queue -- ziran check --project --define SHADER_BENCH_GPU tests/shader_benchmark.zi`: passed.
- `build-queue -- make shader-bench-test`: passed after fixing the particle
  instruction budget; all eight programs finish, UV matches an independent
  pixel oracle, plasma and particles animate. Statistics tests reject missing,
  duplicate, negative-duration and software-GPU records.
- Android NDK builds for armv7 and aarch64: passed. Both physical phones passed
  the same eight-program smoke and independent UV/animation checks.
- Alpha hardware GLES scene smoke: passed for all eight programs at 8×8.
- `build-queue -- make shader-prepared-test shader-bench-test scene-gpu-test shader-scene-test shader-glsl-test SHADER_BENCH_ARGS=--prepared`: passed before CPU scene integration; native/portable prepared parity, scene GPU synchronous/asynchronous/camera fixtures, scene shader suite and private GLES GLSL pixel oracle passed.
- CPU scene integration: `ziran check`, source/saved native+portable scene tests, scene GPU/async/camera tests and prepared benchmark smoke passed after limiting prepared-plan allocation headroom. The latest mixed-alpha conversion block also passed scene GPU tests.

## Remaining work and measurement provenance

All desktop timing runs use the designated physical benchmark host; builds and
correctness tests use the development host. Remote artifacts live only in the
owning project's `build/scratch/shader-benchmark/`; the remote canonical sources
were not changed. The run-only collector uses `KRYON_SHADER_BENCH_ROOT`.

The shared toolchain changed between some runs. Earlier reports preserve
binary and generated-C fingerprints. Later reports add the Ziran executable
and lock hashes and the C compiler version. This limits attribution across
revisions. Phone CPU sampled profiles and SDL presentation timings remain
unverified. The GLES phase timings include the GPU completion wait, so they
establish the measured cost of the current readback path.

Rejected armv7 prepared-v1 collection was stopped deliberately after all
640x360 groups and part of 1920x1080. Its partial raw log is retained as
`diagnostic-prepared-v1-moto-interpreter.log`; it is not a complete matrix.

## GPU conversion comparison

Eight excluded warmups and 60 measured frames per row. GPU-side vertical flip
and exact integer unpremultiplication followed by one CPU copy. Every raw frame
hash matches the original hardware GLES baseline. This is complete-frame wall
time including readback; no 10x result is claimed.

| Shader | Resolution | Before mean ms | After mean ms | After p95 ms | Speedup |
|---|---|---:|---:|---:|---:|
| uv | 640x360 | 0.476 | 0.332 | 0.367 | 1.434x |
| plasma | 640x360 | 0.461 | 0.324 | 0.334 | 1.423x |
| value-noise-fbm | 640x360 | 0.481 | 0.348 | 0.378 | 1.383x |
| sdf-raymarch | 640x360 | 0.460 | 0.329 | 0.352 | 1.400x |
| gaussian-blur | 640x360 | 0.492 | 0.366 | 0.367 | 1.346x |
| crt | 640x360 | 0.466 | 0.362 | 0.447 | 1.285x |
| ripple | 640x360 | 0.479 | 0.340 | 0.349 | 1.410x |
| particles | 640x360 | 0.459 | 0.328 | 0.345 | 1.402x |
| uv | 1920x1080 | 3.211 | 2.205 | 2.423 | 1.457x |
| plasma | 1920x1080 | 3.225 | 2.199 | 2.309 | 1.467x |
| value-noise-fbm | 1920x1080 | 3.309 | 2.382 | 2.570 | 1.389x |
| sdf-raymarch | 1920x1080 | 3.410 | 2.254 | 2.359 | 1.513x |
| gaussian-blur | 1920x1080 | 3.354 | 2.280 | 2.355 | 1.471x |
| crt | 1920x1080 | 3.221 | 2.226 | 2.404 | 1.447x |
| ripple | 1920x1080 | 3.237 | 2.250 | 2.422 | 1.439x |
| particles | 1920x1080 | 3.298 | 2.232 | 2.305 | 1.477x |

[Raw records](shader-performance/straight-gpu-desktop-scene-gpu.log) · [Report](shader-performance/straight-gpu-desktop-scene-gpu.json)

At 1080p the readback phase is 1.362–1.490 ms, including GPU completion;
conversion submission plus the CPU copy is 0.689–0.721 ms. The remaining transfer
and completion time must be measured separately before asserting a physical
limit. The previous opaque-block CPU conversion revision was 1.17–1.33x, retained
as `prepared-desktop-scene-gpu.json`.

## Rejected full desktop CPU revision

The first prepared evaluator used one 132-value SSA scratch array per execution
lane. All before/after hashes match, but dispatch and scratch traffic regressed
six workloads. This revision is rejected for integration.

### interpreter

| Shader | Resolution | Before mean ms | After mean ms | After p95 ms | Speedup |
|---|---|---:|---:|---:|---:|
| uv | 640x360 | 12.432 | 10.184 | 10.404 | 1.221x |
| plasma | 640x360 | 41.027 | 47.369 | 49.142 | 0.866x |
| value-noise-fbm | 640x360 | 146.661 | 195.165 | 199.650 | 0.751x |
| sdf-raymarch | 640x360 | 94.074 | 147.814 | 151.637 | 0.636x |
| gaussian-blur | 640x360 | 83.648 | 111.786 | 115.725 | 0.748x |
| crt | 640x360 | 39.556 | 44.336 | 45.912 | 0.892x |
| ripple | 640x360 | 37.687 | 44.838 | 46.144 | 0.841x |
| particles | 640x360 | 170.981 | 131.185 | 133.947 | 1.303x |
| uv | 1920x1080 | 110.331 | 92.750 | 96.745 | 1.190x |
| plasma | 1920x1080 | 373.088 | 423.703 | 441.035 | 0.881x |
| value-noise-fbm | 1920x1080 | 1325.481 | 1754.305 | 1773.626 | 0.756x |
| sdf-raymarch | 1920x1080 | 842.775 | 1320.155 | 1336.537 | 0.638x |
| gaussian-blur | 1920x1080 | 760.222 | 1006.383 | 1018.496 | 0.755x |
| crt | 1920x1080 | 364.270 | 399.001 | 405.468 | 0.913x |
| ripple | 1920x1080 | 337.356 | 404.109 | 417.645 | 0.835x |
| particles | 1920x1080 | 1586.460 | 1162.490 | 1177.947 | 1.365x |

[Raw records](shader-performance/prepared-desktop-interpreter.log) · [Report](shader-performance/prepared-desktop-interpreter.json)

### scene-cpu

| Shader | Resolution | Before mean ms | After mean ms | After p95 ms | Speedup |
|---|---|---:|---:|---:|---:|
| uv | 640x360 | 9.207 | 8.486 | 9.339 | 1.085x |
| plasma | 640x360 | 16.835 | 18.405 | 19.827 | 0.915x |
| value-noise-fbm | 640x360 | 44.193 | 54.872 | 55.478 | 0.805x |
| sdf-raymarch | 640x360 | 30.099 | 43.069 | 43.458 | 0.699x |
| gaussian-blur | 640x360 | 27.373 | 34.533 | 36.395 | 0.793x |
| crt | 640x360 | 16.222 | 17.982 | 24.182 | 0.902x |
| ripple | 640x360 | 15.710 | 17.213 | 17.726 | 0.913x |
| particles | 640x360 | 50.749 | 39.335 | 40.574 | 1.290x |
| uv | 1920x1080 | 82.636 | 73.812 | 74.971 | 1.120x |
| plasma | 1920x1080 | 146.451 | 160.280 | 167.329 | 0.914x |
| value-noise-fbm | 1920x1080 | 401.018 | 497.799 | 502.414 | 0.806x |
| sdf-raymarch | 1920x1080 | 269.511 | 387.255 | 393.880 | 0.696x |
| gaussian-blur | 1920x1080 | 251.783 | 307.965 | 314.639 | 0.818x |
| crt | 1920x1080 | 146.955 | 154.794 | 159.701 | 0.949x |
| ripple | 1920x1080 | 140.911 | 153.965 | 155.937 | 0.915x |
| particles | 1920x1080 | 457.404 | 351.051 | 356.959 | 1.303x |

[Raw records](shader-performance/prepared-desktop-scene-cpu.log) · [Report](shader-performance/prepared-desktop-scene-cpu.json)

The next 32-register revision passed native/portable parity. Its short probes
still regressed desktop workloads. Batch execution addressed dispatch overhead
in the next revision.

## CPU optimizer and Android linking validation

- Native C and portable ZX prepared/batch parity passed all eight workloads,
  mutable bindings, batch sizes 0–32, overwrites and conditional discard.
- `build-queue -- make shader-scene-test`: source/saved native/portable scene
  tests passed, including leading unshaded meshes, multiple shaders, mutable
  uniforms and recompiled programs. Desktop and browser batch builds completed.
- Desktop and Pixel diagnostic batch interpreter hashes match all first eight
  baseline frames for every workload; desktop CPU scene hashes also match.
- GLES source/saved, context restore, async interleave/cancel, camera crop,
  repeated release and all-256-alpha conversion oracle tests passed after the
  Android binding change. The broader eight-workload CPU/GPU pixel comparison
  passed on headless software GLES over three animation frames: all channels
  matched exactly except blur, whose maximum difference was one byte level.
- `build-queue -- python3 tests/shader_android_link_test.py`: both ARM ABIs
  linked real scene code at API21 through `kryon_android_app`, using
  `--no-undefined`; symbol assertions rejected GLX and `eglGetPlatformDisplay`.
- Full batch desktop/browser collections completed; all reports and raw logs
  were copied locally. Wine baselines also completed and were copied locally.
- New scalar-value propagation avoids repeating sine/cosine four times for a
  provably replicated vector. It keeps the original math and passed native/
  portable parity and scene source/saved tests, including the input-mask change.
  Its full desktop comparison is retained below. The full Moto interpreter
  comparison completed with all output hashes matching.
- `make push-check` passed source, public surface, documentation, desktop,
  gamepad and text-hook host stages. Its final pixmap stage stopped while
  rebuilding the shared Ziran checkout: `compiler_x86_image.zi:753` rejected
  `relocation.offset[:]`. The compiler failure was reported to the parent;
  no compiler edits were made here. After its source changed, the complete
  `build-queue -- make push-check` passed on retry. The Android-only link fix
  was committed separately.

## Full batch CPU comparison, further revision required

All eight programs completed 60 measured frames at both resolutions; every
frame hash matches the original same-path baseline. This revision is not the
final optimization: its CPU scene path regresses several workloads. New scalar
propagation/input-copy reductions passed correctness tests and are not represented here.
The earlier short probes underestimated these full CPU-scene times.


### Desktop interpreter

| Shader | Resolution | Mean (ms) | p95 (ms) | Speedup |
|---|---|---:|---:|---:|
| uv | 640x360 | 3.949 | 4.126 | 3.15x |
| plasma | 640x360 | 21.149 | 22.954 | 1.94x |
| value-noise-fbm | 640x360 | 66.074 | 67.352 | 2.22x |
| sdf-raymarch | 640x360 | 21.127 | 21.525 | 4.45x |
| gaussian-blur | 640x360 | 39.291 | 39.857 | 2.13x |
| crt | 640x360 | 17.777 | 18.098 | 2.23x |
| ripple | 640x360 | 18.127 | 18.529 | 2.08x |
| particles | 640x360 | 18.630 | 18.911 | 9.18x |
| uv | 1920x1080 | 35.234 | 35.970 | 3.13x |
| plasma | 1920x1080 | 188.655 | 202.105 | 1.98x |
| value-noise-fbm | 1920x1080 | 594.552 | 602.871 | 2.23x |
| sdf-raymarch | 1920x1080 | 192.309 | 194.430 | 4.38x |
| gaussian-blur | 1920x1080 | 359.959 | 362.795 | 2.11x |
| crt | 1920x1080 | 161.855 | 163.076 | 2.25x |
| ripple | 1920x1080 | 166.500 | 167.984 | 2.03x |
| particles | 1920x1080 | 168.612 | 169.549 | 9.41x |

Report: `shader-performance/batch-desktop-interpreter.json`.

### Desktop scene-cpu

| Shader | Resolution | Mean (ms) | p95 (ms) | Speedup |
|---|---|---:|---:|---:|
| uv | 640x360 | 8.516 | 13.539 | 1.08x |
| plasma | 640x360 | 14.429 | 21.216 | 1.17x |
| value-noise-fbm | 640x360 | 44.622 | 45.050 | 0.99x |
| sdf-raymarch | 640x360 | 21.863 | 21.936 | 1.38x |
| gaussian-blur | 640x360 | 31.113 | 31.266 | 0.88x |
| crt | 640x360 | 19.803 | 19.838 | 0.82x |
| ripple | 640x360 | 20.088 | 20.162 | 0.78x |
| particles | 640x360 | 20.655 | 20.693 | 2.46x |
| uv | 1920x1080 | 91.050 | 118.296 | 0.91x |
| plasma | 1920x1080 | 186.531 | 191.753 | 0.79x |
| value-noise-fbm | 1920x1080 | 399.620 | 400.253 | 1.00x |
| sdf-raymarch | 1920x1080 | 195.574 | 196.728 | 1.38x |
| gaussian-blur | 1920x1080 | 283.401 | 295.651 | 0.89x |
| crt | 1920x1080 | 176.819 | 177.819 | 0.83x |
| ripple | 1920x1080 | 179.533 | 179.959 | 0.78x |
| particles | 1920x1080 | 183.751 | 184.785 | 2.49x |

Report: `shader-performance/batch-desktop-scene-cpu.json`.


## Full browser batch comparison

This revision completed 60 measured frames after eight warm-up frames at both
resolutions. All frame hashes match the original same-path browser baselines.
It predates the latest scalar propagation/input-copy changes. These gains do
not meet the 10x request.

### Browser interpreter

| Shader | Resolution | Mean (ms) | p95 (ms) | Speedup |
|---|---|---:|---:|---:|
| uv | 640x360 | 9.290 | 10.200 | 2.09x |
| plasma | 640x360 | 33.543 | 35.900 | 1.86x |
| value-noise-fbm | 640x360 | 109.713 | 112.300 | 2.21x |
| sdf-raymarch | 640x360 | 40.950 | 42.400 | 3.44x |
| gaussian-blur | 640x360 | 66.263 | 67.800 | 2.19x |
| crt | 640x360 | 29.497 | 30.500 | 1.98x |
| ripple | 640x360 | 30.218 | 31.500 | 1.94x |
| particles | 640x360 | 36.100 | 37.400 | 8.25x |
| uv | 1920x1080 | 83.933 | 85.200 | 2.14x |
| plasma | 1920x1080 | 298.777 | 313.500 | 1.90x |
| value-noise-fbm | 1920x1080 | 977.985 | 982.700 | 2.27x |
| sdf-raymarch | 1920x1080 | 365.510 | 368.100 | 3.47x |
| gaussian-blur | 1920x1080 | 595.348 | 602.900 | 2.19x |
| crt | 1920x1080 | 265.457 | 268.500 | 1.99x |
| ripple | 1920x1080 | 268.833 | 272.000 | 1.95x |
| particles | 1920x1080 | 321.535 | 325.600 | 8.43x |

[Raw records](shader-performance/batch-browser-interpreter.log) · [Report](shader-performance/batch-browser-interpreter.json)

### Browser scene-cpu

| Shader | Resolution | Mean (ms) | p95 (ms) | Speedup |
|---|---|---:|---:|---:|
| uv | 640x360 | 46.607 | 47.100 | 1.33x |
| plasma | 640x360 | 71.567 | 73.900 | 1.49x |
| value-noise-fbm | 640x360 | 148.060 | 150.200 | 1.97x |
| sdf-raymarch | 640x360 | 78.737 | 80.400 | 2.35x |
| gaussian-blur | 640x360 | 103.505 | 105.800 | 1.85x |
| crt | 640x360 | 65.683 | 68.200 | 1.59x |
| ripple | 640x360 | 65.992 | 67.900 | 1.58x |
| particles | 640x360 | 72.310 | 74.500 | 4.83x |
| uv | 1920x1080 | 408.402 | 416.200 | 1.36x |
| plasma | 1920x1080 | 624.025 | 649.700 | 1.53x |
| value-noise-fbm | 1920x1080 | 1295.888 | 1314.500 | 2.02x |
| sdf-raymarch | 1920x1080 | 691.553 | 701.300 | 2.44x |
| gaussian-blur | 1920x1080 | 907.193 | 918.600 | 1.90x |
| crt | 1920x1080 | 588.685 | 601.500 | 1.60x |
| ripple | 1920x1080 | 591.532 | 601.100 | 1.59x |
| particles | 1920x1080 | 651.697 | 663.700 | 4.88x |

[Raw records](shader-performance/batch-browser-scene-cpu.log) · [Report](shader-performance/batch-browser-scene-cpu.json)

## Latest scalar/input-copy diagnostic probes

These are eight measured frames at 640×360 after eight warm-up frames, with
`ZIRAN_PAR_THREADS=4` set for the benchmark process. They are diagnostic tuning
runs, not a complete matrix or replacement for the full tables. All first eight
frame hashes match the original same-path baseline. The original reports did
not retain this thread environment, so the earlier full scene regressions
still need controlled full-resolution investigation.

| Shader | Interpreter mean (ms) | CPU scene mean (ms) |
|---|---:|---:|
| uv | 3.762 | 6.557 |
| plasma | 13.648 | 9.218 |
| value-noise-fbm | 65.998 | 23.244 |
| sdf-raymarch | 21.239 | 11.367 |
| gaussian-blur | 40.009 | 16.160 |
| crt | 11.453 | 8.732 |
| ripple | 11.920 | 8.871 |
| particles | 18.777 | 10.477 |

[Interpreter report](shader-performance/diagnostic-scalar-input-desktop-interpreter.json) ·
[CPU scene report](shader-performance/diagnostic-scalar-input-desktop-scene-cpu.json).

The shared compiler source changed after the failed gate. The complete
`build-queue -- make push-check` passed as
`742dccdb5b51008754b08cb90672162c4c65` (exit 0). The focused
[Android API21 link fix](https://github.com/kryonlabs/kryon/commit/34f26c252352119b1176185ee05cd167476415d4)
was pushed after both ARM link regressions and the complete gate passed.
It includes only the EGL/GLX platform bindings, display selection, CMake links,
Android documentation and link fixtures. Benchmark and optimization changes
are excluded. The private index preserved the default index and local branch.
Pushed, CI not checked yet. No application installation is claimed.

## Full scalar/input-copy desktop comparison

All eight workloads completed 60 measured frames after eight warm-up frames at
both resolutions. Every frame hash matches its original same-path baseline.
The process explicitly set `ZIRAN_PAR_THREADS=4`; older reports did not retain
their thread environment. This revision shows no mean-time regressions in the
measured matrix, but the changes and thread setting cannot be separated as the
cause of the improvement over the previous full run. None of these results
meets the requested 10x across all workloads.

### Desktop interpreter

| Shader | Resolution | Mean (ms) | p95 (ms) | Speedup |
|---|---|---:|---:|---:|
| uv | 640x360 | 3.718 | 3.983 | 3.34x |
| plasma | 640x360 | 14.658 | 16.067 | 2.80x |
| value-noise-fbm | 640x360 | 66.217 | 67.810 | 2.21x |
| sdf-raymarch | 640x360 | 21.289 | 21.802 | 4.42x |
| gaussian-blur | 640x360 | 39.804 | 40.747 | 2.10x |
| crt | 640x360 | 11.447 | 11.765 | 3.46x |
| ripple | 640x360 | 11.806 | 12.144 | 3.19x |
| particles | 640x360 | 18.665 | 19.089 | 9.16x |
| uv | 1920x1080 | 33.370 | 34.789 | 3.31x |
| plasma | 1920x1080 | 131.050 | 143.946 | 2.85x |
| value-noise-fbm | 1920x1080 | 595.439 | 604.498 | 2.23x |
| sdf-raymarch | 1920x1080 | 193.355 | 197.188 | 4.36x |
| gaussian-blur | 1920x1080 | 358.679 | 367.234 | 2.12x |
| crt | 1920x1080 | 102.218 | 103.711 | 3.56x |
| ripple | 1920x1080 | 106.969 | 109.298 | 3.15x |
| particles | 1920x1080 | 167.557 | 170.957 | 9.47x |

[Raw records](shader-performance/scalar-input-desktop-interpreter.log) · [Report](shader-performance/scalar-input-desktop-interpreter.json)

### Desktop scene-cpu

| Shader | Resolution | Mean (ms) | p95 (ms) | Speedup |
|---|---|---:|---:|---:|
| uv | 640x360 | 7.226 | 12.574 | 1.27x |
| plasma | 640x360 | 9.594 | 10.241 | 1.75x |
| value-noise-fbm | 640x360 | 23.414 | 24.244 | 1.89x |
| sdf-raymarch | 640x360 | 11.817 | 12.382 | 2.55x |
| gaussian-blur | 640x360 | 16.246 | 17.193 | 1.68x |
| crt | 640x360 | 8.746 | 9.579 | 1.85x |
| ripple | 640x360 | 9.002 | 9.645 | 1.75x |
| particles | 640x360 | 10.735 | 11.401 | 4.73x |
| uv | 1920x1080 | 59.267 | 60.371 | 1.39x |
| plasma | 1920x1080 | 85.589 | 88.867 | 1.71x |
| value-noise-fbm | 1920x1080 | 207.045 | 211.250 | 1.94x |
| sdf-raymarch | 1920x1080 | 102.177 | 105.021 | 2.64x |
| gaussian-blur | 1920x1080 | 143.438 | 146.683 | 1.76x |
| crt | 1920x1080 | 78.197 | 78.477 | 1.88x |
| ripple | 1920x1080 | 78.959 | 80.323 | 1.78x |
| particles | 1920x1080 | 94.705 | 95.674 | 4.83x |

[Raw records](shader-performance/scalar-input-desktop-scene-cpu.log) · [Report](shader-performance/scalar-input-desktop-scene-cpu.json)

## Full scalar/input-copy Moto interpreter comparison

The API21 ARMv7 build passed its eight-program correctness smoke and completed
60 measured frames after eight warm-up frames at both resolutions. Every frame
hash matches the original interpreter baseline. The 1080p gains range from
1.73x to 5.97x; the 10x target remains unmet.

| Shader | Resolution | Mean (ms) | p95 (ms) | Speedup |
|---|---|---:|---:|---:|
| uv | 640x360 | 49.192 | 49.357 | 2.65x |
| plasma | 640x360 | 189.343 | 209.433 | 2.19x |
| value-noise-fbm | 640x360 | 997.642 | 1004.968 | 1.78x |
| sdf-raymarch | 640x360 | 364.150 | 365.279 | 2.23x |
| gaussian-blur | 640x360 | 609.713 | 604.689 | 1.75x |
| crt | 640x360 | 161.411 | 157.804 | 2.55x |
| ripple | 640x360 | 172.422 | 168.013 | 2.35x |
| particles | 640x360 | 299.549 | 297.924 | 5.88x |
| uv | 1920x1080 | 444.758 | 442.990 | 2.61x |
| plasma | 1920x1080 | 1675.775 | 2019.900 | 2.21x |
| value-noise-fbm | 1920x1080 | 9119.863 | 9393.187 | 1.73x |
| sdf-raymarch | 1920x1080 | 3297.382 | 3494.840 | 2.21x |
| gaussian-blur | 1920x1080 | 5524.592 | 5788.717 | 1.74x |
| crt | 1920x1080 | 1431.201 | 1605.516 | 2.59x |
| ripple | 1920x1080 | 1535.481 | 1738.389 | 2.35x |
| particles | 1920x1080 | 2737.086 | 2933.863 | 5.97x |

[Raw records](shader-performance/scalar-input-moto-interpreter.log) ·
[Report](shader-performance/scalar-input-moto-interpreter.json).

The subsequent sampler revision reuses adjacent nearest-filtered texels within
one batch instruction and expands bytes through an exact binary64 lookup table.
The public sampler provides an independent division reference. Prepared parity
tests cover all 256 values per RGBA channel, in-place writes, clamping, repeating
and pixel mutations between calls.

## Current sampler verification

The initial managed sampler check failed on a checked floating-point literal.
The sampler table now computes the exact binary64 quotients during module
initialization. Prepared shader native/ZX parity, CPU scene source/saved
native/portable tests, benchmark statistics, and 34 private GLES translator
oracle cases passed using the built Ziran tools. Rebuilding the shared local
Ziran override for the integrated Make targets failed in `cmd/zir/zir_token.h`
on missing `ZirLexer` and `ZirToken` declarations.

The desktop C output exposes a compiler optimization for `std/math.Finite64`. The
generated `std_math.h` declares an external function, while `std_math.c` has
only the NaN and finite-range comparisons. The optimized executable retains
calls, preventing these checks from folding into per-pixel code across units.
The requested upstream investigation is `#inline` on that generic function,
preserving NaN/infinity behavior. Reproduce with the desktop benchmark build
and inspect its generated C declarations and disassembly.

## Updated hardware GLES measurements

The desktop rerun compares 60 timed frames per workload and resolution with
the original hardware GLES baseline. The GPU identity and all 960 RGBA frame
hashes match. At 1080p the mean improved by 1.39–1.50× and p95 by 1.14–1.48×.
Mean frame time is 2.15–2.38 ms and p95 is 2.34–3.03 ms. Readback, including
GPU completion, costs 1.36–1.48 ms per frame. Conversion costs 0.68–0.74 ms.
Even if all other phases took zero time, this measured readback would limit
the speedup over the baseline to 2.24–2.43× at 1080p. That bound applies while
the readback cost stays the same.

[Desktop raw records](shader-performance/current-desktop-scene-gpu.log) · [Desktop report](shader-performance/current-desktop-scene-gpu.json)

### Android armv7 GPU, newly enabled path

The earlier capability gate rejected the device's hardware depth-comparison
filtering. These measurements are for unshadowed scenes using GLES with that
gate relaxed. All eight workload shaders passed three animation frames against
the CPU oracle with a maximum error of one byte level. This matrix establishes
a GPU reference for future comparisons on this device.

| Shader | Resolution | Mean (ms) | p95 (ms) |
|---|---|---:|---:|
| UV | 640×360 | 8.720 | 9.459 |
| Plasma | 640×360 | 9.177 | 9.571 |
| Value noise fbm | 640×360 | 22.398 | 23.436 |
| SDF raymarch | 640×360 | 13.226 | 13.942 |
| Gaussian blur | 640×360 | 17.678 | 18.177 |
| CRT | 640×360 | 11.272 | 12.356 |
| Ripple | 640×360 | 13.100 | 15.988 |
| Particles | 640×360 | 12.333 | 13.736 |
| UV | 1920×1080 | 50.568 | 54.244 |
| Plasma | 1920×1080 | 51.504 | 54.267 |
| Value noise fbm | 1920×1080 | 165.687 | 169.556 |
| SDF raymarch | 1920×1080 | 88.758 | 93.706 |
| Gaussian blur | 1920×1080 | 121.192 | 129.984 |
| CRT | 1920×1080 | 56.781 | 60.521 |
| Ripple | 1920×1080 | 61.036 | 65.357 |
| Particles | 1920×1080 | 73.291 | 76.678 |

At 1080p, readback and GPU completion cost 43.97–155.44 ms. Conversion costs
4.91–7.27 ms. The GPU phase profiler uses Ziran's platform-aware monotonic
timer. This corrects the clock structure on Android ARMv7. The collector
rejects phase measurements that exceed the total frame time.

[Android armv7 raw records](shader-performance/current-moto-scene-gpu.log) · [Android armv7 report](shader-performance/current-moto-scene-gpu.json)

## Updated desktop CPU comparisons

The CPU scene selects 12 lanes on the measured desktop when
`ZIRAN_PAR_THREADS` is unset. The interpreter comparison uses a single lane.
Both reports matched all baseline frame hashes. The desktop interpreter's
particle workload reached 10.03× mean and 10.55× p95 at 1080p.

| 1080p shader | Interpreter mean / p95 | CPU scene mean / p95 |
|---|---:|---:|
| UV | 3.74× / 3.68× | 1.93× / 1.60× |
| Plasma | 2.96× / 2.82× | 2.64× / 2.40× |
| Value noise fbm | 2.14× / 2.16× | 2.98× / 2.53× |
| SDF raymarch | 4.67× / 4.56× | 4.09× / 3.65× |
| Gaussian blur | 1.83× / 1.81× | 2.56× / 2.72× |
| CRT | 3.53× / 3.39× | 2.79× / 2.66× |
| Ripple | 3.08× / 3.02× | 2.67× / 2.31× |
| Particles | 10.03× / 10.55× | 7.30× / 6.87× |

[Interpreter raw records](shader-performance/current-desktop-interpreter.log) · [Interpreter report](shader-performance/current-desktop-interpreter.json) · [CPU scene raw records](shader-performance/current-desktop-scene-cpu.log) · [CPU scene report](shader-performance/current-desktop-scene-cpu.json)

## GPU shadow filtering on PowerVR

The capability probe selects shader-based percentage-closer filtering when
hardware comparison is unavailable. The fragment shader compares four raw
depth texels at each of the same five sample positions and bilinearly mixes
the comparison results. The depth texture uses nearest filtering.

`build-queue -- python3 tests/scene_gpu_test.py` passed the shadow-cache,
CPU parity and foreign EGL context tests with hardware comparison and with
the shader path forced. In the Android GPU accuracy test, the Moto's PowerVR
Rogue GE8320 reported hardware comparison unavailable and rendered the
160×100 shadowed scene on the GPU. Shadows changed 2,994 color channels.
Across the entire image, mean absolute CPU/GPU error was 0.153 byte levels
and 287 of 64,000 RGBA channels differed by more than 16 levels.

## Desktop hardware GLES and animated scene rerun

The desktop rerun matched all 960 baseline shader frame hashes. Across
the eight shaders at 1080p, mean frame time was 2.086–2.219 ms (1.493–1.583×
faster) and p95 was 2.173–2.421 ms (1.402–1.668× faster). Readback and the GPU
completion wait took 1.345–1.455 ms. Conversion and the final copy took
0.660–0.678 ms. A 10× improvement over the baseline would require the entire
image to be available in under 0.35 ms, including the readback phase.

[Shader report](shader-performance/latest-desktop-scene-gpu.json) · [Raw records](shader-performance/latest-desktop-scene-gpu.log)

The four animated fixtures completed 480 measured frames after their warmups.
These are the first measurements for these fixtures.

| Scene | 640×360 mean / p95 (ms) | 1920×1080 mean / p95 (ms) |
|---|---:|---:|
| Lit meshes | 0.515 / 0.632 | 2.345 / 2.566 |
| Skinned character | 0.442 / 0.459 | 2.217 / 2.315 |
| Textured room | 0.439 / 0.483 | 2.354 / 2.658 |
| Particle field | 0.672 / 0.703 | 2.425 / 2.686 |

[Animated scene report](shader-performance/latest-visual-scene-gpu.json) · [Raw records](shader-performance/latest-visual-scene-gpu.log)

## Android frame cadence on changing paint

Android uses a 16 ms deadline from frame start while paint changes or the
looper reports an event. Two consecutive unchanged, event-free frames restore
the 100 ms idle delay. The host regression test covers both transitions, a
render overrun and the retained renderer skipping unchanged presentations.
`build-queue -- sh tests/android_frame_input_test.sh` passed.

A separate NativeActivity on the USB armv7 device alternated the surface color
and inserted 22 ms of render work before posting each frame. It compared the
previous reuse-based delay with the current `AndroidNextFrame` policy in the
same process. Each phase posted 32 frames. Results are consecutive
SurfaceFlinger actual-present timestamps after discarding three warmup frames.

| Policy | Intervals | Median (ms) | p90 (ms) |
|---|---:|---:|---:|
| Reuse-based delay | 28 | 138.458 | 155.918 |
| Paint-aware pacing | 28 | 34.636 | 34.737 |

This fixture measures Android presentation cadence under controlled paint
changes. Application-specific panning timings require the same application
revision, assets and gesture before and after the change.

## Browser Wasm comparison

Chromium ran the full shader matrix with the eight warmup frames excluded.
Both optimized run modes matched all 960 corresponding baseline frame hashes.
The ranges span all eight shaders at the stated resolution.

| Path | Resolution | Mean speedup | p95 speedup |
|---|---|---:|---:|
| Wasm interpreter | 640×360 | 2.59–8.74× | 2.55–9.32× |
| Wasm interpreter | 1920×1080 | 2.68–8.82× | 2.56–9.36× |
| Wasm CPU scene | 640×360 | 1.27–4.58× | 1.26–4.74× |
| Wasm CPU scene | 1920×1080 | 1.27–4.68× | 1.25–4.88× |

[Interpreter report](shader-performance/latest-browser-interpreter.json) · [Interpreter raw records](shader-performance/latest-browser-interpreter.log) · [CPU scene report](shader-performance/latest-browser-scene-cpu.json) · [CPU scene raw records](shader-performance/latest-browser-scene-cpu.log)

## Android aarch64 GLES repeat measurements

All 960 frame hashes match the earlier aarch64 hardware GLES run. These
were collected from separately built executables with the same renderer.
At 1080p, three workloads took 1.57–1.97× as long as in the earlier run,
with most of the increase in GPU completion and readback. The other five
were within 2.3% of their earlier frame means. The separate runs cannot
isolate the causes of the timing increases.

| 1080p shader | Earlier mean / p95 (ms) | Repeat mean / p95 (ms) |
|---|---:|---:|
| uv | 15.338 / 22.043 | 24.173 / 29.568 |
| plasma | 17.538 / 21.152 | 34.497 / 36.166 |
| value-noise-fbm | 54.511 / 92.120 | 85.383 / 93.965 |
| sdf-raymarch | 61.084 / 62.809 | 60.810 / 62.263 |
| gaussian-blur | 82.469 / 87.279 | 82.064 / 87.075 |
| crt | 34.850 / 36.536 | 34.753 / 36.496 |
| ripple | 36.377 / 38.475 | 36.046 / 38.693 |
| particles | 49.264 / 51.608 | 50.369 / 52.444 |

[Earlier report](shader-performance/pixel-scene-gpu.json) · [Repeated report](shader-performance/latest-pixel-scene-gpu.json) · [Repeated raw records](shader-performance/latest-pixel-scene-gpu.log)

## Windows executable under Wine

The Windows interpreter and CPU scene ran the same 16 workload and resolution
pairs under Wine 10.0. Each mode matched all 960 baseline hashes. This fixture
runs CPU rendering and uses a private display for Wine.

| Path | Resolution | Mean speedup | p95 speedup |
|---|---|---:|---:|
| Wine interpreter | 640×360 | 3.33–19.39× | 3.31–18.79× |
| Wine interpreter | 1920×1080 | 3.34–19.70× | 3.33–20.27× |
| Wine CPU scene | 640×360 | 1.71–13.03× | 1.35–9.94× |
| Wine CPU scene | 1920×1080 | 2.34–14.52× | 2.16–16.27× |

[Interpreter report](shader-performance/latest-wine-interpreter.json) · [Interpreter raw records](shader-performance/latest-wine-interpreter.log) · [CPU scene report](shader-performance/latest-wine-scene-cpu.json) · [CPU scene raw records](shader-performance/latest-wine-scene-cpu.log)

## Android aarch64 interpreter comparison

The prepared interpreter matched all 960 original output hashes. All eight
workloads improved at both resolutions. The ranges include the fastest and
slowest improvements across the eight workloads.

| Resolution | Mean speedup | p95 speedup |
|---|---:|---:|
| 640×360 | 2.12–6.22× | 2.12–6.77× |
| 1920×1080 | 2.09–6.12× | 2.07–6.69× |

[Report](shader-performance/latest-pixel-interpreter.json) · [Raw records](shader-performance/latest-pixel-interpreter.log)

## Android armv7 GLES repeat measurements

The repeat on the PowerVR GPU matched all 960 frame hashes from the first
armv7 GLES run. At 1080p, mean frame times were 49.27–168.88 ms and p95
was 51.21–178.04 ms. GPU completion and readback accounted for 43.60–162.07 ms.
The original armv7 build reported GLES unavailable, so these results establish
the cost of the supported GPU path.

[Report](shader-performance/latest-moto-scene-gpu.json) · [Raw records](shader-performance/latest-moto-scene-gpu.log)

## Android armv7 interpreter comparison

All 960 hashes match the original interpreter report. The earlier scalar
comparison remains available above. Another rendering test briefly used the
phone during the 1080p repeat, so the latest timing includes possible scheduler
and thermal interference.

| Resolution | Mean speedup | p95 speedup |
|---|---:|---:|
| 640×360 | 2.11–6.12× | 2.10–6.65× |
| 1920×1080 | 2.07–6.26× | 1.96–6.50× |

[Report](shader-performance/latest-moto-interpreter.json) · [Raw records](shader-performance/latest-moto-interpreter.log)

## Matched 1080p comparisons

Each range spans the eight shaders. The speedups compare each shader against its
own baseline. Every matched pair has identical output hashes for all 960
measured frames across the two resolutions.

| Execution path | Baseline mean (ms) | Optimized mean (ms) | Mean speedup | p95 speedup |
|---|---:|---:|---:|---:|
| Desktop interpreter | 110.331–1586.460 | 29.516–620.632 | 1.83–10.03× | 1.81–10.55× |
| Desktop CPU scene | 82.636–457.404 | 42.809–134.523 | 1.93–7.30× | 1.60–6.87× |
| Desktop hardware GLES | 3.211–3.410 | 2.120–2.261 | 1.46–1.57× | 1.41–1.58× |
| Wine interpreter | 165.579–3133.734 | 28.326–641.164 | 3.34–19.70× | 3.33–20.27× |
| Wine CPU scene | 96.040–903.244 | 41.063–132.780 | 2.34–14.52× | 2.16–16.27× |
| Chromium Wasm interpreter | 179.222–2710.798 | 66.617–702.192 | 2.68–8.82× | 2.56–9.36× |
| Chromium Wasm CPU scene | 554.882–3178.005 | 437.252–1079.382 | 1.27–4.68× | 1.25–4.88× |
| Android aarch64 interpreter | 395.912–5516.638 | 116.522–1510.946 | 2.09–6.12× | 2.07–6.69× |
| Android armv7 interpreter | 1159.770–16344.677 | 413.232–7612.361 | 2.07–6.26× | 1.96–6.50× |

Particles reached 10× at 1080p in the desktop and Wine interpreters and in
Wine's CPU scene renderer. The desktop GLES frame times include synchronous
delivery of an RGBA image. GPU completion and readback alone exceed the entire
10× frame budget. The timings on CPU and Wasm quantify the remaining gap for
those paths. Each Android GLES report identifies the comparison available for
that phone.

## Pinned Ziran verification

The benchmark built against the `0c4465a461df0bba473cf249ed30e60d94a6d364`
Ziran pin. Local `ziran check --project --define SHADER_BENCH_GPU
tests/shader_benchmark.zi` and `make -j1 push-check` passed through the build
queue. The queued `shader-prepared-test`, `shader-bench-test`,
`shader-scene-test`, `shader-glsl-test` and `scene-gpu-test` targets also
passed, followed by CPU and GPU visual correctness smokes. A separate local
run passed the Android frame input regression.

The rebuilt desktop executable matched all 960 baseline shader hashes at both
resolutions. At 1080p, mean frame time was 2.120–2.261 ms and p95 was
2.215–2.497 ms. GPU completion and readback took 1.347–1.471 ms, with another
0.658–0.683 ms in output conversion and copy. The desktop benchmark host ran
the same queued Python recipe directly because it has no `make`. The reports
record a build from the shared working tree and include the lockfile and tool
fingerprints.

[Pinned shader report](shader-performance/pinned-desktop-scene-gpu.json) · [Raw records](shader-performance/pinned-desktop-scene-gpu.log)

| Animated scene | 640×360 mean / p95 (ms) | 1920×1080 mean / p95 (ms) |
|---|---:|---:|
| Lit meshes | 0.555 / 0.701 | 2.373 / 2.681 |
| Skinned character | 0.440 / 0.453 | 2.232 / 2.435 |
| Textured room | 0.434 / 0.488 | 2.298 / 2.445 |
| Particle field | 0.658 / 0.672 | 2.437 / 2.590 |

[Pinned animated scene report](shader-performance/pinned-visual-scene-gpu.json) · [Raw records](shader-performance/pinned-visual-scene-gpu.log)

## Android room startup and shader binaries

The armv7 GLES room fixture uses Jungle's treehouse geometry and materials,
2×2 placeholder textures, and a 411×813 output. It renders three identical
camera positions in each case. The scene contains 170 nodes, 129 meshes and
99,444 vertices. Times below are single first-frame samples, in milliseconds.
GPU context creation and scene construction are measured separately.

| Process and program cache | GPU probe | Prepare | Shadows | Draw and resolve | Readback wait | Total first frame |
|---|---:|---:|---:|---:|---:|---:|
| Baseline, new process | 656.48 | 7055.94 | 12.38 | 17.32 | 34.03 | 7181.03 |
| Specialized shaders, empty cache | 699.33 | 3530.85 | 6.00 | 15.02 | 26.62 | 3622.86 |
| Specialized shaders, saved binaries, new process | 297.88 | 137.43 | 4.55 | 15.15 | 48.57 | 214.53 |

GLES links the shared scene vertex shader once per context. Fragment code
omits shadow samplers for lights with no shadow map and merges material
variants whose lighting flags have no effect. Android keeps linked binaries
in its private files directory and validates the driver and source key before
loading. Missing files and rejected binaries compile from GLSL. The saved
binaries occupied about 270 KiB in this fixture. All three RGBA hashes matched
the baseline. These timings cover the room renderer and exclude the Jungle
APK's startup, asset decoding, and UI event handling.

## Android touch pacing and pixel conversion

The standard Android host now keeps the active frame deadline while a pointer
is held, even when consecutive paints are unchanged. It resumes idle pacing
after release. In an armv7 test APK with the same workload in both modes,
three unchanged paints separated each changed paint. SurfaceFlinger reported
20 intervals per mode.

| Changed-frame intervals (ms) | Median | p90 |
|---|---:|---:|
| Before | 294.4 | 294.6 |
| While held | 103.9 | 104.0 |

An armv7 conversion probe compared byte-exact RGBA channel shuffles, including
stride padding and tail pixels. The existing kernel took 4.482 ms at 719×1560
and 0.615 ms at 249×412. An unrolled shuffle took 4.213 and 0.645 ms, and
structured NEON loads and stores took 4.359 and 0.657 ms. The present path
keeps the existing SIMD kernel for dirty-rectangle workloads.

## Scene submission and readback

Camera transforms, scene lighting, and grade values are uploaded once per
program per frame. Materials still supply their own colors, texture and
shader bindings, and grade enable state. The desktop benchmark verifies
identical output hashes for all eight shaders and four animated scenes.

| Desktop animated scene | 640×360 mean was → now (ms) | 1920×1080 mean was → now (ms) | 1920×1080 p95 was → now (ms) |
|---|---:|---:|---:|
| Lit meshes | 0.555 → 0.440 | 2.373 → 2.311 | 2.681 → 2.541 |
| Skinned character | 0.440 → 0.403 | 2.232 → 2.197 | 2.435 → 2.435 |
| Textured room | 0.434 → 0.381 | 2.298 → 2.253 | 2.445 → 2.549 |
| Particle field | 0.658 → 0.539 | 2.437 → 2.303 | 2.590 → 2.484 |

At 1080p, the eight desktop shader workloads took 2.101–2.258 ms mean and
2.220–2.508 ms p95, compared with 2.120–2.261 ms mean and 2.215–2.497 ms p95
at the previous checkpoint. Each workload has 60 measured frames after eight
warm-ups at both sizes.

An armv7 experiment read the opaque MSAA resolve straight into CPU memory
instead of staging via the straight-RGBA pass. It produced identical pixels,
but warm GPU wait rose from 50.495 ms to 265.441 ms median and from 51.401 ms
to 489.952 ms p95 across 24 matched frames per path. Synchronous readback
retains the staging pass. The measured host draw stage fell from 4.132 ms to
3.112 ms median after the uniform change.

The async PBO path was also measured with and without a staging pass before
the pixel transfer. Both matched the synchronous image exactly. Each result
uses 28 warm frames in the same armv7 room fixture.

| Async output | Direct median / p95 (ms) | GPU staging before PBO median / p95 (ms) |
|---|---:|---:|
| 411×813 | 122.059 / 125.869 | 120.082 / 126.297 |
| 1291×1423 | 432.264 / 454.859 | 440.831 / 462.718 |

At 1291×1423, mapping, copying, and unmapping the completed PBO took
172.834 ms median. The extra staging pass left that cost essentially unchanged.

A split measurement on the same GPU found 169.316 ms in the mapped-memory
copy, 0.046 ms in mapping, and 0.011 ms in unmapping at 1291×1423. The
unrolled NEON copy took 176.323 ms. Reading a staged RGBA texture into CPU
memory after the fence reduced total latency with identical output hashes.
This comparison used 14 warm frames per path and size.

| Async output | PBO median / p95 (ms) | CPU read after fence median / p95 (ms) |
|---|---:|---:|
| 411×813 | 118.814 / 126.533 | 96.176 / 97.615 |
| 1291×1423 | 409.617 / 480.385 | 281.796 / 327.207 |

Kryon uses a separate staging texture for each pending frame on the measured
32-bit Android renderer. It checks the fence before reading and preserves
four-sample antialiasing, exact RGBA output, and the two-frame queue.

The final implementation was measured again with 14 warm frames per size.
All 32 corresponding frames, including warm-up frames, matched the PBO
baseline and synchronous RGBA hashes. The 170-node fixture has 129 meshes,
99,444 vertices, and a 2×2 placeholder texture.

| Async output | Final mean (ms) | PBO median → final median (ms) | PBO p95 → final p95 (ms) |
|---|---:|---:|---:|
| 411×813 | 81.482 | 118.814 → 87.826 | 126.533 → 96.737 |
| 1291×1423 | 258.559 | 409.617 → 256.693 | 480.385 → 289.708 |

At the larger size, the ready-frame transfer took 34.833 ms median and the
final CPU copy took 6.250 ms median. Most end-to-end latency still elapsed
before the fence signalled. The ARMv7 device test checked distinct readback
textures for both slots, an interleaved synchronous render, all 256 alpha
values, cancellation, context loss, and 24 release cycles. These measurements
cover Kryon's renderer. The Jungle APK has additional work per frame.
