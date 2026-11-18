# Figure preview lag — open work (2026-11-18)

Owner, about Breeze's figure preview with the VRoid model: "it looks much much
better but do fix the lag fix the core isuses plesae in kryon".

The preview (`waozixyz/apps/breeze/design/figure-preview`) loads
HairSample_Female/Male through `src/ui/scene_gltf.zi` (Kryon 0ad6f6cf). The model
has about 25-27k triangles, many primitives and materials, 25 textures, many
face blend shapes, and 357/273 joints including the hair spring bones.

## Measured fix: compact shared accessor primitives

The avatar's material primitives each copied the full shared vertex accessor
into their own scene mesh and skin. The woman has 27,068 triangles but this
expanded to 1,149,420 animated vertices. The scene adapter now retains each
primitive's referenced vertices in accessor order, remaps triangle indices,
and carries the source vertex mapping through morph blending. The model's
accessors and morph targets are unchanged.

Windowless real AMD Radeon Graphics measurements, 120 timed updates after 12
warmup updates, on 2026-11-18. The benchmark alternates neutral, active,
negative and returning-to-zero morph weights and tests linear and DQ skinning.

| Avatar | Vertices before → after | Linear median/p95 ms before → after | DQ median/p95 ms before → after |
| --- | --- | --- | --- |
| Woman | 1,149,420 → 19,520 | 15.849/26.781 → 1.378/2.353 | 29.302/35.649 → 2.119/4.388 |
| Man | 965,948 → 18,145 | 15.190/19.702 → 1.541/5.720 | 24.145/29.407 → 1.957/2.946 |

Warm GPU step medians (six renders, both skinning modes; milliseconds):

| Avatar | Prepare before → after | Draw submission before → after | Readback before → after | Conversion before → after |
| --- | --- | --- | --- | --- |
| Woman | 18.098 → 0.859 | 0.018 → 0.039 | 2.810 → 5.083 | 0.244 → 0.722 |
| Man | 16.389 → 1.607 | 0.020 → 0.037 | 2.961 → 6.075 | 0.264 → 1.576 |

Readback includes the GPU completion wait. These sparse render samples are
diagnostics, not full-preview frame medians. The preview's supplied original
baseline was 160 ms median / 232 ms p95 with 58 ms rendering. Full-preview
after timings and a steady 60 fps remain to be verified after its Kryon pin
is updated; the figure lane is rebuilding the stable executable separately.

All eight triangle-order surface hashes and all eight 600×800 RGBA pixel hashes
per avatar match the original exactly, including painted pixel counts.
`make scene-gltf-test` passed for native source and saved IR with added
unused-prefix/suffix, linear/DQ animation, morph and exact pixel regressions.
`make scene-gltf-gpu-test` also passed for native source and saved IR. The
dependency lane's serialized published-toolchain gate passed `make push-check`
and the full `make test`, including source/saved glTF and GPU integration.
The final full-test receipt completed successfully on 2026-11-18. Its ARMv7
Moto source/saved picture verification also passed; the aligned dependency
commit is published. The downstream pin and complete-preview after
measurements remain pending.

Reproduce the benchmark from the Kryon root through the build queue:

```sh
build-queue -- env LDLIBS='-lEGL -lGLESv2 -lGLX -lX11 -lz -lm' \
  ziran build --project --target=c --exe \
  --entry scene_gltf_update_benchmark:main --module-path tests \
  -o build/scratch/scene-gltf-benchmark tests/scene_gltf_update_benchmark.zi
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
  -u DBUS_SESSION_BUS_ADDRESS HARMONY_DESKTOP_RECOVERY=0 \
  EGL_PLATFORM=surfaceless \
  build/scratch/scene-gltf-benchmark/scene_gltf_update_benchmark MODEL.vrm
```

The current shell's device boundary selects llvmpipe. Real-GPU runs used
`launch_app` on the ignored `backup/figure-preview-lag/run-baseline.sh` and
`run-compacted.sh`, which scrub desktop variables and run only surfaceless
offscreen rendering through the build queue. Both completed successfully.
Their bounded logs and the private index remain in that ignored folder.

No changes to the other lane's GPU/present files or the figure lane's motion
files were needed for this fix. Further optimization depends on integrated
preview measurements.

## Measure first

- Real GPU: an owner-visible launch log, like the figure lane's `--profile`
  (per-step timers, a line every 120 frames). Shells get llvmpipe only, so the
  real GPU is measured through `launch_app`.
- Headless: llvmpipe on a private Xvfb.
- CPU profile: no perf on Omega. Use a SIGPROF sampler on an unstripped build,
  then nm/addr2line.
- CPU steps: glTF update, animation sampling, skinning, morph blending, spring
  bones, normals, the scene bake/snapshot, and upload.
- GPU steps: draw calls, texture binds, the shadow pass, and readback.
- Report the median and p95 frame time on the real GPU before and after, with
  the per-step breakdown.

## Suspects to confirm or rule out

- Morph blending over every target each frame, even at zero weight.
- CPU skinning of all vertices.
- One draw call per primitive, with texture rebinds.
- `GltfUpdateScene` rebuilding nodes every frame.
- Unchanged streams being snapshotted or uploaded again.
- The preview rendering offscreen and reading pixels back into the raster
  every frame instead of drawing straight into the window.

## Fixes to make where measured

- Skip zero-weight morphs and keep sparse morph deltas.
- GPU skinning in the vertex shader, with joint matrices in a texture for the
  GLES2 uniform limit.
- Batch draws by material.
- No readback: draw the 3D scene straight into the frame.

Each change needs a pixel-identical test and a benchmark.

## Ownership and checkout state

- Our implemented fix is in `src/ui/scene_gltf.zi`; its tests, benchmark and
  this plan are owned here. No skin implementation or GPU/present changes
  were needed for primitive compaction.
- Not ours: `src/ui/scene_vrm.zi` (untracked) and the future `scene_bvh.zi`
  belong to the Breeze figure lane
  (claude-breeze-astral-threejs-animations-2-2-2-2-2), and so does Breeze's
  practices package. Tell the coordinator if an API they use changes.
- `src/backend/scene_gpu.zi`, `scene_gpu_gl.zi`, `scene_gpu_sources.zi` and
  `src/ui/scene.zi` already carry another lane's uncommitted edits:
  `scene_gpu.zi` is +291/-120 at 14:42, with a canvas/web render path
  (`SceneGpuRenderCanvas`), reworked readback targets
  (`DropReadbackTarget`, `CopyReadback`) and async submit/poll. The untracked
  `scene_gpu_web_context.zi` belongs with it. Find that lane before editing
  these files.
- The main `.git/index` dates from 03:37 and doesn't match HEAD, so plain
  `git status` and `git diff HEAD` show hundreds of false changes. Inspect and
  commit through a private index: `GIT_INDEX_FILE=<file> git read-tree HEAD`.
- `origin` is Gitea. Run `build-queue -- make push-check` before pushing.
