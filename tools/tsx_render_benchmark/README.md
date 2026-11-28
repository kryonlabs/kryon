# Persistent TSX and native Ziran renderer benchmark

Run the study from the canonical Kryon checkout on physical Alpha after reserving an exclusive host window. It uses the canonical sibling Ziran checkout and its built compiler, SDL2, Xvfb, and glibc malloc debugging. Keep the November 26 study and its scratch binaries in place. This campaign writes new generated outputs under `build/scratch/tsx-render-benchmark/persistent/` and evidence to a fresh Documents directory.

```sh
python3 tools/tsx_render_benchmark/benchmark.py \
  --output ~/Documents/tsx-native-benchmarks-persistent --samples 9
```

`--verify-only` builds both executables and checks every warmup and measured-position frame. A normal run checks parity before timing any case. Both modes compare pixmap pixels, tree geometry and clips at exact float precision, semantics, retained identities, paint counts, and expected state changes. The regular timing runs compare behavior and counts with the parity pass. Pixel and tree hashing happen in the separate parity pass so they cannot perturb timed frames. The collector pins the runners and Xvfb to separate physical cores and launches a private display with a new authentication cookie.

The six original cases cover retained and forced toolbar, changing counter, and 128-row list frames. Additional cases cover a hit-tested counter, alternating Unicode insertion and deletion in a controlled field, 96 independent stateful counters with queued actions, keyed reorder with retirement and remount, a 1,024-row unvirtualized list, a moving viewport into 10,000 rows, and a live Like action in the migrated Web starter. The last case calls the maintained template's TSX `Frame` with the same native page model used by the reference. Ziran drawing code is confined to the internal benchmark fixture. The two lanes use the same widget implementations and renderer.

Timing samples distinguish input, submission, commit, CPU pixmap raster, and SDL software upload/presentation. The collector measures startup from process launch and from native runner entry through the first completed presentation. CPU affinity, observed clock frequency, per-logical-CPU busy fraction, startup, RSS, executable size, and source/tool/binary hashes are retained. Allocation profiling has its own runs and measures glibc allocation calls and requested bytes after warmup. Full build trials clear the campaign's frontend and output caches. Incremental trials change the same toolbar label in each authored lane and restore the fixture afterward. Run the collector when no one else is editing its fixture files.

The November 26 campaign measured a TSX-to-Ziran prototype. This campaign builds the current TSX directly through Ziran's generic frontend and C exporter. Compare the lanes within this campaign. Its software presentation timings characterize the private X server and selected workload at 960 × 600.
