# TSX migration: current work and verification matrix

The owner removed the pixmap application target. Current profile coverage is desktop, portable ZX, Canvas/web, Android and terminal; screenshots use the desktop renderer. Historical pixel-renderer benchmark evidence is preserved.

The pixmap application backend/profile, headless frame host and obsolete parity route are removed. Fresh and installed CLI rejection and C99/ZX build-route rejection pass; all four scaffolds and dashboard omit the old profile. Screenshots use the desktop renderer. The previously published Kryon f4bda473 tools were installed and byte-verified; current native guest-host changes and their matching provider installation remain pending. Tool installation does not install a consumer application. Final current-tree owning suites and consumer/profile checks remain open.

Owner scope: full breaking migration; this matrix is incomplete until every maintained source/import/submodule/CI consumer is classified and every required check is recorded. A pending row is not successful coverage.

## Current implementation

- The persistent full-scope goal is active, with no token budget. Ziran's generic TS/TSX frontend constructs common checked IR directly. The experimental `tsx2zi` command, sources, build/install target and local artifact are deleted; no generated `.zi` screen pipeline remains.
- Published compiler: [4ae21503](https://git.waozi.xyz/ziranlang/ziran/commit/4ae21503f282f77c4c83df81fcea70bbbb5392aa), pushed, CI not checked yet. All 269 retained local checks are covered: 268 passed in the full run; its sole generated-site failure was regenerated normally and passed its focused rerun. The compiler is unchanged since the earlier 89-root bootstrap and independent stage-two comparison. Entry-selected checked IR now preserves portable semantics without native-only folding or record-field pruning; the existing four notification guests produce identical source/saved-IR ZX bytes. The common IR preserves native signatures, ownership, generic package visibility and saved declarations. Bounded native pointer views, fixed-array references, scoped array/matrix captures, read-only byte inputs with writable typed host context, copied inline storage, caller-owned record views and mutually exclusive branch loans pass source/saved C, Go and ZX checks. Unsafe expired/readonly views remain rejected. Named nested owner clears and recursive helpers that do not write backing storage are checked generically. Named string and real compound initializers agree across C, Go, portable and native execution, including wide IDs. Thirteen duplicate commands are removed and two positive mutation fixtures are merged, preserving all fourteen rejection cases and distinct backend assertions. The shared native `std/guest_host` now owns assets, clock, process and file capability handling for each instance. Actual guest execution passes argument indexing, process units, shell NUL rejection, binary file reads and assets. All twelve installed tool payloads and shared guest modules match the current tree; the existing launcher is preserved. Final frontend, consumer and platform coverage remains open.
- The compiler tracks ordered must-clear view effects through wrappers and control-flow joins while retaining local and unrelated aliases. Two duplicate TSX warm-up invocations and one overlapping shallow rejection case are removed; the stronger nested-identity case remains. Breeze now passes the repaired application services and awaits dependency graph alignment; no lifetime bypass or reduced behavior is accepted. Mesh Client [05eed82a](https://git.waozi.xyz/ziranlang/mesh-client/commit/05eed82a77bfe728dea3132943574c8cfd5d92c4) and Lua [b659c954](https://git.waozi.xyz/ziranlang/lua/commit/b659c954adffd9f198af4246b70a9c7c2fdd95a1) publish their native view-lifetime repairs. Their complete existing local suites pass, including real Mesh crypto/key wiping and Lua Wasm/conformance plus all 17 Katalis mods; both are pushed, CI not checked yet. Final consumer pins remain pending.
- Root, four templates, CLI init/upgrade defaults and 103 example toolchain declarations select published `4ae21503` (IR 71). All 104 root/example locks were regenerated through normal package commands using canonical Git objects and agree with their manifests; templates do not retain locks. Public TextField state exports, metadata and duplicate-check removals are verified locally and prepared for publication; native guest-host integration remains a separate unverified change. The trimmed starter check passes creation/pins, app rendering, web checking, upgrades and guards. Both original snapshot digests were reproduced with the historical instruction; current light/dark renders change only that label, with identical geometry and semantics. Clean published-default starter builds await the new Kryon milestone. The pre-push run found an owned-buffer overwrite in native reload cleanup; that check passes after the focused fix. The remaining GPU step and reduced gamepad driver also pass, covering every pre-push component without repeating unchanged green work. Generic module-path forwarding and hosted-input cleanup retain prior source/saved-IR and Android evidence.
- Generic package boundaries now restrict native widget composition in 83 widget implementation files, including the Text DrawSession helper. Kryon internal callers and DataViews TSX source checking pass; an external Draw call and its re-export are rejected. Public measurement, policy, native widget types and custom drawing remain available. The public-surface and complete local pre-push gate pass. These boundaries are published in e2b0fbd8; the remaining owning suites are pending.
- Kryon's mixed-source build emits checked native entries instead of generated Ziran adapters. `window.tsx` runs on real desktop, Canvas and ZX hosts. Ordinary entry tests check native window mapping, opaque/transparent Web pixels and useful missing-host diagnostics. Import selection assertions pass for basic windows/widgets and minimal hosted bundles; plain labels omit optional component storage; the portable App.Run frame loop now samples a shared monotonic input clock for actual timing. Complete import-selection coverage remains pending.
- Native component storage uses the retained tree transaction and hosted protocol. Source/saved C/Go/ZX and sanitizer checks cover independent keyed instances, nested parents, reorder/removal, duplicate-key rollback, owned snapshots, queued actions, timers, cancellation and stale handles. Private native/browser hosts preserve sibling focus and pixels during replacement/rollback. Ordinary JSX component calls now pass with the normally resolved published compiler: two same-line calls keep independent state, keyed loops and nested parents retain the correct nodes, source/saved C/Go/ZX execution agrees and ZX bundles are byte-identical. Newly generated canonical and package-compiler C passes ASan/UBSan/leak detection. Eight actual portable App.Run runs cover normal and failed composition on source/saved bundles through the desktop player and native executable; timer ordering, cancellation, unmount cleanup and stale callbacks pass. Final component, async and resource-lifetime coverage remains open.
- Android mixed-source generation uses saved IR and explicit checked host roots. The current ARMv7 component APK builds with all five exact guest bundles and required JNI exports. The default TSX entry passes the normally selected published compiler, 119 saved modules, 105 generated C outputs and real ARM64 NDK/JNI link. Gradle asset checks cover directories/files, changed contents, removal and undeclaration. Current complete profile/device execution is pending; these builds establish no installation.
- Browser compiler transport checks actual TSX/Ziran through the common checker with authored diagnostics. Real Wasm/private Chromium tests pass props/host errors, recovery and stale-worker teardown. Source publication preserves authored TSX bytes. Complete gallery/editor integration and website publication remain pending.
- All 103 nested example manifests now select TSX; this source count is not complete profile coverage. Native desktop/ZX and saved-IR Canvas proofs cover input, editing, selection, navigation, containers, overlays, actions and media. The repaired current Canvas presenter passes 28 actual website selection/overlay interaction routes and 25 exact comparison frames, including Carousel artwork and resized TileMap sprites. Five live Carousel mouse states match exactly. The shared texture readback now copies JS pixels back into Wasm and respects straight versus premultiplied alpha. Countdown intentionally corrects its original hidden-days labels. Final published-pin, complete-profile and clean-project checks remain pending.
- Optional Plot and DataViews own their typed adapters and nine real shared TSX screens; the Kryon core does not import optional packages. All nine standalone examples pass source checking; 18 paint scenarios agree across source/saved C/Go/ZX, 62 original/current native frame pairs match including hover, selection, sorting, drag and scrolling, and the Plot cookbook and DataViews tree-to-table interactions pass native and portable execution. Gallery publication resolves their actual manifest roots, case-sensitive package aliases and authored bytes through normal package paths. Browser/Android profile checks and final published dependency refresh remain pending. Native reusable Game2D controls expose typed TSX bindings and keep their Ziran implementations package-private, published in 17d9260. Source checking, native and portable control tests, saved-IR replay, an external-consumer fixture and a chooser/pad interaction probe pass. Plot candle/band native call sites are migrated; actual codec runs pass source/saved C and ZX plus entry-linked saved-IR Go. DataViews Kanban now uses typed TSX slots and passes its full local native/saved-IR/ZX/Go/law checks. Email folder controls use the same typed Button and pass native/ZX/saved selection, disabled, retained click and paint probes. Plot, DataViews and Game2D now publish a coherent e265c54e/f4bda473 dependency graph with passing existing full local suites; Workbook publishes the same graph with its full native/service/round-trip/UI gate. Their Gitea pushes are complete, CI not checked yet. Remaining profile runs and consumer pins remain pending.
- All 91 typed adapter/helper source files at the prior stage passed checking against native widget declarations. The maintained cheat sheet contains actual TSX composition, native domain logic and an ordinary build; all six extracted blocks build, capture and pass the private-display driver test against the published pin. The shared native/hosted TextField binding now executes ordinary consumer TSX source/saved guests against source/saved native hosts: four 18-frame runs pass UTF-8 editing, selection, focus, IME hold/cancel/commit and platform text-request replacement. Native TSX source/saved C and Go runs also pass the same 18-frame oracle; both C forms pass ASan/UBSan/leak checks. Controlled-only TextField source/saved guests select only the ordinary capability and execute on both native hosts. Hosted evidence uses no-op raster; final visual/IME, full-widget, scaffold and documentation coverage remains pending.
- TextField binding now stages accepted edit and IME bytes before leaving the borrowed draw-result scope, then publishes its native state. The existing widget and rollback suites pass source/saved ZX, C and Go without new cases. Four identical portable re-executions are removed after byte-for-byte source/saved bundle comparison; unique assertions and backend coverage remain.
- Stock TypeScript editor checking remains incomplete. The native reference/slice extensions have been replaced by typed intrinsics with ordinary TypeScript syntax. Stock TypeScript 6.0.3 parses the 105 Kryon adapter/dashboard/template sources, 101 Gallery/shared-screen sources, 25 Plot/DataViews/Terminal sources, 50 Harmony/Jobs/Social modules and 93 Breeze sources without syntax errors; this is not complete declaration or runtime evidence. Consumed JSX expressions now call the same typed component directly; the Gallery follow-up changes 134 such expressions across 67 files while retaining 428 actual JSX composition elements. Both production Gallery desktop and Web roots pass native source checks. The explicit native TypeScript declaration-generation workflow is published in [26bf07e3](https://git.waozi.xyz/ziranlang/ziran-vscode/commit/26bf07e3def2aea7a385e85b041fea450557901c), with Code dependency update [4a95e9db](https://git.waozi.xyz/harmony/code/commit/4a95e9dbdf1d1d81d30967ac922727ea7c404c92), both pushed, CI not checked yet. Installed extension 0.7.6 is byte-verified. A fresh private installed host preserves ordinary TypeScript diagnostics, makes zero compiler calls on open/hover/edit/save, generates declarations on explicit Check/Run/Preview, invalidates failed results and reports wrong props/callbacks/ordinary errors. The owner's editor was not reloaded. The new checked-IR declaration generator passes the retained API check and actual stock TypeScript server resolution, generic/callback/reference/wide/UTF-8 typing, negative props/action diagnostics and repair. Its actual 49-module Kryon declaration graph passes stock TypeScript 6 semantic checking without validation suppression. The current compiler stage is published in `dc3ecf0a`; whole-application editor coverage remains open. Current source fixes checked enum constants, native zero-initializer input types, scoped JSX children and native collection metadata. Actual desktop/Web Gallery and Breeze Habits declaration generation succeeds. The fresh desktop Gallery graph passes strict stock TypeScript checking with 192 authored modules, 1,869 declaration files and zero diagnostics, including the corrected native re-exports, Props and TextView declarations. The fresh Web graph also passes with 196 authored modules, 1,784 declarations and zero diagnostics. Reserved parameter labels, native constant/procedure name collisions, native record initialization and state writes retain their checked types. Source and saved-IR requester projections and the ordinary Web native source check pass. The installed stock TypeScript server and plugin proof reports zero positive diagnostics, 13 intended negative diagnostics, bigint hover and successful repair. Native three-part generic type qualification remains unsupported; the editor projection supports its namespace declarations. Saved TSX declaration generation requires the recorded authored files for import spellings. The published compiler complete gate passes; complete consumer/editor coverage remains pending. No duplicate schemas or suppressed diagnostics count as completion.
- The existing frontend correctness fixture runs under QEMU on AArch64 Linux/Android. ARMv7 Linux/Android AOT still rejects its printing service with `assembly.gap.machine-platform-service-unavailable`; this is unresolved coverage, not a Kryon host result. Physical Alpha's extended matched-renderer/state/input/keyed benchmark campaign remains pending.

## Required coverage

| Area | Concrete final check | Status |
| --- | --- | --- |
| Frontend and interoperability | `source + saved ZIR + ZX/native parity; wrong props/actions reject at authored spans` | Pending |
| Component lifetime | `independent instances, keyed insertion/reorder/removal, duplicate keys, cleanup and stale callback rejection` | Pending |
| Input and tree | `private-display keyboard/pointer/touch/editing/IME, focus, clipping, semantics and retained idle allocations` | Pending |
| Kryon owning checks | `make test; make laws; make sanitize-test; make docs-check; make push-check` | Last published f4bda473 passed pre-push. Current trimmed templates and toolchain-pin checks pass. The initial pre-push command failed `text-hook-hosts-ir-test` on an owned guest-buffer overwrite. Its focused rerun passes after repair; the otherwise completed source/public-surface/docs/desktop/gamepad routes and subsequently passing GPU step cover all pre-push components. The reduced gamepad driver also passes its focused rerun. Native player behavior verification remains separate and open. Remaining full owning suites pending. |
| Ziran owning checks | `make check (every retained check); make check-bootstrap` | Published `fac8a396`: all 269 checks executed, zero reused; all 89 refreshed bootstrap roots and independent stage-two reproduction pass. Normal user installation passes and all 12 installed tools match the fresh build. |
| Supported hosts | `desktop, ZX, Canvas/web, Android and terminal build/run on each declared supported profile` | Pending |
| Clean scaffolds | `kryon new app/pages/web/tui; lock/build/run without local overrides or warmed dependency cache` | Pending |
| Removed route | `old .zi application entries/API reject with migration diagnostic; no prototype command/recipes remain` | Project parser rejects `.zi`/`.ts` UI entries with a TSX migration diagnostic; retained source and portable options checks pass. Native `.zi` profile hosts remain valid. Existing project fixtures are migrated in source; full CLI/runtime checks and removal of the old public composition export surface remain pending. |
| Alpha performance | `matched real-renderer/state/input/keyed/list/app study; >=9 pairs; raw samples, identities, inclusive IQR and confidence intervals` | Pending |

## Kryon examples

Standalone entries outside the per-widget project directories:

| Entry | Authored surface | Current build/run evidence |
| --- | --- | --- |
| `examples/window.tsx` | TSX JSX calling the actual typed native window declaration | Native C, Go, C/Web and portable ZX builds pass. Ordinary project route runs its Canvas page with opaque/transparent pixel assertions and opens its native desktop window on private Xvfb. Portable native execution through the actual desktop player and AOT executable passes; remaining applicable checks pending. |
| `examples/hello.tsx` | Actual TSX Text composition in the native App.Run frame | Original and migrated native desktop captures match all 320x160 RGBA pixels. Go, C/Web and portable ZX builds pass. The ordinary pinned project-entry check verifies actual Canvas pixels, transparency and portable ZX entry; remaining applicable checks pending. |
| `examples/hosted_surface` | Actual TSX screen and events; native state/smoke callbacks retained | Default and combined-smoke ZX source checks pass. Existing `make zx-surface-test` nine presentation/scheduling variants await serialized runtime verification. |
| `examples/motion_chart` | Actual TSX screen and event wiring; native plotting/tween retained | C source check passes. Existing motion-chart build and private-display week-switch/render behavior pending. |
| `examples/loading_demo` | Actual TSX loading composition; native argv/palette/host retained | C source check passes. Existing explicit native loading-demo build/run and argument behavior pending. |
| `examples/desktop_image`, `examples/desktop_time_picker` | Native host/input harnesses retained; imported `demo/image_demo.tsx` and `demo/time_picker_demo.tsx` own actual screen composition | C checks pass for both TSX screens and both native harnesses. Existing `desktop-image-test` source/saved parity and seven `time-picker-shots` scenes pending. |
| `examples/zx_libraries.zi` | Native static/dynamic ABI fixture, retained | Classification confirmed; existing library linking/behavior checks remain required. This fixture is not application TSX coverage. |

| Project | Current entry | Declared profiles | Final build/run |
| --- | --- | --- | --- |
| `examples/actionmodal` | `actionmodal.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pass 108 group enabled/disabled interaction assertions and 92/96 exact baseline captures, with four intentional disabled-appearance corrections. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/alert` | `alert.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/avatar` | `avatar.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/badge` | `badge.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/bandchart` | `bandchart.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/banner` | `banner.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/box` | `box.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/breadcrumb` | `breadcrumb.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens; applicable native interactions pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/button` | `button.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/buttongroup` | `buttongroup.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens; applicable native interactions pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/candlechart` | `candlechart.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/card` | `card.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/carousel` | `carousel.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/checkbox` | `checkbox.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas captures and interaction pass, including controlled editing and corrected radio selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/checklist` | `checklist.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens, including active and disabled editing/selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/chip` | `chip.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/collapsible` | `collapsible.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pixels and interactions pass, including Scroll wheel bounds, focus reveal and idle stability. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/colorpicker` | `colorpicker.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin checks and desktop/ZX/saved-IR Canvas rendering and 168 group interaction assertions pass. Current website, complete-profile and clean-project checks pending. |
| `examples/column` | `column.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/commandpalette` | `commandpalette.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual shared TSX composition; all four declared profiles source-check. Eighteen geometry cases agree across source/saved C, Go and ZX. Private-display native and portable input tests cover typing, confirmation, Escape, drag, grace and reorder: 123 original/current frame pairs have no unexpected differences; intentional open/close corrections are recorded. Canvas, final published-pin and complete-profile checks pending. |
| `examples/confirmbutton` | `confirmbutton.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin source, native desktop, ZX and saved-IR Canvas rendering/input pass, including corrected ConfirmButton placement and portable timing. Final current-pin/profile and clean-project checks pending. |
| `examples/confirmpopover` | `confirmpopover.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual shared TSX composition; all four declared profiles source-check. Eighteen geometry cases agree across source/saved C, Go and ZX. Private-display native and portable input tests cover typing, confirmation, Escape, drag, grace and reorder: 123 original/current frame pairs have no unexpected differences; intentional open/close corrections are recorded. Canvas, final published-pin and complete-profile checks pending. |
| `examples/contextmenu` | `contextmenu.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pass 108 group enabled/disabled interaction assertions and 92/96 exact baseline captures, with four intentional disabled-appearance corrections. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/copybutton` | `copybutton.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin source, native desktop, ZX and saved-IR Canvas rendering/input pass, including corrected ConfirmButton placement and portable timing. Final current-pin/profile and clean-project checks pending. |
| `examples/countdown` | `countdown.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/counter` | `counter.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Authored TSX count updates 0→1→2 match all original pixels, labels and geometry on real desktop C, desktop ZX and Canvas/Wasm. Clean published-pin and portable-web checks pending. |
| `examples/dashboard` | `src/app.tsx` | `desktop` (desktop/c99), `portable` (desktop/zx), `web` (canvas/c99) | Composition and preview fixtures authored in TSX. Preview-only CLI checks pass; responsive native build exposed captured-slice C precedence, generic fix under verification. Final source/build/input/profile gates pending. |
| `examples/discreteslider` | `discreteslider.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens, including active and disabled editing/selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/dismissibleoverlay` | `dismissibleoverlay.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pass 108 group enabled/disabled interaction assertions and 92/96 exact baseline captures, with four intentional disabled-appearance corrections. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/dpad` | `dpad.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin source, native desktop, ZX and saved-IR Canvas rendering/input pass, including corrected ConfirmButton placement and portable timing. Final current-pin/profile and clean-project checks pending. |
| `examples/dropdown` | `dropdown.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin checks and desktop/ZX/saved-IR Canvas rendering and 168 group interaction assertions pass. Current website, complete-profile and clean-project checks pending. |
| `examples/emptystate` | `emptystate.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/fieldset` | `fieldset.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/flow` | `flow.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/gallery` | `src/app.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Desktop and website screen composition authored in TSX; current desktop/Web source checks and actual desktop C99 native link plus assets pass. Private Xvfb keyboard and pointer checks pass: selection/focus survives theme and disabled changes; Badge, Avatar and Keycap render in light/dark captures. Fresh browser compiler/editor/runtime, final pins and complete-profile checks pending. |
| `examples/grid` | `grid.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/gridmulti` | `gridmulti.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pixels and interactions pass, including Scroll wheel bounds, focus reveal and idle stability. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/group` | `group.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/guidedtour` | `guidedtour.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual shared TSX composition; all four declared profiles source-check. Eighteen geometry cases agree across source/saved C, Go and ZX. Private-display native and portable input tests cover typing, confirmation, Escape, drag, grace and reorder: 123 original/current frame pairs have no unexpected differences; intentional open/close corrections are recorded. Canvas, final published-pin and complete-profile checks pending. |
| `examples/holdbutton` | `holdbutton.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/hovercard` | `hovercard.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual shared TSX composition; all four declared profiles source-check. Eighteen geometry cases agree across source/saved C, Go and ZX. Private-display native and portable input tests cover typing, confirmation, Escape, drag, grace and reorder: 123 original/current frame pairs have no unexpected differences; intentional open/close corrections are recorded. Canvas, final published-pin and complete-profile checks pending. |
| `examples/image` | `image.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens; applicable native interactions pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/inlineedit` | `inlineedit.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas initial pixels match, with real activation/editing, commit/cancel and disabled behavior verified. Ordinary published-pin source checks pass; final complete-profile and clean-project checks pending. |
| `examples/joystick` | `joystick.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin source, native desktop, ZX and saved-IR Canvas rendering/input pass, including corrected ConfirmButton placement and portable timing. Final current-pin/profile and clean-project checks pending. |
| `examples/kanban` | `kanban.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/keycap` | `keycap.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/linechart` | `linechart.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/linechartcompare` | `linechartcompare.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/link` | `link.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin source, native desktop, ZX and saved-IR Canvas rendering/input pass, including corrected ConfirmButton placement and portable timing. Final current-pin/profile and clean-project checks pending. |
| `examples/listbox` | `listbox.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin checks and desktop/ZX/saved-IR Canvas rendering and 168 group interaction assertions pass. Current website, complete-profile and clean-project checks pending. |
| `examples/listitem` | `listitem.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/loading` | `loading.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/loadingscreen` | `loadingscreen.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Pending migration; `kryon build` and profile interaction/run |
| `examples/menu` | `menu.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pass 108 group enabled/disabled interaction assertions and 92/96 exact baseline captures, with four intentional disabled-appearance corrections. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/modalframe` | `modalframe.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pass 108 group enabled/disabled interaction assertions and 92/96 exact baseline captures, with four intentional disabled-appearance corrections. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/moneyfield` | `moneyfield.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas initial pixels match, with real activation/editing, commit/cancel and disabled behavior verified. Ordinary published-pin source checks pass; final complete-profile and clean-project checks pending. |
| `examples/navigationbar` | `navigationbar.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin checks and desktop/ZX/saved-IR Canvas rendering and 168 group interaction assertions pass. Current website, complete-profile and clean-project checks pending. |
| `examples/navigationbarconfig` | `navigationbarconfig.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin checks and desktop/ZX/saved-IR Canvas rendering and 168 group interaction assertions pass. Current website, complete-profile and clean-project checks pending. |
| `examples/onetimecode` | `onetimecode.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas initial pixels match, with real activation/editing, commit/cancel and disabled behavior verified. Ordinary published-pin source checks pass; final complete-profile and clean-project checks pending. |
| `examples/page` | `page.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pixels and interactions pass, including Scroll wheel bounds, focus reveal and idle stability. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/panedview` | `panedview.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pixels and interactions pass, including Scroll wheel bounds, focus reveal and idle stability. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/plotbars` | `plotbars.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/plotlines` | `plotlines.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/popup` | `popup.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pass 108 group enabled/disabled interaction assertions and 92/96 exact baseline captures, with four intentional disabled-appearance corrections. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/press` | `press.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin source, native desktop, ZX and saved-IR Canvas rendering/input pass, including corrected ConfirmButton placement and portable timing. Final current-pin/profile and clean-project checks pending. |
| `examples/progress` | `progress.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/progressring` | `progressring.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/radio` | `radio.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas captures and interaction pass, including controlled editing and corrected radio selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/reorder` | `reorder.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual shared TSX composition; all four declared profiles source-check. Eighteen geometry cases agree across source/saved C, Go and ZX. Private-display native and portable input tests cover typing, confirmation, Escape, drag, grace and reorder: 123 original/current frame pairs have no unexpected differences; intentional open/close corrections are recorded. Canvas, final published-pin and complete-profile checks pending. |
| `examples/row` | `row.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/screen` | `screen.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pixels and interactions pass, including Scroll wheel bounds, focus reveal and idle stability. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/scroll` | `scroll.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition and shared gallery source; desktop/ZX pixels and interactions pass, including Scroll wheel bounds, focus reveal and idle stability. Current Canvas rendering and assigned website interaction routes pass; complete-profile, published-pin and clean-project checks pending. |
| `examples/scrubfield` | `scrubfield.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas initial pixels match, with real activation/editing, commit/cancel and disabled behavior verified. Ordinary published-pin source checks pass; final complete-profile and clean-project checks pending. |
| `examples/segmentedcontrol` | `segmentedcontrol.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens, including active and disabled editing/selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/selectable` | `selectable.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens, including active and disabled editing/selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/separator` | `separator.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/shape` | `shape.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/sheet` | `sheet.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual shared TSX composition; all four declared profiles source-check. Eighteen geometry cases agree across source/saved C, Go and ZX. Private-display native and portable input tests cover typing, confirmation, Escape, drag, grace and reorder: 123 original/current frame pairs have no unexpected differences; intentional open/close corrections are recorded. Canvas, final published-pin and complete-profile checks pending. |
| `examples/shortcutrecorder` | `shortcutrecorder.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas initial pixels match, with real activation/editing, commit/cancel and disabled behavior verified. Ordinary published-pin source checks pass; final complete-profile and clean-project checks pending. |
| `examples/skeleton` | `skeleton.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/slider` | `slider.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas captures and interaction pass, including controlled editing and corrected radio selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/spinbox` | `spinbox.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas captures and interaction pass, including controlled editing and corrected radio selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/stack` | `stack.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real nested-package desktop, ZX and production saved-IR Canvas match original pixels; gallery and applicable input cases pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/stylepicker` | `stylepicker.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Shared scene composition is authored in TSX. Current source, full-profile interaction/run and final published pins remain pending. |
| `examples/svgicon` | `svgicon.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens; applicable native interactions pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/swipearea` | `swipearea.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/tabbar` | `tabbar.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens; applicable native interactions pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/tableview` | `tableview.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/taginput` | `taginput.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens, including active and disabled editing/selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/text` | `text.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; real native desktop and ZX match original pixels/behavior. Published-pin, clean-project and remaining profile checks pending. |
| `examples/textarea` | `textarea.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens, including active and disabled editing/selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/textfield` | `textfield.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas captures and interaction pass, including controlled editing and corrected radio selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/tilemap` | `tilemap.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; native desktop, ZX and real saved-IR WebGL Canvas comparison frames pass, including visible artwork/sprites and applicable interaction. Countdown corrects its original hidden-days labels. Final published pins, clean-project and complete-profile checks pending. |
| `examples/timeline` | `timeline.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Pending migration; `kryon build` and profile interaction/run |
| `examples/timepicker` | `timepicker.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; ordinary published-pin checks and desktop/ZX/saved-IR Canvas rendering and 168 group interaction assertions pass. Current website, complete-profile and clean-project checks pending. |
| `examples/titlebar` | `titlebar.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens; applicable native interactions pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/toast` | `toast.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Shared scene composition is authored in TSX. Current source, full-profile interaction/run and final published pins remain pending. |
| `examples/toggle` | `toggle.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas captures and interaction pass, including controlled editing and corrected radio selection. Published-pin, clean-project and remaining profile checks pending. |
| `examples/toolbar` | `toolbar.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition; desktop, ZX and saved-IR Canvas pixels match original screens; applicable native interactions pass. Published-pin, clean-project and remaining profile checks pending. |
| `examples/tooltip` | `tooltip.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Shared scene composition is authored in TSX. Current source, full-profile interaction/run and final published pins remain pending. |
| `examples/transition` | `transition.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Shared scene composition is authored in TSX. Current source, full-profile interaction/run and final published pins remain pending. |
| `examples/treeview` | `treeview.tsx` | `desktop` (desktop/c99), `web` (canvas/c99), `portable` (desktop/zx), `portable_web` (canvas/zx) | Actual TSX composition in owning optional package; source/saved C/Go/ZX paint agrees, original/current native frames and applicable interactions pass. Final published pins, Canvas, clean-project and complete-profile checks pending. |
| `examples/workout` | `src/workout.tsx` | `desktop` (desktop/c99), `android` (android/c99) | Actual TSX screen composition with native model retained; focused source check passes. Final native/Android build, interactions and published-pin verification pending. |

## Manifest dependency consumers

Fresh scan excludes vendor/build/recovery/archive trees. Maintenance status and actual UI use remain to be established from each scoped repository; manifest presence alone does not prove support.

| Project | Entry | Dependencies | Profiles | Final build/run |
| --- | --- | --- | --- | --- |
| `harness/benchmarks/challenges/scene3d/v1` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `harness/benchmarks/challenges/todo/v1` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `harness/harmony` | Native host with TSX screens | kryon, terminal, data_views, workbook, css | manifest default | Active maintained consumer; independent owner reports client and both maintained plugin UI sources migrated to TSX. Combined native host/source checks and the full native desktop compilation/link pass with the current compiler. Subsequent UI edits, standalone plugins, portable/Android runtime, final published pins and installation remain pending. |
| `harness/harmony/account` | `src/account_app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `harness/harmony/client/security` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `harness/harmony/plugins/jobs` | `src/app.zi` | kryon, data_views | manifest default | Pending classification, ordinary package build and run |
| `harness/harmony/plugins/social` | `src/app.zi` | kryon, css | manifest default | Pending classification, ordinary package build and run |
| `harness/music` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `harness/waozi/tests/kryon` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/browser` | `src/main.tsx` and actual TSX chrome; native model, input policy and document painting | kryon | Desktop; final declared profiles pending | Address editing, navigation, toolbar and viewport composition are TSX. The current native build and existing private-Xvfb window tests pass: address selection/copy/replace, loading/Stop/retry, HTML/CSS, bounded scripting, history, scrolling and resizing. Final published pins, commit/push and remaining declared profile coverage are pending. |
| `kryonlabs/explorer` | `src/app.zi` | kryon, data_views | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/github` | `src/app.zi` | kryon, css | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/gitnex` | `src/app.zi` | kryon, css | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/kryon/templates/app` | `src/app.tsx` | kryon, css | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/kryon/templates/pages` | `src/app.tsx` | kryon | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/kryon/templates/tui` | `src/app.tsx` | kryon | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/kryon/templates/web` | `src/app.tsx` | kryon | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/packages/css` | Native parser, style packs and policies; TSX behavior fixtures | kryon | native C/C++, Go, ZX, Android, browser export | Published in [20b7c95](https://git.waozi.xyz/kryonlabs/css/commit/20b7c95dbb678d799c5170c48372d29e7990ba29), pushed, CI not checked yet, with fac8a396/f4bda473 pins. Existing conformance, style packs, laws, browser light/dark (240 values each) and 2,000 ASan/UBSan fuzz cases pass; after correcting borrowed style/paint inspection scopes, all six native/motion fixtures pass. Nine duplicate C compilations are removed; assertions remain. Consumer pins remain pending. |
| `kryonlabs/packages/data-views` | `examples/data_views.tsx` | kryon | desktop, portable (desktop/ZX) | Typed table/tree/Kanban components and four example screens. Published [12725bb0](https://git.waozi.xyz/kryonlabs/data-views/commit/12725bb08aa7cb538e7e58ce6f1efab32d44b0c3), pushed, CI not checked yet. Normal published e265c54e/f4bda473 dependencies and the existing full local source/saved C, Go, ZX, render and law suite pass. Native composition stays package-private. Final declared profile runs and consumer pins remain pending. |
| `kryonlabs/packages/game2d` | TSX control components; native SDL/game logic | kryon | native C, portable ZX; SDL surface | Typed controls retain native implementations. Published [e81f31b3](https://git.waozi.xyz/kryonlabs/game2d/commit/e81f31b3e8d4e03fc5322e52ed2ce9a0583ca469), pushed, CI not checked yet. Normal published e265c54e/f4bda473 dependencies and the existing full local test suite pass. Earlier source/saved C/ZX, external-consumer and actual chooser/pad interaction evidence stands. Game consumer pins and final platform profiles remain pending. |
| `kryonlabs/packages/gamekit` | `—` | kryon | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/packages/gamekit/tests/fixture-game` | `src/fixture.zi` | gamekit, kryon | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/packages/plot` | `src/app.tsx` | kryon | cairo (desktop), portable (desktop/ZX) | Typed chart components, six shared scenes and dashboard cookbook. Published [eaa818b7](https://git.waozi.xyz/kryonlabs/plot/commit/eaa818b7a122957c61fddc8b86911a03d7a938fa), pushed, CI not checked yet. Normal published e265c54e/f4bda473 graph passes the existing full local package and dashboard UI gates, including Cairo/ZX 550/1200 pixel comparisons. Final declared profile runs and consumer pins remain pending. |
| `kryonlabs/rill` | `—` | kryon, terminal, explorer | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/taskmgr` | Native models/host; TSX application screens | kryon, data_views | Desktop, portable ZX, Canvas and terminal | Native, Canvas, terminal and explicit ZX source checks pass. Portable sampling now reads real proc/DRM data through standard guest files and typed process services; fabricated empty snapshots and hard-coded accounting units are removed. Standard native-host implementation is being shared upstream across the runner and Kryon players. Final actual builds, input/runtime checks, published pins, Taiji guest GUI and publication remain pending. |
| `kryonlabs/telegram` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `kryonlabs/terminal` | TSX application entry and screens | kryon | desktop, portable ZX | Active maintained consumer. Native pane bindings pass source/saved native behavior and selective imports. Actual tabs/chrome/menu/prompt TSX composition passes desktop C source checking, source/saved ZX bundles, saved C IR and existing test-module checking. Native/ZX player links, private interaction, final pins and publication remain pending. |
| `kryonlabs/workbook` | `src/desktop.tsx`; native models and policies | kryon, mingcute | desktop; hosted bundle | Public TSX desktop, editor, grid and menus. Published [1870b590](https://git.waozi.xyz/kryonlabs/workbook/commit/1870b5904c836e837186f31c9779b4b6c95d0a1f), pushed, CI not checked yet. Normal e265c54e/f4bda473 graph and the existing full local test pass: desktop/private UI, services, structure/grid, Gnumeric round trips and function catalogs, parity fixtures, autosave/formulas/undo, headers and menus. Native theme/reply views have explicit valid scopes. Consumer pin updates remain pending; application installation is not claimed. |
| `taijiosnet/taijios/appl/wm` | `—` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/apps/breeze` | `src/main.zi` native host; authored TSX screens | kryon, css, game2d, plot, mingcute | desktop, Web, Android | Active maintained consumer. Root UI locally committed `522892ad`; earlier desktop/Web/Android source checks, asset-bearing bundles and hosted first frames pass. The current desktop graph advances through the repaired native service/view lifetime violations to dependency alignment; the current complete graph is not green. Current native build, updated published pins, complete editor declarations, real standalone app-host launch, Alpha interaction and final push remain pending. |
| `waozixyz/apps/breeze/design/figure-preview` | `figure_preview.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/apps/photos` | `src/app.zi` | kryon, mingcute | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/apps/photos/portable` | `—` | kryon, mingcute | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/apps/relay` | `src/desktop.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/apps/werk` | `src/desktop.zi` | kryon, mingcute, workbook, taskmgr | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/apps/werk/apps/gallery` | `gallery.zi` | kryon, mingcute | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/autotrade/native/vnpy_research_ui` | `src/app.tsx` | kryon, plot, data_views, css, workbook | desktop (Linux X11/Cairo) | All nine maintained routes and details compose in TSX; native models/actions/math remain. The normal production package build with the real image decoder and private embedded-window execution pass, including resize. Loaded native callbacks preserve all 24 training trials and held-out results. Existing route, card open/reorder/close, navigation and resize checks pass. The chart comparison covers 23 cases and 132 matching native drawing commands; full raster equivalence is not established. The coherent published compiler, Kryon, Plot, DataViews, CSS and Workbook pins are locked in [a8846ced](https://git.waozi.xyz/harmony/autotrade/commit/a8846ced0d87cf20033205c6d5e03273712068be), pushed, CI not checked yet. A fresh root build and install succeeds for all ten native components; the installed desktop passes private-display open, decode, resize and clean-exit acceptance. Native embedding ownership/restart/cleanup also passes. The owner’s running application was not restarted. Only the declared desktop profile is claimed. |
| `waozixyz/games/apple` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/arcade` | `src/desktop.zi` | kryon, mingcute, game2d, css | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/choano` | TSX entry, screens and controls; native game drawing and models | kryon, game2d, mingcute, css, entity | desktop, portable, Web and Android build routes | The current native build links successfully against fac8a396/f4bda473 with the required Opus link and runs on a private display. Entity still uses a development override. The existing private-Xvfb interaction test passes profile creation, start, HUD at three sizes, swim and restart in both Amoeba and Fish. Remaining profiles, final published dependency pins and commit/push remain pending. |
| `waozixyz/games/garden` | `garden.zi` | Game2D | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/kasaival` | `kasaival.zi` | Game2D, kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/katalis` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/neon` | `neon.zi` | Game2D | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/shark` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/shovey` | `—` | kryon, mingcute | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/voyki` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/wurm` | `src/app.zi` | kryon, mingcute, game2d | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/games/zoombinis` | `src/app.zi` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/learning/jungle` | 41 maintained TSX modules; native domain, drawing and platform logic retained | kryon, mingcute | Native desktop and GPU web; final declared matrix pending | Fresh native application build and source-receipt validation pass against canonical source overrides. All 32 scene routes list successfully, and the real account window opens and remains alive on private Xvfb. The generic startup repair removes the former model/view initialization abort. All 14 activity/root source checks and a private GPU-texture orientation/alpha/frame probe pass; the latter uses SwiftShader and is not hardware-performance evidence. Final published pins, locked native/web rebuilds, full activity walks, loading/error/back/retry and narrow/localized layout checks remain pending. |
| `waozixyz/modules/apps` | Native host/lifecycle with TSX launcher, connection, HUD, catalogue and host strip | kryon | Native and supported hosted consumers, including Breeze Android | Published fac8a396/f4bda473/Mesh 05eed82/Crypto cd4a75b pins resolve without local overrides. Existing native fixture, external consumer, HUD native/ZX/hosted (320 geometry cases), tree/deep routes, document, add-on, artwork, audio and locale checks pass. The existing four notification guests now pass exact source/saved-IR ZX byte equality with the verified e265c54e compiler; the final published-pin rerun remains pending. Final publication and downstream pins remain pending. |
| `waozixyz/modules/auth` | `src/auth.zi` | kryon, mingcute | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/modules/dew` | `—` | kryon | manifest default | Pending classification, ordinary package build and run |
| `waozixyz/modules/entity` | Native models/actions with actual TSX views | kryon | Native and hosted consumer profiles | Active maintained library. Auth, alias, referral, friends, achievements, Telegram, toast, Dew balance and leaderboard composition are TSX. Shared presentation controls replace repeated screen helpers; Friends reads native owned row snapshots. Account/profile, Dew shop, picture/appearance and remaining modals also use TSX. The actual TSX entry passes hosted source checking; the existing toast and activity-view runtime tests pass. Complete native/portable builds and interaction remain pending. The existing native model test passes with duplicate C compilation removed; complete profile checks and final pins remain pending. |
| `waozixyz/modules/practices` | `—` | kryon | manifest default | Pending classification, ordinary package build and run |

## Source/submodule evidence requiring classification

Source imports, build files and submodules were scanned independently of manifests. These rows preserve repository ownership and representative evidence; active/archive/custom-drawing/logic-only status still requires scoped review.

| Repository | Matching files | Representative source/submodule | Status |
| --- | --- | --- | --- |
| `Unresolved ownership` | 3 | `taijiosnet/taiji/.gitmodules` | Pending classification and build/run selection |
| `harness/benchmarks` | 6 | `harness/benchmarks/challenges/scene3d/v1/reference_scene.zi` | Pending classification and build/run selection |
| `harness/harmony` | 97 | `harness/harmony/.gitmodules` | Pending classification and build/run selection |
| `harness/music` | 2 | `harness/music/src/app.zi` | Pending classification and build/run selection |
| `harness/waozi` | 1 | `harness/waozi/tests/kryon/src/app.zi` | Pending classification and build/run selection |
| `kryonlabs/browser` | 5 | `kryonlabs/browser/src/app.zi` | Pending classification and build/run selection |
| `kryonlabs/explorer` | 11 | `kryonlabs/explorer/src/app.zi` | Pending classification and build/run selection |
| `kryonlabs/github` | 2 | `kryonlabs/github/src/app.zi` | Pending classification and build/run selection |
| `kryonlabs/gitnex` | 4 | `kryonlabs/gitnex/src/app.zi` | Pending classification and build/run selection |
| `kryonlabs/packages/gamekit` | 2 | `kryonlabs/packages/gamekit/src/gamekit_host.zi` | Pending classification and build/run selection |
| `kryonlabs/rill` | 44 | `kryonlabs/rill/app/about_main.zi` | Pending classification and build/run selection |
| `kryonlabs/taskmgr` | 10 | `kryonlabs/taskmgr/src/app.zi` | Pending classification and build/run selection |
| `kryonlabs/telegram` | 13 | `kryonlabs/telegram/src/app.zi` | Pending classification and build/run selection |
| `kryonlabs/terminal` | 35 | `kryonlabs/terminal/src/app/app_chrome.zi` | Pending classification and build/run selection |
| `taijiosnet/taijios` | 34 | `taijiosnet/taijios/appl/wm/avi.zi` | Pending classification and build/run selection |
| `waozixyz/apps/breeze` | 162 | `waozixyz/apps/breeze/apps/diary/diary.zi` | Pending classification and build/run selection |
| `waozixyz/apps/photos` | 21 | `waozixyz/apps/photos/portable/photos_view.zi` | Pending classification and build/run selection |
| `waozixyz/apps/relay` | 3 | `waozixyz/apps/relay/src/app.zi` | Pending classification and build/run selection |
| `waozixyz/apps/werk` | 33 | `waozixyz/apps/werk/apps/calculator/calculator.zi` | Pending classification and build/run selection |
| `waozixyz/autotrade` | 16 | `waozixyz/autotrade/native/vnpy_research_ui/src/analysis_page.zi` | Pending classification and build/run selection |
| `waozixyz/games/apple` | 6 | `waozixyz/games/apple/src/app.zi` | Pending classification and build/run selection |
| `waozixyz/games/arcade` | 16 | `waozixyz/games/arcade/src/app.zi` | Pending classification and build/run selection |
| `waozixyz/games/choano` | 32 | `waozixyz/games/choano/src/accessories.zi` | Pending classification and build/run selection |
| `waozixyz/games/kasaival` | 1 | `waozixyz/games/kasaival/kasaival.zi` | Pending classification and build/run selection |
| `waozixyz/games/katalis` | 9 | `waozixyz/games/katalis/src/app.zi` | Pending classification and build/run selection |
| `waozixyz/games/shovey` | 43 | `waozixyz/games/shovey/src/android_glue.zi` | Pending classification and build/run selection |
| `waozixyz/games/voyki` | 1 | `waozixyz/games/voyki/src/app.zi` | Pending classification and build/run selection |
| `waozixyz/games/wurm` | 42 | `waozixyz/games/wurm/src/account.zi` | Pending classification and build/run selection |
| `waozixyz/games/zoombinis` | 2 | `waozixyz/games/zoombinis/src/app.zi` | Pending classification and build/run selection |
| `waozixyz/learning/jungle` | 95 | `waozixyz/learning/jungle/games/animal/animal_count.zi` | Pending classification and build/run selection |
| `waozixyz/modules/apps` | 34 | `waozixyz/modules/apps/bench/arcade_bench.zi` | Pending classification and build/run selection |
| `waozixyz/modules/auth` | 3 | `waozixyz/modules/auth/src/auth.zi` | Pending classification and build/run selection |
| `waozixyz/modules/dew` | 2 | `waozixyz/modules/dew/src/topup.zi` | Pending classification and build/run selection |
| `waozixyz/modules/entity` | 47 | `waozixyz/modules/entity/src/account_handoff.zi` | Pending classification and build/run selection |
| `waozixyz/modules/practices` | 11 | `waozixyz/modules/practices/figure/figure_cloth.zi` | Pending classification and build/run selection |

## Native UI export surface

This is an inventory of native exports, not a list of obsolete APIs to delete. Widget bindings, implementation visibility and optional-package ownership must be established before removing application entry points.

| Export | Native source | TSX surface + verification |
| --- | --- | --- |
| `Clock` | `src/ui/clock.zi` | Pending |
| `DPI` | `src/ui/dpi.zi` | Pending |
| `Dialog` | `src/ui/dialog.zi` | Pending |
| `Frame` | `src/ui/tree_draw.zi` | Pending |
| `Media` | `src/ui/media.zi` | Pending |
| `PageLayout` | `src/ui/page/page_layout.zi` | Pending |
| `ScrollDevice` | `src/ui/scroll_device.zi` | Pending |
| `Semantic` | `src/ui/semantic.zi` | Pending |
| `Service` | `src/ui/service.zi` | Pending |
| `SystemTheme` | `src/ui/system_theme.zi` | Pending |
| `SystemThemeHost` | `src/ui/system_theme_host.zi` | Pending |
| `Theme` | `src/ui/theme.zi` | Pending |
| `Video` | `src/ui/video.zi` | Pending |
| `WebView` | `src/ui/webview.zi` | Pending |
| `Window` | `src/ui/window.zi` | Pending |
| `Zoom` | `src/ui/zoom.zi` | Pending |
| `alert` | `src/ui/alert/alert.zi` | Pending |
| `avatar` | `src/ui/avatar/avatar.zi` | Pending |
| `badge` | `src/ui/badge/badge.zi` | Pending |
| `banner` | `src/ui/banner/banner.zi` | Pending |
| `bitmap_image` | `src/ui/bitmap_image.zi` | Pending |
| `box` | `src/ui/box/box.zi` | Pending |
| `breadcrumb` | `src/ui/breadcrumb/breadcrumb.zi` | Pending |
| `button` | `src/ui/button/button.zi` | Pending |
| `button_group` | `src/ui/buttongroup/buttongroup.zi` | Pending |
| `card` | `src/ui/card/card.zi` | Pending |
| `carousel` | `src/ui/carousel/carousel.zi` | Pending |
| `checkbox` | `src/ui/checkbox/checkbox.zi` | Pending |
| `chip` | `src/ui/chip/chip.zi` | Pending |
| `clipboard` | `src/ui/clipboard.zi` | Pending |
| `collapsible` | `src/ui/collapsible/collapsible.zi` | Pending |
| `color_picker` | `src/ui/colorpicker/colorpicker.zi` | Pending |
| `command_palette` | `src/ui/commandpalette/commandpalette.zi` | Pending |
| `confirm_button` | `src/ui/confirmbutton/confirmbutton.zi` | Pending |
| `confirm_popover` | `src/ui/confirmpopover/confirmpopover.zi` | Pending |
| `context_menu` | `src/ui/contextmenu/contextmenu.zi` | Pending |
| `control_props` | `src/ui/control_props.zi` | Pending |
| `copy_button` | `src/ui/copybutton/copybutton.zi` | Pending |
| `countdown` | `src/ui/countdown/countdown.zi` | Pending |
| `cursor` | `src/ui/cursor_api.zi` | Pending |
| `daylight` | `src/ui/daylight.zi` | Pending |
| `direction_pad` | `src/ui/dpad/dpad.zi` | Pending |
| `dpi` | `src/ui/dpi.zi` | Pending |
| `drawing_props` | `src/ui/drawing_props.zi` | Pending |
| `dropdown` | `src/ui/dropdown/dropdown.zi` | Pending |
| `elevation` | `src/ui/elevation.zi` | Pending |
| `empty_state` | `src/ui/emptystate/emptystate.zi` | Pending |
| `fieldset` | `src/ui/fieldset/fieldset.zi` | Pending |
| `flow` | `src/ui/flow/flow.zi` | Pending |
| `font_metrics` | `src/ui/font_metrics.zi` | Pending |
| `frame_capture` | `src/ui/frame_capture.zi` | Pending |
| `frame_input` | `src/ui/frame_input.zi` | Pending |
| `frame_motion` | `src/ui/frame_motion.zi` | Pending |
| `frame_pacing` | `src/ui/frame_pacing.zi` | Pending |
| `frame_snapshot` | `src/ui/frame_snapshot.zi` | Pending |
| `frame_transition` | `src/ui/frame_transition.zi` | Pending |
| `geometry` | `src/ui/geometry.zi` | Pending |
| `grid` | `src/ui/grid.zi` | Pending |
| `grid_multi` | `src/ui/gridmulti/gridmulti.zi` | Pending |
| `grid_props` | `src/ui/grid_props.zi` | Pending |
| `guided_tour` | `src/ui/guidedtour/guidedtour.zi` | Pending |
| `hold_button` | `src/ui/holdbutton/holdbutton.zi` | Pending |
| `hover_card` | `src/ui/hovercard/hovercard.zi` | Pending |
| `icon` | `src/ui/icon.zi` | Pending |
| `image` | `src/ui/image/image_files.zi` | Pending |
| `image_behavior` | `src/ui/image/image_behavior.zi` | Pending |
| `image_mesh` | `src/ui/image/image_mesh.zi` | Pending |
| `image_props` | `src/ui/image/image_props.zi` | Pending |
| `image_raster` | `src/ui/image/image_raster.zi` | Pending |
| `image_showing` | `src/ui/image/image_showing.zi` | Pending |
| `inline_edit` | `src/ui/inlineedit/inlineedit.zi` | Pending |
| `input_props` | `src/ui/input_props.zi` | Pending |
| `input_protocol` | `src/ui/input_protocol.zi` | Pending |
| `joystick` | `src/ui/joystick/joystick.zi` | Pending |
| `key_input` | `src/ui/key_input.zi` | Pending |
| `keycap` | `src/ui/keycap/keycap.zi` | Pending |
| `layout` | `src/ui/layout/layout.zi` | Pending |
| `layout_motion` | `src/ui/layout_motion.zi` | Pending |
| `link` | `src/ui/link/link.zi` | Pending |
| `list_box` | `src/ui/listbox/listbox.zi` | Pending |
| `list_item` | `src/ui/listitem/listitem.zi` | Pending |
| `loading_indicator` | `src/ui/loading/loading.zi` | Pending |
| `loading_screen` | `src/ui/loadingscreen/loadingscreen.zi` | Pending |
| `markdown_editor` | `src/ui/markdown/markdown_editor.zi` | Pending |
| `markdown_images` | `src/ui/markdown/markdown_images.zi` | Pending |
| `markdown_preview` | `src/ui/markdown/markdown_preview.zi` | Pending |
| `media_audio` | `src/ui/media_audio.zi` | Pending |
| `menu` | `src/ui/menu/menu.zi` | Pending |
| `modal` | `src/ui/modal/modal_frame.zi` | Pending |
| `money_field` | `src/ui/moneyfield/moneyfield.zi` | Pending |
| `motion` | `src/ui/motion.zi` | Pending |
| `navigation` | `src/ui/navigation/navigation.zi` | Pending |
| `navigation_bar_config` | `src/ui/navigation/navigation_config.zi` | Pending |
| `notification` | `src/ui/notification.zi` | Pending |
| `onboarding_checklist` | `src/ui/checklist/checklist.zi` | Pending |
| `onboarding_checklist_task` | `src/ui/checklist/onboarding_checklist_task.zi` | Pending |
| `one_time_code` | `src/ui/onetimecode/onetimecode.zi` | Pending |
| `overlay` | `src/ui/overlay/overlay.zi` | Pending |
| `page` | `src/ui/page/page.zi` | Pending |
| `paint_matrix` | `src/ui/paint_matrix.zi` | Pending |
| `paint_queue` | `src/ui/paint_queue.zi` | Pending |
| `paint_transform` | `src/ui/paint_transform.zi` | Pending |
| `paned_view` | `src/ui/panedview/panedview.zi` | Pending |
| `pointer_input` | `src/ui/pointer_input.zi` | Pending |
| `press` | `src/ui/press/press.zi` | Pending |
| `progress` | `src/ui/progress/progress.zi` | Pending |
| `progress_ring` | `src/ui/progressring/progressring.zi` | Pending |
| `radio` | `src/ui/radio/radio.zi` | Pending |
| `reorder` | `src/ui/reorder/reorder.zi` | Pending |
| `router` | `src/ui/router.zi` | Pending |
| `router_props` | `src/ui/router_props.zi` | Pending |
| `scene` | `src/ui/scene.zi` | Pending |
| `scene_gltf` | `src/ui/scene_gltf.zi` | Pending |
| `scene_render` | `src/ui/scene_render.zi` | Pending |
| `scene_skin` | `src/ui/scene_skin.zi` | Pending |
| `scene_vrm` | `src/ui/scene_vrm.zi` | Pending |
| `scroll` | `src/ui/scroll/scroll.zi` | Pending |
| `scroll_device` | `src/ui/scroll_device.zi` | Pending |
| `scrub_field` | `src/ui/scrubfield/scrubfield.zi` | Pending |
| `segmented_control` | `src/ui/segmented/segmented.zi` | Pending |
| `selectable` | `src/ui/selectable/selectable.zi` | Pending |
| `semantic` | `src/ui/semantic.zi` | Pending |
| `separator` | `src/ui/separator/separator.zi` | Pending |
| `session` | `src/ui/session.zi` | Pending |
| `shader` | `src/ui/shader.zi` | Pending |
| `shader_glsl` | `src/ui/shader_glsl.zi` | Pending |
| `shape` | `src/ui/shape/shape.zi` | Pending |
| `sheet` | `src/ui/sheet/sheet.zi` | Pending |
| `shortcut_recorder` | `src/ui/shortcutrecorder/shortcutrecorder.zi` | Pending |
| `skeleton` | `src/ui/skeleton/skeleton.zi` | Pending |
| `slider` | `src/ui/slider/slider.zi` | Pending |
| `sortable` | `src/ui/sortable/sortable.zi` | Pending |
| `spinbox` | `src/ui/spinbox/spinbox.zi` | Pending |
| `style` | `src/ui/style.zi` | Pending |
| `style_motion` | `src/ui/style_motion.zi` | Pending |
| `style_sheet` | `src/ui/style_sheet.zi` | Pending |
| `surface` | `src/ui/surface.zi` | Pending |
| `svg_icon` | `src/ui/svgicon/svgicon.zi` | Pending |
| `svg_shape` | `src/ui/svg_shape.zi` | Pending |
| `swipe` | `src/ui/swipe.zi` | Pending |
| `swipe_props` | `src/ui/swipe_props.zi` | Pending |
| `swipe_row` | `src/ui/swiperow/swiperow.zi` | Pending |
| `system_clipboard` | `src/ui/system_clipboard.zi` | Pending |
| `system_theme` | `src/ui/system_theme.zi` | Pending |
| `tab_bar` | `src/ui/tabs/tabs.zi` | Pending |
| `tag_input` | `src/ui/taginput/taginput.zi` | Pending |
| `text` | `src/ui/text/text.zi` | Pending |
| `text_align` | `src/ui/text_align.zi` | Pending |
| `text_area` | `src/ui/textarea/textarea.zi` | Pending |
| `text_context_menu` | `src/ui/text_context_menu.zi` | Pending |
| `text_field` | `src/ui/textfield/textfield.zi` | Pending |
| `text_host` | `src/ui/text_host.zi` | Pending |
| `text_input` | `src/ui/text_input.zi` | Pending |
| `text_input_platform` | `src/ui/text_input_platform.zi` | Pending |
| `text_layout` | `src/ui/text_layout.zi` | Pending |
| `text_measure` | `src/ui/text/text_measure.zi` | Pending |
| `text_props` | `src/ui/text/text_props.zi` | Pending |
| `text_request` | `src/ui/text_request.zi` | Pending |
| `text_selection` | `src/ui/text_selection_widget.zi` | Pending |
| `text_style` | `src/ui/text/text_style.zi` | Pending |
| `text_typeface` | `src/ui/text_typeface.zi` | Pending |
| `theme` | `src/ui/theme.zi` | Pending |
| `tile_map` | `src/ui/tilemap/tilemap.zi` | Pending |
| `time_picker` | `src/ui/timepicker/timepicker.zi` | Pending |
| `timeline` | `src/ui/timeline/timeline.zi` | Pending |
| `title_bar` | `src/ui/titlebar/titlebar.zi` | Pending |
| `toast` | `src/ui/toast/toast.zi` | Pending |
| `toggle` | `src/ui/toggle/toggle.zi` | Pending |
| `toggle_measure` | `src/ui/toggle/toggle_measure.zi` | Pending |
| `toolbar` | `src/ui/toolbar/toolbar.zi` | Pending |
| `tooltip` | `src/ui/tooltip/tooltip.zi` | Pending |
| `tooltip_common` | `src/ui/tooltip_common.zi` | Pending |
| `touch_replay` | `src/ui/touch_replay.zi` | Pending |
| `transition` | `src/ui/transition.zi` | Pending |
| `tree` | `src/ui/tree.zi` | Pending |
| `tree_draw` | `src/ui/tree_draw.zi` | Pending |
| `tree_frame` | `src/ui/tree_frame.zi` | Pending |
| `tree_input` | `src/ui/tree_input.zi` | Pending |
| `typeface_source` | `src/ui/typeface_source.zi` | Pending |
| `vector_path` | `src/ui/vector_path.zi` | Pending |
| `view_transform` | `src/ui/view_transform.zi` | Pending |
| `watercolor` | `src/ui/watercolor.zi` | Pending |
| `widget_kind` | `src/ui/widget_kind.zi` | Pending |
| `widget_palette` | `src/ui/widget_palette.zi` | Pending |
| `window` | `src/ui/window.zi` | Pending |
| `window_chrome` | `src/ui/window_chrome/window_chrome.zi` | Pending |
| `window_flags` | `src/ui/window_flags.zi` | Pending |
| `workspace_pager` | `src/ui/workspace_pager/workspace_pager.zi` | Pending |
| `zoom` | `src/ui/zoom.zi` | Pending |

## Preserved benchmark evidence

Read `/home/wao/Documents/tsx-native-benchmarks-2026-11-26/REPORT.txt`, `summary.csv`, and the structure of `metadata.json` and `results.json`; archived sources and 120 raw files are present. Physical Alpha, real pixmap renderer, nine pairs/case. Prototype numeric labels doubled allocations; callbacks were no-ops and repeated/keyed component state was not measured. These results do not verify the production migration.
