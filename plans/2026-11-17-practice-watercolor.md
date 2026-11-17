# Native practice skinning and watercolor — open items

Consumer: Breeze's native Tai Chi, Workout and astral characters, which use
`kryon/scene_skin`, `kryon/watercolor` and the `LoadImagePixels` path
resolver.

- Breeze adoption: Breeze pins Kryon 3f724419, which predates these modules.
  Its shared Kryon repin (plot, entity, game2d, css and apps on one Kryon,
  Breeze audio on Ziran std audio) is tracked in Breeze's
  `plans/2026-11-15-breeze-desktop-without-raylib.md`. The painters connect
  only after that repin.
- Full gate: the focused module tests and the CPU/GLES parity test pass
  (`make scene-skin-test watercolor-test practice-material-gpu-test
  image-pixels-resolver-test`). The full `make test push-check` still stops
  at other lanes' failures: program-entry project expectation, Ziran app
  files/system theme, template package graph, and stale deleted-raylib
  boundary references. Rerun it once those are fixed.
- `tests/scene_gpu_test.py` (scene/shader lane) calls `zi2c --entry` without
  `--exe`, so with the current toolchain its programs have no `main` and fail
  to link. `practice_material_gpu_test.py` builds with `--exe` itself.
- Performance: the material and the finishing pass (brush, grain, pooling,
  pencil contour) were measured only on llvmpipe and the CPU renderer. Real
  phone GPU time, memory and thermals are unmeasured.
