# Kryon runtime research against CUI

Date: 2026-11-08  
Scope: platform/rendering architecture, dependency and size inventory, performance
boundaries, and the general Ziran work needed for small, hot-swappable `.zx`
components. Component parity and website design are separate research tasks.

This is a research report, not a benchmark result and not an implementation. No
Kryon build, browser run, desktop run, Android run, or performance test was run
for this report. Existing artifacts were only inspected for identity, size, and
freshness.

## Evidence snapshot

- Kryon worktree: `a139f22c5eb3c5b97994a727cfca94ab63104d79`, dirty with
  unrelated active work. Counts and file state below are a research snapshot,
  not a clean release claim.
- Ziran worktree: `acb15f2ae465a2b3b62881cf48313fab8774d854`, dirty.
- CSS package worktree: `6aad71c34fffec40ec903cc8f9d5c95f02572d35`, clean.
- Kryon snapshot inventory: 275 `src/ui/*.zi` modules, 177
  `src/backend/*.zi` modules, and about 105,000 source lines in the researched
  set. `src/ui/widget_kind.zi:2` defines 56 semantic widget kinds (values 0
  through 55); `WidgetKindCount` is a sentinel, not a 57th widget. This is not
  a component-count comparison because CUI and Kryon do not define “component”
  identically.

## CUI claims, in CUI's own terms

