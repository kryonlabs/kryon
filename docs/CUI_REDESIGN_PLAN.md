# Redesign Kryon around portable Ziran components

Kryon should match CUI's default quality, then exceed it with one canonical
Ziran-authored system: a single semantic token contract, independently
replaceable `.zx` families, shared runtime/render/style services, typed state
migration, atomic rollback, Android/desktop/Canvas2D hosts, and a static website
generated from those components. The verified corpus is **127 documented CUI
families**, not an ambiguous “about 170”; the dirty-checkout assessment is
**2 strong, 5 extend, 30 partial, 84 new, 2 package-promote, and 4 partial-package**—source findings, not release or parity-test claims
([component research](CUI_COMPONENT_RESEARCH.md),
[matrix](CUI_COMPONENT_PARITY.tsv)).

Kryon already has safe whole-bundle replacement and rollback, but not retained
or migrated public state for one independently changed component. CUI's sizes
and frame timings are upstream claims only; its separate full repaint metric can
reach 19 ms. Kryon must publish only its own distributions
([runtime research](CUI_RUNTIME_RESEARCH.md),
[CUI README](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/README.md#L315-L321)).

## One canonical default removes ambiguity and legacy

There is one improved default, not a CUI theme beside retained Classic.
`src/ui/theme.zi` owns semantic color, radius, type, spacing, motion, focus, and
shadow values; customization changes those values and metrics rather than
selecting another implementation. Customization spans semantic tokens, typed
style properties, selectors/classes/IDs/states, layout and geometry, fonts,
borders, motion, and per-component rules. Website chrome consumes a generated
bridge; the CSS parser is optional as a dependency, not removed from the product
([component research](CUI_COMPONENT_RESEARCH.md),
[website research](CUI_WEBSITE_RESEARCH.md)).

Plot/DataViews families are promoted once into `src/ui/`, imported selectively,
and their package-facing duplicates leave the dependency path. There are no
parallel widget APIs, theme runtimes, wrappers, or editor stacks: `/demo`
redirects after catalog migration and its old implementation/assets are deleted.

| Decision | Rule | Primary owner |
|---|---|---|
| Default theme | One CUI-compatible semantic contract | `src/ui/theme.zi` |
| Customization | Typed style properties plus optional CSS selectors/rules | `src/ui/stylepicker/stylepicker.zi`, CSS package |
| Component API | One public family concept | `src/ui/**` |
| Package promotion | Promote once, remove duplicate surface | `src/ui/**`, `ziran.toml` |
| Website examples | Generated static Ziran shots | `docs/site/**`, site scripts |
| Superseded editor | Redirect route, delete unused code/assets | `docs/site/**` |

## Fix the general runtime before scaling components

The first gate is upstream Ziran, not widget count. Ziran already supports
verified bundles, explicit foreign calls, opaque host handles, and module
composition. Standalone portable VM instances already have independent state;
the composed library/add-on path currently has one independent global-state
instance, which cannot host multiple independently replaced UI components.
Browser linked code also has one-instance and scalar-subset
constraints. Ziran needs general module identity/version metadata, typed ABI and
capability negotiation, independent instances, ownership/lifetime checks, state
export/import/migration, stale-reference rejection, transactional replacement,
partial declaration replacement, loader timing, a production wasm story, and a
dev/runtime split ([runtime research](CUI_RUNTIME_RESEARCH.md)).

### Stage A: baseline and capability audit

Kryon owns `render_dispatch.zi`, current desktop/pixmap/Canvas/Android hosts,
the snapshot/benchmark tests, and new trace fixtures. With no public API change,
it adds identical scene/pixel oracles and counters for composition,
reconciliation, layout, paint, raster, present, damage, input latency,
allocations, and memory. `build-queue -- python3 tests/backend_render_benchmark_test.py`,
then the identified snapshot/pixmap/Canvas/Android build gates, must report
p50/p95/p99 and counts while pixels match the oracle.

### Stage B: general component lifecycle

Ziran goes first in `zir_bundle.c/h`, `zir_zx_host.c/h`, `zir_zx_native.c/h`,
`zir_zx_wasm.c/h`, `std/bundle_host.zi`, and bundle/ZX/spec tests. It exposes
general identity/version, typed capability compatibility, independent instances,
state migration, stale-reference failure, and transactional replacement. No
Ziran API learns a Kryon widget name.

Only then does Kryon consume it through `tools/zx.zi`, `bundle_watch.zi`,
`widget_host.zi`, and `widget_codec.zi`. Public state versions editing/IME,
scroll, focus, open state, active item, drag, undo, and semantic animation
state; caches are disposable and data remains caller-owned. Successful migration
commits before the replacement frame; failure preserves the prior frame.

The Ziran suite must prove two concurrent independent replacements, successful
migration, rollback, ABI mismatch rejection, and stale-reference rejection. The
Kryon focused test is `python3 tests/zx_reload_test.py`, extended to two
components while an unrelated component remains live. It reports verify,
instantiate, migrate, resume, and first-frame separately.

### Stages C–E: presenters and dependency pruning

Desktop keeps the deterministic pixmap renderer and small SDL
window/event/present host. Stage C owns the pixmap/SDL/text/Cairo/desktop files
and tests; Cairo/Pango leave only after Unicode, bidi, ligatures, fallback,
clipping, caret, density, IME, damage, and presentation match privately. Run
`build-queue -- make pixmap-parity-test desktop-native-test`, then Stage A on
the same machine.

Android replaces raylib ownership with a thin NativeActivity/`ANativeWindow`
host, not a draw-call substitution. Stage D owns the Android runtime/surface/
touch/text/clipboard/file/notification modules, Android glue, and Cmake. It
preserves lifecycle, input/IME, clipboard, assets, fonts, timing, and software
presentation. Build-only tests precede a private-emulator or designated USB Moto
gate; raylib files/artifacts are removed only afterward.

Web makes Canvas2D required. Stage E owns page/canvas modules, `ziran.toml`,
renderer artifacts, and their three tests. The minimal artifact has no WebGL
symbol/reference, Node tooling, or development compiler; player, bundle, assets,
and wrapper sizes are separate. Run `build-queue -- make zx-canvas-test
renderer-artifact-test`, then browser-tier and Stage A web tests.

## Implement 127 families in serialized waves

`docs/CUI_COMPONENT_PARITY.tsv` is the issue tracker. Each row already supplies
family, status, current surface, exact gap, target file/public contract, and
acceptance test. Family workers own only those files. Shared `theme.zi`,
`style_picker.zi`, `ziran.toml`, `src/ui/modules.txt`, codec generators, and
fixtures are serialized by the foundation owner to protect the already dirty
active tree. Selective imports are mandatory: an app that does not request a
family does not carry it.

### Component waves

| Wave | Exact initial owners | Focused command and completion definition |
|---|---|---|
| 0 | `theme.zi`, `style_picker.zi`, then Stage B Kryon hosts/codecs | `make public-surface-check && python3 tests/cui_theme_contract_test.py`; every mode/accent/custom metric and reduced-motion tree/raster passes; replacement gate above passes |
| 1 | Extend `text_props.zi`, `text_widget.zi`, `button_props.zi`, `icon.zi`, `svg_icon_props.zi`, `image_props.zi`; add TSV basics/actions/inputs | `python3 tests/cui_basics_wave_test.py`; geometry, pointer, keyboard, loading/disabled, tooltip, formatting, and accessibility pass row by row |
| 2 | Existing selection/navigation/overlay owners plus canonical `date_picker.zi`, `month_calendar.zi`, `tooltip.zi`, and all TSV targets | `python3 tests/cui_controls_overlays_wave_test.py`; keyboard, focus, placement, boundary, delay, drag, locale, and reduced-motion contracts pass |
| 3 | Promote `table_view.zi`, `tree_view.zi`, `list_item.zi`, `kanban.zi`, `data_grid.zi` | `make public-surface-check && python3 tests/cui_data_wave_test.py`; stable IDs, virtualization, editing, selection, sorting, filtering, and accessibility pass without a package-only API |
| 4 | Create `src/ui/chart/{line,bar,donut,sparkline}.zi`, promote/extend `candlestick.zi`, then remaining chart rows | `python3 tests/cui_chart_wave_test.py`; axes, formatting, legends, empty data, hit testing, goldens, and reduced motion pass |
| 5 | Add planning/editors/media/motion/AI rows such as `chat.zi`, `markdown_view.zi`, `code_editor.zi`, `node_editor.zi`, `gantt.zi`, `timeline_editor.zi`, `text_effects.zi` | `python3 tests/cui_advanced_wave_test.py`; layout, content boundaries, keyboard, link/task/media results, virtualization, motion, and capability declarations pass |
| 6 | Only affected host adapters from Stages C–E | `make test`, then `make push-check`; host-specific behavior passes after nonvisual tree/input/state/style tests |

Every command above runs as `build-queue -- <command>`. The `cui_*` Python gates
and final performance command are proposed tests; the named `make` targets and
existing runtime tests already exist. Ziran Stage B, canonical theme/style
foundations, Stage A instrumentation, and disjoint static website metadata/shell
work can proceed in parallel. Only `.zx` hot-swap consumer integration waits on
the upstream lifecycle API; shared theme, manifest, codec, and module files stay
serialized, while family waves follow style/state contract availability.

New family APIs use `Family(session, props) -> FamilyResult`, or
`Family(session, props, state) -> FamilyResult` when public state survives frame
boundaries. Props describe input/presentation, results describe typed mutations
and actions, state is versioned and serializable, and business behavior remains
outside the component. A row marked `extend` changes its current canonical API;
a row marked `new` creates one canonical family module. No CUI facade is added.

## Measure components and complete apps separately

A component benchmark covers one independently imported family. It reports cold
load, verify/instantiate/migrate/resume, first semantic tree, first raster,
steady-state frame, memory, and replacement latency. An application benchmark
separately accounts for shared runtime, selected modules, production player,
fonts, icons, Unicode/emoji data, renderer, wrapper, assets, and application
payload. Exact CUI-compatible fonts can increase bytes; the report says so.

Comparisons must pre-register complete binaries, common scenes, fonts, viewport,
DPI, input script, hardware, OS, browser/device, toolchain, optimization flags,
run count, warmup, and every deviation. Match or exceed CUI's documented 30
warmup and 200 measured frames, preserve failures, and report p50/p95/p99,
min/max, standard deviation, allocations, peak RSS, and raw traces. Measure
input dispatch, composition, layout, paint, raster, present, end-to-end frame,
and replacement independently. Android and no-WebGL web results are Kryon
capability results because CUI documents no equivalent target.

The proposed report entry point is `build-queue -- python3
tests/cui_performance_benchmark.py --report build/scratch/cui/performance.json`.
Passing means the report exists, every accounting field is present, sample
counts or confidence are stated, and correctness oracles pass. It never means
every app is under 250 KB or that Ziran output beats C99.

## Build the website from the winning system

Primary navigation becomes Overview, Guide, Components, Showcase, Apps, and
Docs. `/widgets` remains the stable Components URL; `/components` may redirect
to it. The catalog has search, category/module counts, grid/list views, and
static light/dark images. It does not import `live-editor.js` or embed a
compiler, editable source pane, or preview iframe. `/demo` redirects after
migration and its old implementation/assets are deleted.

`scripts/gallery.py`, `examples/gallery/modules.toml`, and gallery sources
enrich every existing and new scene with category, module, honest summary,
source path, APIs, options, keyboard note, related entries, and alt text.
`scripts/build-gallery.py`, `scripts/build-site.py`, new
`examples/gallery/src/catalog-shot.zi`, and `examples/gallery/ziran.toml` render
each scene alone using `KRYON_CATALOG_SCENE`/`KRYON_CAPTURE_PATH` under private
headless/Xvfb conditions. Stable WebP/PNG outputs go under
`docs/site/assets/components/`.

`widgets.html`, `gallery.css`, `gallery.js`, and generated `gallery-data.js`
render cards and compact rows. Site JavaScript handles search, filtering, view
switching, theme selection, drawer behavior, and focus management only; it never
reimplements widget appearance, state, or behavior. `current.css` and
`current.js` consume generated tokens for a 56px topbar, 264px desktop sidebar,
880px docs measure, 76ch prose, 860px drawer breakpoint, and 640px compact
layout.

The focused commands are `build-queue -- make site` and
`build-queue -- make site-browser-test`. Done means manifest and scene counts
match, every image path and alt text resolves, search and filters work, keyboard
focus survives view changes, reduced motion is honored, the drawer closes on
navigation/scrim/Escape/resize, and no maintained page overflows at 320, 390,
768, 1024, or 1366px. The Python test also asserts that no live editor or demo
compiler path remains reachable. Cloudflare Pages deployment is owned by
`.github/workflows/pages.yml`, not GitHub Pages.

## Protect active work and report only completed gates

The Kryon and Ziran worktrees are dirty with unrelated active changes. Workers
must preserve them, avoid cleanup/revert, and report shared-file conflicts rather
than overwriting. Each coherent feature set completes focused tests, relevant
host suites, and integrated local gates before one coherent repository commit.
No source-generation-only milestone is committed. Final Kryon gates are
`build-queue -- make test` and `build-queue -- make push-check`; upstream Ziran
runs its own suite before the Kryon consumer changes.

The final report distinguishes source inspection, generation, nonvisual tests,
private visual tests, native/platform tests, local website tests, Cloudflare
deployment, and GitHub CI. It does not call matrix statuses parity-test passes,
CUI metrics Kryon measurements, or component counts total app size. These
research documents remain uncommitted because implementation has not started.

## Conclusion

The dependency order is fixed: audit current hosts, generalize Ziran component
identity/state/replacement, prove selective imports, adopt one canonical theme,
then consume the 127-row matrix in serialized waves while the website is
generated from the same components. Portable `.zx` remains primary and future
Ziran self-hosting remains a clean-language goal; C/Go output is never the
optimization deliverable. Parallel themes, package duplicates, raylib ownership,
Cairo dependence where general text can replace it, WebGL assumptions, and the
old website editor are removable obstacles. Kryon's credible advantage is a more
customizable, accessible, measurable, portable Ziran system—not another C copy.

## Execution status

Research and synthesis are verified. Active implementation sessions are
`glm-kryon-canonical-style-foundation` for canonical tokens, typed style, and
shared manifest integration; `glm-kryon-zx-component-host-integration` for
hot-ZX host integration; `glm-css-css-kryon-typed-style-parity` for typed CSS
parity; `glm-kryon-responsive-canvas-no-webgl` for the browser/Canvas runtime;
`glm-kryon-android-raylib-free-host` for the Android host; and the
`glm-kryon-cui-action-components` plus
`glm-kryon-cui-input-components` family waves, and
`glm-kryon-kryon-static-catalog-site` for the static catalog. Canonical theme, token-export
source/saved-ZX parity, retained Button raster behavior, style-cache parity,
and Android source/bundle checks have passed. A short-`TMPDIR` full `make
test` currently stops in native `tests/cursor_api_test.py` because its missing
asset request never changes the cursor; the backend input owner is diagnosing
delivery before any whole-suite rerun.

The static-site stream has generated the compact seven-page shell, retained
redirects including the former API route, and source-driven metadata for all 76
gallery scenes—64 original scenes plus the completed input and overlay waves—with
152 private-headless light/dark Ziran captures. Capture reuse is invalidated by
scene-source hashes, gallery-source hashes, capture-selection logic, and the
built gallery binary hash plus canonical exporter source/output hashes. The
current private-Chromium gate verifies files, links, redirects, canonical token
equality, metadata/source/API/options/status/alt, unique capture contents,
search/filter/details, grid/list, themes, keyboard focus, Escape, reduced
motion, and no overflow at all required widths. The only equal captures are the
inherited `Group` and `Screen` equivalence; the former local-index duplication
is gone. Subjective CUI visual comparison and full-bleed-versus-crop acceptance
remain unverified.

All 127-family parity remains incomplete. No original-Kryon redesign milestone
is complete. No commit, push, deployment,
integrated platform validation, or performance result is complete yet. Remaining
backend/component work includes portable replacement/state migration, all
component-family waves, package promotion, presenter/raylib/dependency pruning,
and meaningful platform/performance validation before coherent per-repo commits.