The relevant primary sources are the
[README](https://github.com/yappologistic/CUI/blob/master/README.md#L311-L325),
[performance methodology](https://github.com/yappologistic/CUI/blob/master/docs/performance.md),
[benchmark source](https://github.com/yappologistic/CUI/blob/master/tests/bench.c),
[integration guide](https://github.com/yappologistic/CUI/blob/master/docs/integration.md),
and [rendering guide](https://github.com/yappologistic/CUI/blob/master/docs/rendering.md).

- **Components:** CUI's README advertises about 170 built-in components. The
  owner's “about 160” recollection is close, but 170 is the figure to quote.
- **Codebase/dependencies:** CUI describes one C99 codebase with no required
  dependencies. This report did not audit every optional platform library or
  prove that the claim holds at every commit.
- **Size:** “Complete app” in the integration guide means the stripped
  MinSizeRel `hello` executable, reported at about 240 KB. The complete library
  build is reported at about 930 KB. These are CUI's definitions and build
  configuration, not interchangeable with Kryon's current project bundles.
- **Frame timing:** CUI reports `cui_frame` build/layout/paint at 0.09–0.75 ms,
  with most results below 0.25 ms. Its full software repaint is a separate
  metric: 3.4–19 ms at 2480×1640 with four threads. Quoting only the sub-0.25 ms
  figure as “rendering” would be dishonest.
- **Benchmark method:** CUI's documented suite uses warmup and measured frames,
  randomized input, soak behavior, software rasterization loops, a shared
  cloud x86-64 machine, and allocation/memory checks. Its benchmark source uses
  30 warmup and 200 measured frames; any Kryon comparison must disclose all
  differences rather than borrow those numbers.
- **Platform:** CUI's platform table covers Windows, macOS, and Linux desktop.
  Its live web documentation uses WebGL2. No Android target and no no-WebGL web
  path was found in the primary documentation. This is a platform opportunity,
  not evidence that Kryon is faster or smaller.
- **Rendering model:** CUI produces a triangle draw list for one shader and has
  a software reference renderer. It is a compact, explicit rendering boundary;
  its numbers still cannot be transferred to Kryon's retained tree, damage, and
  presenter work without running the same scenes on the same hardware.

No performance or size superiority over C99/CUI is claimed here. In particular,
there is no current evidence that Ziran-generated code is faster or smaller than
CUI. The immediate objective is a coherent, testable architecture; only the
pre-registered procedure below can support comparative claims.

## Current Kryon architecture

### Portable UI core

Kryon already keeps widget behavior, composition, layout, input routing, and
paint production in Ziran:

- `src/ui/tree.zi:113` keeps committed and submitted tree state separate.
- `src/ui/tree.zi:308` uses fixed label storage rather than per-label heap
  allocation.
- `src/ui/tree_draw.zi:28` records frame input and damage.
- `src/ui/paint_queue.zi:113` owns paint pools and snapshots.
- `src/ui/paint_queue.zi:1571` retains draw data and damage information.

These are the right assets to preserve. They provide frame-boundary ownership
and bounded storage, but they are not yet a measured CUI competitor.

### Web without WebGL

A maintained Canvas2D path already exists:

- `src/backend/page_core.zi:516` selects Canvas2D contexts.
- `src/backend/canvas_frame.zi:114` retains frame damage and presentation work.
- `src/backend/canvas_raster.zi:323` can raster triangles through Canvas2D.
- `src/backend/page_texture.zi:338` has a non-WebGL texture fallback.
- `src/backend/page_mesh.zi:138` contains the only observed WebGL-oriented image
  mesh path and can be made optional or removed for a Canvas2D-only gate.

This is stronger than CUI's documented live-web position because it does not
require WebGL. It is not yet evidence of better speed: Canvas2D performance and
its CPU/GPU backing depend on the browser and device.

### Desktop

The current desktop path combines:

- SDL window/events through `src/backend/Desktop/module.zi:1`,
  `src/backend/desktop_runtime.zi:1`, and `src/backend/desktop_loop.zi`.
- Cairo rasterization and host text through `src/backend/cairo_raster.zi:43`.
- SDL pixel presentation through `src/backend/pixmap_sdl.zi:1`.
- The deterministic portable rasterizer in `src/backend/pixmap.zi`, including
  damage clipping, row parallelism, and caller-owned buffers.
- Bundled-font raster text in `src/backend/pixmap_text.zi:6`, with fixed glyph
  and edge scratch arrays at `src/backend/pixmap_text.zi:29`.

Cairo currently provides convenient host FreeType/Pango shaping. Removing Cairo
is possible only after a general text capability can preserve metrics, Unicode,
bidi, ligatures, fallback faces, clipping, and IME caret behavior. The existing
host contracts are `src/ui/font_metrics.zi:3` and `src/ui/raster_text.zi:6`.

### Android and raylib

Raylib is not merely one optional renderer on Android. It is structural:

- `ziran.toml:9` pins raylib as a source dependency.
- `ziran.toml:280` defines the raylib renderer artifact and many codec/font
  outputs.
- `src/backend/android_run.zi:6` documents raylib NativeActivity owning the
  window.
- `src/backend/android_surface.zi:98` reads raylib `CORE` state.
- `cmake/KryonAndroid.cmake:167` links `raylib`, Android, log, OpenSLES, EGL,
  and GLESv2 and wraps lifecycle, EGL, and asset file access.
- The researched source has 156 direct raylib bindings across eight backend
  files, including `src/backend/android_run.zi`,
  `src/backend/android_surface.zi`, `src/backend/raylib_run.zi`,
  `src/backend/raylib_runtime.zi`, `src/backend/raylib_frame_render.zi`,
  `src/backend/bundle_raylib.zi`, `src/backend/audio_sink_raylib.zi`, and
  `src/backend/media_audio_raylib.zi`.

Complete removal requires replacing NativeActivity/window ownership,
`ANativeWindow` presentation, lifecycle, EGL or software presentation, touch,
key, IME, clipboard, asset access, fonts, image decode, audio, notifications,
secondary windows where applicable, timing, error reporting, and tests. A
draw-call substitution is insufficient.

### Styling

CSS remains separable:

- The CSS package parses a documented closed subset and installs values into
  Kryon's typed style table. It is whole-or-not-at-all at parse time.
- Compiled style packs can populate the same typed table without the parser.
- Package-owned files are under `kryonlabs/packages/css/src`; its parser,
  vocabulary, migration, and sheet code should stay optional.

The smallest dependency strategy is therefore compiled style packs in the core
surface and the CSS parser as an optional package, not a runtime requirement.

### Existing `.zx` behavior

Kryon already has safe whole-app replacement:

- `tools/zx.zi:398` opens and prepares a replacement before switching.
- `src/backend/bundle_watch.zi:1` keeps immutable bundles and rejects incomplete
  pointers.
- `tests/zx_reload_test.py:38` verifies immutable hashes/sequences and rejects
  rollback.
- Replacement is frame-boundary atomic, but the new module instance starts with
  fresh global state.

The hosted protocol is different from independently swappable components:
`src/backend/widget_host.zi:1` exposes a fixed generated widget-call surface and
`src/backend/widget_codec.zi:1` implements its bounded value codec. It does not
provide independent component identity, version negotiation, state migration, or
replacement.

## Ziran capabilities and missing general contracts

Verified current Ziran behavior in `/mnt/storage/Projects/ziranlang/ziran/docs/ZX.md`:

- Saved IR and bundle capabilities are verified. Foreign-call signatures are
  explicit, and native hosts can inspect and bind required capabilities
  (`docs/ZX.md:71`–`99`).
- Handles are opaque and host-owned; mutable slices are copied back only after
  successful synchronous calls (`docs/ZX.md:106`–`118`).
- Bundle instances retain their own globals (`docs/ZX.md:124`).
- Partial modules can compose disjoint declarations, but composed library and
  add-on code execute with one independent global-state instance
  (`docs/ZX.md:173`).
- Linux/Android native execution currently uses checked generated C plus a
  TinyCC cache (`docs/ZX.md:217`).
- Browser linked code and the wasm tier have important one-instance and scalar
  subset restrictions (`docs/ZX.md:255`).

General upstream work required, with no Kryon widget names or UI special cases:

1. Stable module/component identity and semantic version metadata.
2. Typed capability and ABI compatibility across replacement, including record
   and procedure revisions.
3. Independent instance state for multiple components, not one global state per
   composed library/add-on.
4. Explicit host/component ownership rules and opaque handle lifetime checks.
5. State export, import, migration, rollback, and stale-reference prevention.
6. Pause/resume or transactional replacement at a safe program boundary.
7. Partial-module composition with declaration replacement that preserves
   unrelated declarations.
8. Loader overhead instrumentation: verify, instantiate, initialize, migrate,
   resume, and first-frame timings.
9. A production wasm/linking story that removes the current one-instance and
   scalar-subset barriers for multi-component UIs.
10. Explicit dev-runtime/runtime split so TinyCC and compiler machinery are not
    shipped with a minimal production player.

Likely Ziran owners are `zir_bundle.c/h`, `zir_zx_host.c/h`,
`zir_zx_native.c/h`, `zir_zx_wasm.c/h`, `std/bundle_host.zi`, and the existing
bundle/ZX tests under `tests/`. Exact implementation files must be selected from
a clean Ziran tree when work starts; this dirty research snapshot does not
authorize edits there.

## Existing size evidence

The following files were inspected, not rebuilt or executed. They are existing,
uncommitted outputs and are not performance evidence:

| Artifact | Raw bytes | gzip bytes | Interpretation |
|---|---:|---:|---|
| `build/bin/kryon` | 51,264 | 21,966 | Project tool only; not a complete app. |
| `build/bin/zx` | 6,103,208 | 1,797,521 | Development/player runtime; compiler machinery makes size inference invalid. |
| `build/zx-raylib/zx` | 13,477,224 | 4,539,453 | Raylib development player; not a minimal production target. |
| Canvas player wasm | 2,790,458 | 1,076,379 | Current no-WebGL-capable web baseline, not minimized. |
| Canvas player HTML | 956 | 589 | Wrapper only. |
| Project options bundle | 106,986 | 24,891 | Options payload, not whole app. |
| Hello bundle | 604,104 | 168,584 | Bundle bytes, not host/runtime size. |

At inspection, no relevant source file was newer than the latest artifacts
checked. Freshness is not correctness or optimization. A future minimal target
must report executable/wasm plus required assets and host wrapper as separate
numbers under the pre-registered size definition.

## Bottleneck inventory

These are evidence-backed structural risks, not measured hotspots:

- **Reload state:** whole-app replacement reinitializes module globals, losing
  UI/application state and making component-level hot replacement impossible
  without the Ziran lifecycle work.
- **Layout and tree work:** fixed storage prevents per-widget allocator churn,
  but overflow and reconciliation costs at large trees remain unmeasured.
- **Paint snapshot/copy:** retained pools and damage are promising, but the
  number of copied commands, snapshots, and damage rectangles needs counters and
  traces before optimization.
- **Raster:** `src/backend/pixmap.zi` has deterministic integer raster,
  parallel rows, and clipping. Full repaint and text-heavy damage costs need
  device measurements.
- **Text:** fixed glyph caches are predictable but can thrash with many type
  faces, scripts, or subpixel positions. Cairo removal must not regress shaping
  correctness.
- **Web present:** Canvas2D removes the WebGL requirement, but browser command
  conversion, `ImageData` uploads, compositing, and damage merging must be
  measured independently.
- **Android present:** `src/backend/render_dispatch.zi:221` already overlaps
  render work, but raylib owns the surrounding event/present lifecycle. That
  overlap is not enough to assess input latency.
- **Dependencies:** codecs, audio, optional GPU, and the CSS parser can all be
  decoupled. Font/image support and platform capability definitions are the
  actual functional tradeoffs.
- **Development payload:** TinyCC/compiler-inclusive players dwarf the claimed
  production app target. Production and development outputs need separate size
  gates.

## Smallest coherent Ziran-first architecture

### 1. Shared portable core

Keep behavior, tree reconciliation, layout, input, accessibility, styling
contracts, and paint production in `.zi`. The output to every presenter is a
bounded retained display list plus damage and presentation metadata. Widget
behavior never moves into a browser, Android, SDL, or Cairo host.

### 2. General host capability ABI

Define one typed, non-UI-specific platform surface:

- window lifecycle and visibility;
- pointer, key, wheel, touch, text, IME, and composition events;
- font metrics, shaping, caret measurement, and glyph rasterization;
- image decode and cache lifetime;
- clipboard, files, and notifications;
- presentation timing, vsync where available, and frame pacing;
- optional audio and secondary-window capabilities.

Presenters implement only drawing/presentation. Kryon consumes capabilities as
ordinary Ziran imports; Ziran does not learn widget names.

### 3. Platform presenters

- **Web:** Canvas2D is the required path. Keep command translation and retained
  damage; make WebGL mesh generation optional and exclude it from the minimal
  gate. Use the deterministic pixmap renderer as a pixel oracle, and only add a
  shared `ImageData` presenter if Canvas2D measurements justify it.
- **Desktop:** use the shared pixmap renderer and a small SDL window/event/present
  host initially. Remove Cairo only after the general text capability reaches
  parity. Direct Wayland/X11 can be a later measured decision, not a prerequisite.
- **Android:** replace raylib with a thin NativeActivity/`ANativeWindow` host.
  Start with software presentation through the shared renderer; add EGL/GPU only
  behind the same presenter interface if measurements require it.

SDL is therefore optional but initially useful on desktop. Raylib and Cairo are
removable. The CSS parser is optional. Codec dependencies become format-specific
capabilities rather than global requirements.

### 4. `.zx` components

After the Ziran lifecycle work, a Kryon component is a small verified module with
typed props/events, semantic identity/version, independent state, and a
frame-boundary transaction. State belongs to the component or its declared host
capability, never to a globally shared module slot. Whole-app reload remains the
fallback during migration.

### 5. Self-hosting boundary

Self-hosting Ziran/Kryon is a long-term goal, not a prerequisite for this
architecture and not a performance claim. The immediate system still uses the
Ziran compiler for development and emits verified bundles or native code for
production. A self-hosted compiler should be justified by workflow and dependency
goals after minimal runtime and reload measurements exist.

## Removable versus essential

### Essential

- `src/ui/*`: retained tree, layout, input, widget behavior, accessibility, and
  style contracts.
- `src/backend/pixmap*`: deterministic software raster, density, blend, image,
  and bundled text foundations.
- `src/backend/canvas_frame.zi`, `canvas_raster.zi`, and the non-WebGL portions
  of `page_core.zi`/`page_texture.zi`.
- General host capability definitions and platform event/lifecycle adapters.
- Font metrics/shaping contracts and compiled style packs.
- `tools/zx.zi` and `bundle_watch.zi` for verified reload, once generalized.

### Removable or optional

- All `src/backend/*raylib*` modules, `mk/raylib-library.mk`,
  `mk/zx-raylib.mk`, `cmake/KryonAndroid.cmake` raylib/EGL wrapping, and the
  `renderer-raylib` artifact after Android replacement lands.
- `src/backend/cairo_raster.zi` after host text parity and desktop pixmap
  presentation are proven.
- `src/backend/page_mesh.zi` and WebGL texture paths in the Canvas2D-only target.
- CSS parser package; compiled style packs remain.
- AVIF/WebP/AOM/Opus/audio/image dependencies when the corresponding optional
  capability is absent.
- TinyCC/compiler-inclusive player components in production artifacts.
- Legacy compatibility wrappers, if any are found during implementation; the
  owner accepts breaking changes.

## Staged implementation contracts

Each later stage depends on the earlier gate. Every build command must use the
shared build queue. Visual or window tests must use a private display and must
not touch the owner's live desktop.

### Stage A — baseline and capability audit

**Owner:** Kryon.  
**Files:** `src/backend/render_dispatch.zi`, desktop/pixmap/canvas/Android host
files, `tests/host_frame_snapshot_test.*`, `tests/backend_render_benchmark_test.py`,
`tests/gpui_render_benchmark_test.py`, and new benchmark manifest/trace fixtures
under `tests/`.  
**API:** no public API change. Add counters and identical scene/oracle fixtures.  
**Done:** one command reports p50/p95/p99 and counts for composition, layout,
paint, raster, present, damage, input queue latency, allocation, and peak memory
on desktop headless; pixel output matches the oracle. Existing Canvas2D, pixmap,
and Android build gates still pass.  
**Tests:** targeted snapshot, pixmap parity, canvas build, and Android build-only
tests through `build-queue`; no device run in this stage.

### Stage B — general Ziran component lifecycle

**Owner:** Ziran first, then Kryon.  
**Ziran files:** `zir_bundle.c/h`, `zir_zx_host.c/h`, `zir_zx_native.c/h`,
`zir_zx_wasm.c/h`, `std/bundle_host.zi`, and focused additions under
`tests/bundle_*`, `tests/zx_*`, and `tests/spec/`.  
**Kryon files:** only after the Ziran API lands: `tools/zx.zi`,
`src/backend/bundle_watch.zi`, `src/backend/widget_host.zi`,
`src/backend/widget_codec.zi`, and reload tests.  
**API:** general module identity/version, typed capability compatibility,
independent instances, state export/import/migration, safe stale-reference
failure, and pause/resume or transactional replace. No Kryon widget special case.  
**Done:** Ziran tests prove two independent replacements concurrently, state
migration success and rollback, ABI mismatch rejection, stale-reference rejection,
and browser/native constraints. Kryon whole-app reload remains unchanged until
this gate is green.  
**Tests:** Ziran bundle/ZX integration suite plus a Kryon two-component reload
test that preserves unrelated component state and measures verify/instantiate/
migrate/resume/first-frame separately.

### Stage C — desktop pixmap presenter and Cairo removal

**Owner:** Kryon.  
**Files:** `src/backend/pixmap.zi`, `pixmap_sdl.zi`, `pixmap_text.zi`,
`cairo_raster.zi`, `desktop_runtime.zi`, `desktop_loop.zi`, font/text contracts,
and `tests/pixmap_*`, `tests/ziran_desktop_*`, text/IME tests.  
**API:** desktop consumes the shared host capability ABI; public widget APIs do
not change.  
**Done:** SDL window/event/present host uses only the shared pixmap renderer;
Cairo and Pango are absent from the target link; text, clipping, IME, density,
damage, and presentation tests pass on a private display.  
**Tests:** existing pixmap parity and desktop native tests, Unicode/bidi/
ligature/fallback/caret tests, frame snapshot equality where deterministic, and
Stage A benchmark on the same desktop hardware.

### Stage D — Android NativeActivity replacement and raylib removal

**Owner:** Kryon, with any generic platform capability fixed upstream in Ziran.  
**Files:** `src/backend/android_run.zi`, `android_surface.zi`,
`android_touch.zi`, `android_text_editor.zi`, `android_clipboard.zi`,
`android_fopen.zi`, `android_bundle_run.zi`, relevant notification/JNI modules,
`templates/droid/src/android_glue.zi`, `cmake/KryonAndroid.cmake`, and Android
tests under `tests/`.  
**API:** same host capability ABI; optional GPU presenter is not exposed as a
widget API.  
**Done:** NativeActivity and `ANativeWindow` lifecycle, input/touch/IME,
clipboard, assets, fonts/images, software presentation, and timing work without
raylib. APK/link metadata no longer contains raylib or its license payload.  
**Tests:** first an integrated Android build-only gate; then an explicit private
emulator or designated USB Moto device gate covering cold start, pause/resume,
rotation, touch, IME, clipboard, damage, frame pacing, and Stage A metrics.
Only after those pass remove all raylib files/artifacts and run the full gate.

### Stage E — Canvas2D-only web and dependency pruning

**Owner:** Kryon, plus upstream Ziran wasm work where needed.  
**Files:** `src/backend/page_core.zi`, `page_mesh.zi`, `page_texture.zi`,
`canvas_frame.zi`, `canvas_raster.zi`, `ziran.toml`, CMake renderer artifact
files, and `tests/zx_canvas_test.py`, `tests/renderer_artifact_test.py`,
`tests/renderer_web_probe.zi`.  
**API:** Canvas2D is the supported baseline; WebGL remains optional at most.  
**Done:** minimal web artifact contains no WebGL API/symbol/reference, no Node
tooling, and no dev compiler; Canvas2D damage and deterministic oracle tests
pass. Production player, project bundle, assets, and wrapper are reported as
separate sizes.  
**Tests:** artifact inspection, Canvas2D behavior in the existing non-live-desktop
harness, wasm/browser tier tests from Ziran, and Stage A web benchmark on the
chosen browser/device. Finally prune unused codecs/audio/legacy exports and rerun
desktop, Android, and web gates.

## Apples-to-apples benchmark procedure

No comparative number may be published until this procedure is pre-registered
and run.

1. **Define binaries exactly.** Compare complete runnable applications, not a
   library archive with a bundle or a development player. For each target report
   executable/wasm, required wrapper/JS, required assets/fonts, compressed
   transfer, installed APK size, and strip/LTO/optimization flags.
2. **Match features and scenes.** Use only a common primitive scene set that both
   systems can express identically: fixed viewport/DPI, identical fonts and
   Unicode text, colors, images, widget geometry, scroll/input scripts, and frame
   count. Any unsupported feature removes that scene from both sides, not just
   the slower side. Separately report a full Kryon stress scene as Kryon-only.
3. **Use identical hardware and environment.** Name computer model, OS/kernel,
   CPU/governor, memory, power state, browser version, Android device/API level,
   thermal state, battery/power connection, viewport scale, and background load.
   Never mix CUI's cloud x86-64 result with a local phone result.
4. **Use a pre-registered order.** Match or exceed CUI's 30 warmup and 200
   measured frames, randomize input order deterministically, repeat enough runs
   for variance, discard no failures silently, and save raw traces.
5. **Measure boundaries separately:** app input timestamp to event dispatch;
   composition/reconciliation; layout; paint/display-list production; raster;
   present/composite; end-to-end frame; reload verify/instantiate/migrate/
   resume/first-frame. Never compare CUI's frame-only metric with Kryon raster
   or present time.
6. **Report distributions:** p50, p95, p99, min/max, standard deviation, failures,
   allocations, peak RSS, cache misses where available, CPU energy only if the
   device counter is trustworthy. Averages alone are insufficient.
7. **Check correctness under load.** Use deterministic pixel oracles, input-state
   oracles, soak tests, randomized input, rotation/configuration changes on
   Android, and reload rollback. Performance of wrong pixels is invalid.
8. **Run each platform on that platform.** Desktop C/CUI versus Kryon on desktop;
   browser Canvas2D Kryon on the selected browser/device; Android Kryon on a named
   device. CUI has no documented Android/no-WebGL target, so those are Kryon
   capability results rather than cross-system wins.
9. **Publish limitations.** State compiler/toolchain versions, feature subset,
   unsupported scenes, and every deviation from CUI's published methodology.

## Current uncertainty

- No fresh performance benchmark was run; all existing timings are CUI's, not
  Kryon's.
- No fresh build was run; artifact freshness does not prove current-source
  correctness.
- The dirty Kryon/Ziran worktrees may change host tests and desktop behavior
  before implementation.
- CUI's dependency and performance claims were verified against its primary
  docs/source, not rebuilt in this environment.
- Android IME, clipboard, notification, audio, image, and multi-window scope
  needs a product decision before exact raylib replacement size can be predicted.
- Canvas2D browser backing and phone thermal behavior are unknown until tested
  on named devices.
- General component state migration semantics need an upstream Ziran design;
  no current API should be assumed stable.
- The long-term self-hosting path is intentionally separate from the minimal
  runtime plan and has no size/performance evidence yet.

## Recommendation

Do not port CUI's renderer into Kryon and do not add UI-specific loader rules to
Ziran. Preserve Kryon's retained tree, bounded paint pools, damage model, and
deterministic pixmap renderer; make Canvas2D the required web presenter; keep SDL
only as the initial desktop shell; replace raylib's Android ownership with a thin
NativeActivity host; make Cairo and the CSS parser removable; and first fix the
general Ziran module-instance/version/state lifecycle. Establish the benchmark
and pixel oracle before changing hot paths. Only that sequence can credibly aim
to exceed CUI on Android, no-WebGL web, desktop responsiveness, and hot-swappable
Ziran components without making unsupported C-versus-Ziran claims.
