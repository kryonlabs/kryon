# Changelog

## Moving an app to this Kryon

Bump the Ziran toolchain, Kryon and CSS pins together: `kryon upgrade` in the
app's directory pins the set this kryon knows (the starter's: Ziran e225a4fe,
Kryon 2f9b5db2, CSS d55590a6) and locks afresh. A CSS package must pin the
same Kryon as the app, or the build names the export both define. Then:

- **Card is a container.** Before: `c := Card.Draw(session, .{.bounds = slot.bounds}).content`
  and children drawn at `c`. After: `card := Card.Draw(session, Card.Props.{.variant = .CardOutlined})`,
  then `if card.opened { children; unused End(session) }`; without a height it grows to its
  children. (atr src/ui/chrome.zi:30 uses the old form.)
- **Child-rect helpers are gone:** `ColumnChildBounds`, `RowChildBounds`, `StackChildBoundsFor`,
  `FlowChildBounds`, `CardContentBounds`. Draw children inside `Layout.Draw`, `Row`, `Stack`,
  `Grid` or `Flow` and let it place them; a widget leaves its width (column) or height (row)
  unset to fill. `FocusDefaultOutlineBounds` is gone: `FocusRingFor` sizes the focus ring
  from the style's `:focus-visible` outline.
- **No Stack slots for composites.** TitleBar sizes itself in a column; draw trailing widgets
  at `bar.trailing_bounds`. Containers without a height size to their children at `End`, and
  Row, Column and Grid paint their CSS background, border, radius and box-shadow.
- **Widget keys are optional.** Before: `.key = cast(u64)(100 + index)`. After: no key; a
  widget is known by its calling line and parent, and a loop's repeats count in order. Keep
  `.key = item.id` only where rows reorder.
- **Stricter CSS.** A plain `CssSheetParse` refuses browser-only declarations with the
  reason; `CssSheetParseWithDiagnostics` reports them. A style pack in an older layout is
  refused as stale: rebuild it with the pinned CSS package.
- **Transforms are native and opt-in.** `transform`, `translate`, `rotate`, `scale` and
  `transform-origin` paint on every host once the app calls `PaintTransformInstall()` from
  `kryon/paint_transform`; `CssSheetInstall` refuses a sheet that sets one until then.
- **`translate` moves the whole box.** Before: `NavigationBar[role=Panel] { translate: 340px 0; }`
  set a widget part's offset or metric. After: `NavigationBar[role=Panel] { -kryon-content-offset: 340px 0; }`
  keeps that metric, and `Button:hover { translate: 0 -2px; }` lifts the whole button at paint
  time, as on the web. The old meaning has no alias: rename every `translate:` that set a part
  metric (the CSS presets already use `-kryon-content-offset`). Apps' own sheets need the same
  rename: Breeze's `assets/styles/inbe.css:243` `translate: 0 0` reset a part offset and now asks
  for a transform, which `CssSheetInstall` refuses without `PaintTransformInstall()`; it should
  read `-kryon-content-offset: 0 0`.
- **Import `kryon/Frame`.** The old `kryon/tree_draw` name still works but is on its way out.
- `kryon/swipe_row` stays: Breeze Lists uses it.

## Unreleased

- Open native apps through `kryon.code-workspace` and use the current
  `js/ts.validate.enabled` setting, including multi-root workspace folders.

- Configure VS Code and TSX starters to use built-in TypeScript React editing
  with native Ziran diagnostics, rather than JavaScript/React assumptions.
  Document why native syntax and checked imports need the Ziran extension
  instead of duplicate ambient widget declarations (docs/VSCODE.md).

- Resolve Android's program-entry binding through the public package import,
  so compiler module namespaces cannot break native application builds.

- Fix one-pixel gaps in GPU-rendered pills and rounded rectangles at
  fractional positions.

- Complete native flex layout: `flex-direction` with reversal, `wrap` and
  `wrap-reverse` lines, `flex-grow`/`flex-shrink`/`flex-basis` (px, rem and
  percentage bases), `justify-content`, `align-items`, `align-content` and
  `align-self` including baseline, `order`, and min/max sizing constraints
  on flex items.

- Match structural sibling selectors (`:first-child`, `:last-child`,
  `:only-child`, `:nth-child()` and the of-type family) against the frame's
  tree: early positions match while the frame builds, late positions read
  the committed tree and converge on the next frame after a structure
  change.

- Parse and paint `radial-gradient()` backgrounds on layout containers:
  circle and ellipse shapes, extent keywords, explicit radii, `at`
  positions, and two to six percentage stops.

- Place the gallery demo scenes in measured Layout containers instead of
  manual `GalleryBounds` rectangles, keeping explicit rectangles for
  overlays and control canvases.

- Let Android hosts load embedded bundle pictures through the lazy, size-aware picture cache.

- Decode Android thumbnails with bounded bitmap sampling before final reduction,
  retain intrinsic dimensions, and reduce ARM indexed-animation upload overhead.

- Honor `App.Window` title and dimensions in portable desktop frame programs
  and direct native executables, rather than silently using player defaults.

- Pin the compiler with readable direct C foreign bindings and closed-program
  pruning of unused optional hooks; keep generated modules in separate files.

- Keep widget key derivation independent of sessions and bitmap ownership.
  Duplicate-key recovery uses the stateless `widget_key` module.

- Build and register the Windows native host with `make install-windows-host`,
  alongside its runtime DLLs, independently of the Linux host library.

- Update only changed indexed mesh ranges during mixed CPU/GPU animation, and skip transparent pixel premultiplication.

- Select `App.Run` hosts by code generator, then platform or explicit profile.
  Terminal and headless pixmap profiles take priority over the desktop default
  on both Linux and Windows; desktop suppression now applies to Windows too.

- Move font-independent UTF-8 tokenization, word/grapheme wrapping and line-height arithmetic into Ziran `std/text_layout`; Kryon supplies measurement and font-revision caches.
- Load portable host time only through optional motion modules. Static Hello World no longer imports the clock service.

- Allow glTF updates to skip hidden non-shadow-casting primitives while preserving transform updates and posing newly shown meshes.

- Filter managed runtime bitmaps on the GPU rather than continually rebuilding CPU Lanczos copies for animated frames.
- Retain Android GPU composition across swapped window buffers, preventing stale pixels after partial redraws and rotation without CPU readback.

- Decode hosted records and slices imported through a dependency's module alias, including Plot's band charts.

- Keep full styles, selector matching, shadow policy and theme tables out of plain portable labels. Text carries typography through optional stylesheet and motion adapters; installed themes supply live colors. Hello's dependency modules fall from 57 to 53 and its portable bundle from 1,555,437 to 1,470,285 bytes, with the same `Text.Draw` call.

- Keep native GPU timestamp queries out of WebGL builds so applications using the scene renderer compile for the web.
- Select the scene's WebGL context before probing or drawing, including when input prepares the scene before its first rendered frame.

- Recognize readable package identities in hosted widget and record module names, so existing bundles start alongside hosts built with newer package naming.

- Reduce simple text programs' dependency graph by separating container measurement, paint operations, style facts, color values, bitmap metadata and portable editor policy. Keep `Text.Draw` and container APIs unchanged; layout-only programs still require no renderer, and editing buffers load with editing widgets.
- Pin starter projects to the current Ziran toolchain and recognize its dependency-alias module names in portable hosts.

- Avoid a duplicate C entry for plain `src/main.zi` programs on desktop and web. Android supplies its native host bridge in generated sources, so application entries need no `#program_export` annotation.

- Keep CPU-only web scene builds compiling: the shared skinning capability queries already return unavailable through the web probe and need no duplicate fallback declarations.

- The HTML DOM host is removed: no owner project, template or the editor preview selected it (the `web` template and previews use Canvas). Deleted `frames_dom.zi`, `dom.zi`, `dom_tree.zi`, `dom_metrics.zi`, `dom_raster.zi`, the `dom` profile and `KRYON_DOM` define in `kryon/app` and `mk/ziran-project.mk`, `mk/dom-shell.html`, `dom-profile-test` and its fixtures, and the DOM profile documentation. The browser story is the Canvas2D host (`frames_web.zi`); a `backend = "dom"` profile now fails with the ordinary unavailable-backend error.

## [0.7.0] - 2026-11-25

- Desktop windows, the native ZX runner (`zx-desktop`), `zx2native` executables (`libkryon-desktop.so`), and SDL pixel hosts now render on the GPU by default instead of CPU software rendering: `pixmap_sdl.zi` (`CreatePixmapRenderer`) requests `SDL_RENDERER_ACCELERATED` first on real GPU desktops (honoring `SDL_RENDER_DRIVER`, `KRYON_PIXMAP_GPU=0` opt-out, and avoiding `llvmpipe`/`softpipe` on headless Xvfb), and `desktop_gpu.zi` draws rectangles, rounded rectangles, outlines, lines, triangles, cached Pango/Cairo text runs, cached images and warped image meshes directly on the GPU via `SDL_RenderGeometry` into a retained `SDL_TEXTUREACCESS_TARGET` texture with zero full-frame CPU framebuffer uploads (`SDL_RenderReadPixels` is used only on demand for PNG captures and frame snapshots).

- `zx2native` desktop UI executables link the real SDL2/GPU/Cairo desktop host library (`libkryon-desktop.so`, registered as `kryon-desktop.library`) instead of the headless CPU-only `libkryon-pixmap.so` (`kryon-pixmap.library`), which is removed: `zx_window_host.zi` now exports both `App.Show` (`ZxWindow*Host`) and full `App.Run` (`frames_zx` loop, Cairo/Pango/FreeType text measurement & raster, SDL2 window/input/clipboard/clock/theme/service) capabilities, `CairoTextOpen` lazily creates a 1×1 measurement surface so headless `MeasureGlyphWidth` calls work without opening a window, and `desktop_runtime.zi` prefers hardware-accelerated SDL2 renderers on real GPU desktops (falling back cleanly to software on `llvmpipe`/`softpipe` or `LIBGL_ALWAYS_SOFTWARE=1`). Linking tradeoff: pure/console/file/clock `zx2native` ELFs remain static, whereas graphical desktop `zx2native` executables dynamically link `libkryon-desktop.so` (SDL2/Cairo/X11) so portable `.zx` bundles run at native speed on the platform's real SDL2/GPU stack.

- `main` is the only Kryon UI program entry: the program declares `main` and runs its window through `kryon/app` (`App.Run(window, Frame)`), and an entry without `main :: () -> s32` fails the build with a migration error. The Frame program export is removed with no compat path: the `frame_entry` binding, the old `desktop_run`/`canvas_run`/`dom_run`/`terminal_run`/`pixmap_run`/`zx_run` host programs and the `KRYON_FRAME_ENTRY` escape are gone, every fixture and example runs as a program, `App.Frame` carries the `viewport` its draw runs in, and quitting is `WindowQuit()` from `Window` instead of a Frame return value.

- A program entry builds on non-desktop C backends without carrying desktop modules: terminal, canvas, dom and pixmap builds define `KRYON_NO_DESKTOP_HOST`, and `kryon/app` then maps `App.Show`/`App.Run` to `app_unsupported` on Linux C targets, so `kryon run shot` links a program entry (which imports `kryon/app`) without the desktop window and accessibility libraries.

- Android system text draws from the pixel backend's GPU atlas as textured quads, like bundled glyphs, instead of one quad per pixel: the Android typeface raster keeps the last 64 runs (4 MB together) instead of one, names each raster by its text, font, face, style and color, and a run wider than an atlas entry draws in column chunks (`PixmapGpuAddMaskRows`). The GPU recording test draws a 600-pixel named run on both paths.

- Android picture caches decode PNG through the activity's BitmapFactory bridge first, and keep the in-process decoder for hosts without a bridge; a 960x1440 title picture decodes three times faster on the USB Moto.

- CSS transforms paint natively on every host: `kryon/paint_transform` gives Row, Column, Grid, Button, Card, Text, Link and Image nodes the `transform`, `translate`, `rotate` and `scale` of their style rules (with `:hover` and `:active` from the pointer) about their `transform-origin`, to the node and everything in it, and eases changes over a transform transition. Whole-pixel moves, text and images are rewritten in place and keep the host's rasterizer; other shapes draw as triangles through a new `PaintKindTransformed`. It is opt-in: an app calls `PaintTransformInstall()` once, and apps that do not link none of it. `StyleBoxData` carries the transform fields and `PaintCommand` an owner node and a matrix.

- `TabBar.Props.fit` keeps a horizontal tab row on one line: tabs that would scroll first lose padding, down to half the style's, then label size, down to four fifths of it (`TabBarFitFor`); only a row still too wide at that floor scrolls. The bar's height and tap area stay. The widget test checks a four-tab row at 320, 360 and 412 dp widths and two scales.

- `kryon/sortable` reorders rows by dragging, also into, out of and between nested groups: `Sortable.Item` lifts a row on a mouse drag or a touch hold (a swipe still scrolls), `Sortable.Target` says where it lands (between rows, at a group's end, or into a group dropped on its start row) and `Sortable.Indicator` shows it. Built from Press and Box, so hosted packages use it too. `ziran_sortable_test` covers drops, presses and the hosted build.

- Android GLES presentation stays on GLES when a smaller picture uploads after a larger one: the presenter's staging buffer keeps its capacity instead of refusing the smaller image and dropping the window to software Pixmap. A GPU image quad whose source passes the image edge by float rounding alone (within 1/256 texel) snaps to the edge instead of taking the per-pixel path. `android_egl_composition_test` covers both.

- glTF loading decodes only the images that material texture slots sample. Others, such as a VRM thumbnail or an extension's maps, stay encoded until `GltfDecodeImage(model, index, base_dir)` asks for them, and an unreadable one no longer fails the model. A practice VRM figure decodes about half as many pixels.

- A Button a placing parent lays out takes its slot before it paints (TreePlaceAhead), so a grid cell's button centers its label in the cell, and an explicit width stays at the start of a wider cell as in CSS. Tests cover buttons in a padded column's grid and a grid in a padded column.

- examples/dashboard builds on Kryon d0711324 and CSS 164d983; its KPI progress bars now paint their fill, and the README records the new light, dark and narrow digests.

- Row, Column and Grid paint `linear-gradient()` at any angle or toward a side or corner with two to six stops, from vertical gradient strips (one strip split at its stops for a vertical gradient, up to 64 otherwise). `StyleBoxData` carries the gradient direction, corner, stops and middle colors.

- `Layout.Box(session, props, face)` opens a padded column (or row) that takes a widget's own resolved face for its padding, gap and paint, so a composite widget such as Card can be a self-sizing container that paints at `End`; Row, Column and Grid also paint a CSS `box-shadow` (an elevation role) under their box.

- CSS selectors carry tree context natively: one ancestor matched by class or id (`.card Text`, `#sidebar .item`), the parent (`.toolbar > Button`), `:first-child`, and `:not(.class)` or `:not(:state)`. `StyleSelector` gains `context`, `ancestor_class`, `ancestor_id`, `not_class` and `not_state`; the style_tree module, installed by `BeginFrame`, matches the widget being styled as the next child of the open container, and rules without context skip it. The starter budget is raised to 3,950,000 bytes (3,885,985 measured).

- Row, Column and Grid paint CSS per-side borders (`border-top|right|bottom|left`, the logical block/inline sides, and their `-width`, `-color` and `-style`) as square bands over the uniform border, for dividers, accent bars and tab underlines; with a border radius a box keeps its uniform border. `StyleBoxData` carries the side widths and colors and the `outline` width and offset, and `FocusRingFor` sizes the keyboard focus ring Button, ConfirmButton, Selectable and Radio draw from their `:focus-visible` outline rules. `FocusDefaultOutlineBounds` is removed.

- CSS percentages size Row, Column and Grid natively: `width`, `height`, `min-height`, `min-width`, `max-width` and `max-height` in % of the parent's content box (a grid child's track), with a percent height in a content-sized parent computing to auto, and `flex-basis` % of a measured flex row's inner size. `StyleData`, `StyleBoxData` and `StyleFlexData` carry a `percent` mask that layout resolves when it opens the container; other widgets ignore percentages.

- examples/dashboard is rewritten on self-sizing containers and its stylesheet: no rectangle arithmetic or computed keys, 760 px media rules for the narrow layout, painted cards, and a passing driver test on the pinned Kryon and CSS.

- Containers lay out like CSS flex and block boxes without sizes from the app: in a placing Row an auto-width container takes the width its earlier siblings left and a container in a fixed-height Row stretches to it; CSS `flex: 1` on a container in a fixed-height Column takes the remaining height; `margin-left: auto` pushes a fixed-width container to the row's end. Text placed with no size fills a column or grid track (a row keeps its own width) and takes its wrapped line height. Badge draws its label where it was placed, and a full-width Button fills the column that places it.

- Row, Column and Grid paint their CSS background, border and border radius beneath their children, like a styled div; a content-sized one paints its box at `End` and moves it under the children in place. The frame fills the painting slot, so a program that never paints carries no paint queue.

- `make budget-test` fails when a 1,000-row ListItem frame grows past its recorded tree nodes or paint commands, or the app starter's portable bundle past its recorded bytes: counts that are the same on every machine.

- Row, Column, Grid and Flow without a height size to the children they place when `End` closes them, and move the next sibling down; Flow wraps; `Layout.GridSpan` spans the next grid child. The manual `ColumnChildBounds`, `RowChildBounds`, `StackChildBoundsFor` and `FlowChildBounds` helpers are gone.

- Removed legacy and duplicate code with no users left: the BSD and GNU make shims and go.mod beside the one Makefile, the SwipeRow example (the widget stays: Breeze Lists uses it), the images_pending stub, the host/android and host/terminal re-exports, the unused canvas and DOM HTML shells, the system_typeface_linux and typeface_collection modules, the tree_label_cost scripts, and the release-notes, update_version, subset-fonts and update-showcase scripts (Harmony keeps its own copies).

- Layout containers read CSS margins (with `auto` centering), min/max width, max height and overflow clipping; Text reads text-align.

- Place measured row and column children with wrapping, flex sizing and indexed layout rules.

- Test fixtures lock against the exact Gitea Ziran the starter templates pin and this checkout's Kryon revision through one helper in tests/toolchain.py, overriding only Kryon, so checked packages lock in every fixture.

- Measure native grid rows from each item’s available track width.

- The app starter opts into the Material look preset in dark mode through a pinned CSS package revision that shares its Kryon, so its lock holds one Kryon; light mode keeps Kryon's default theme.

- New widgets: `kryon/chip` (selectable and dismissible pills), `kryon/list_item` (leading icon or avatar, title, subtitle, trailing text, badge, chevron or switch) and `kryon/skeleton` (block, line and circle placeholders that pulse unless motion is reduced). Each takes its size from the theme, draws where its container places it, and has a gallery example and a ui-map entry.

- When no app answers, the test driver names the fix: start and watch the app from the test (`StartOnDisplay`, `DriverEnvironment`, `DriverWatch`) instead of a hand-started Xvfb or app, or build it with `KRYON_TEST_DRIVER=1`. docs/FRAME_REPLAY.md lists all five frame capture bindings a replay host needs.

- Project builds create stamp files by truncation instead of touch timestamps and set published page and executable modes through fchmod, so restricted builds that deny path chmod and timestamp syscalls still produce readable pages and runnable programs.

- `kryon run [PROFILE] --watch` builds a desktop or terminal app, runs it, and after every saved change stops it, rebuilds and runs it again; a failed build waits for the next change.

- Clip native line segments at scroll and viewport edges in either direction.

- docs/CHEATSHEET.md is one page for a model that has never seen Kryon: the project, an app shell to copy, one line per widget, the layout recipe, theme tokens, rules for state and events, the screenshot command and a driver test. `make cheatsheet-test` builds every block as a project, shoots it and runs its driver test. README.md and AGENTS.md link it.

- `kryon init` with a Kryon template pins a `[toolchain]` that floats on a branch, as `ziran new` writes it, to the Ziran Kryon's packages are checked with, keeps an exact pin the project chose, and names the fix when the lock still refuses.

- The app starter is a modern screen with no extra work: an app bar with a dark mode switch, a card list and a filled primary button, placed by Layout containers and sized by theme tokens, with its state in a module free of Kryon imports. Its first build needs no `ziran lock`, and the templates test checks its light and dark pixels.

- `kryon run [PROFILE] --png FILE.png [--width PX] [--height PX] [--theme light|dark]` renders any app entry offscreen at any size up to 4096 pixels, with its widget tree beside the PNG and no preview fixture. The dashboard example's stylesheet parses again and reports a rejected sheet once.

- `kryon new` and `kryon init` take templates from the Gitea origin, and every starter pins exact Gitea Ziran and Kryon revisions, so a new app locks and builds with checked packages. `kryon init` explains a kept floating toolchain when the lock refuses it.

- The kryon project tool reads its command line from argv directly, so restricted sandboxes without procfs can run Kryon builds.

- Add miniature workspace tiles with real window geometry and symmetric centered panel bands.

- Share Sweet dark window chrome and compact dark panel style tokens across native and ZX applications.

- Hosted desktop ZX shells can run approved child bundles in their own
  widget scopes with separate state, assets and native service grants.

- Hosted ZX surfaces can lend packed RGB strips directly to a presenter,
  avoiding per-pixel RGBA conversion and a second guest strip buffer.

- Hosted ZX surfaces can schedule state checks separately from composition,
  avoiding widget-tree rebuilds on unchanged idle timeouts.

- Linked desktop ZX launchers can supply per-instance typed host bindings
  alongside Kryon's frame, drawing, window and input capabilities.

- Android applications can opt in to GLES composition and EGL presentation for
  Pixmap frames, with retained damage and a software fallback.

- Native PDF exports can draw caller-owned RGBA pixels and measure complete
  wrapped text before pagination, preserving image proportions and searchable text.

- Pixmap applications that supply their own framebuffer can register, clear and
  reload images without requiring the built-in framebuffer.

- Native grids can measure dashboard rows and spans from item heights, apply
  CSS tracks and spacing, and compose cards with stylesheet padding. Native
  heading, paragraph and link selectors match their styled text widgets.

- Pointer drags no longer activate controls on release, including after moving
  away and back. X11 window-manager grabs no longer become application clicks.
- Portable ZX players forward each frame's resolved cursor to the host so
  bundled apps get pointer feedback without app-specific host code.
- Browser ZX players accept and apply bundled frame cursor requests, including
  system shapes, hidden cursors and bitmap cursors.
- Android links native image codecs from the app's checked Ziran graph.
  Native AVIF and WebP decoding are opt-in. AVIF decoding uses dav1d and
  includes libaom when the app uses its engine or encodes AVIF.
- Android uses system text and emoji by default. New app templates use an
  event-driven frame loop that sleeps at idle and paces active frames with
  the display.


- Keep Android codec builds reproducible and exclude build-machine paths from library loading.

- Keep hidden Android status bars out of the safe-area inset while preserving visible bars, camera cutouts and the keyboard; hide the status bar consistently on older Android devices without leaving a blank strip.
- Cover shared half-pixel image edges consistently in smooth raster draws, preventing background seams between adjacent tiles at fractional display densities.
- Native hosted style codecs preserve width and height across record encoding,
  decoding and validation, while retaining older host field layouts.

- `kryon/window_chrome` provides a themed title bar with window actions shared
  by native and ZX hosts.
- Portable ZX surfaces can retain the previous presentation and reconstruct
  changed pixels in bounded raster strips while still processing input and
  idle callbacks.
- Portable ZX apps can stream changed rows to a host that retains its pixels,
  reducing guest framebuffer memory for larger windows.
- Concurrent desktop builds can share completed AVIF libraries safely.
- Hosted ZX surfaces route wheel input to Kryon widgets and can sample an
  optional monotonic clock during frame callbacks.
- Portable apps can share GTK style records with the native GTK renderer.

- Preview fixtures build once for comma-separated viewport widths and produce
  a PNG and widget tree for each width. The machine-readable HTML map lists
  native widget imports, entry points, and layout equivalents.
- Retained layouts accept styled width and height. Keyed children can animate
  entry and resize, and apps can use a presence sample to animate exit.
- Opt-in retained style transitions and keyframes animate native widget paint.
  Keyed layout reorders ease through the same geometry used for input.

- `kryon/motion` provides CSS easing, timed keyframes, and interruptible
  value transitions for native widgets and charts. Samples tell apps when
  their values have settled so they can stop requesting animation frames.
- The ZX desktop player clears to the guest's default canvas color.
  Retained paint keeps its text styling state usable without a glyph
  measurement host.
- The dashboard starter ships a locked dependency set and a quick start for
  rendering its first preview.
- Smoothly scaled image meshes preserve the opacity of thin textures and
  keep transparent edges feathered.
- `kryon preview` renders a named UI fixture at a chosen size and theme to a
  PNG and a JSON widget tree. The dashboard example shows responsive layouts,
  forms and preview states, and the HTML-to-Kryon guide maps common elements
  to native widgets. Native layouts compose rows and grids, and text fields
  bind directly to caller-owned state. Web projects can choose the `dom`
  backend to render semantic HTML with browser CSS and focus behavior.
- Kryon apps compiled to `.zx` can open a TaijiOS guest window, draw text and
  widgets, and respond to pointer, keyboard, resize and close events.
- Android viewport snapshots copy only the retained app content for route
  fades, keeping system-bar pixels out of the transition.
- Normal Android builds start when a previous UI test left its private
  test-driver folder behind; only builds with the driver use that folder.
- Animated glTF material primitives skin and upload only their referenced
  vertices, preserving the original triangles, expressions and pixels.

- Android shipped pictures probe dimensions before decoding and retain a
  Lanczos reduction at the drawn device resolution, reusing it until a
  larger draw needs more detail. Intrinsic dimensions stay stable.
- Scene textures fingerprint with a quarter of the multiplies: the
  fingerprint still covers every byte but multiplies once per four-byte
  pixel, across four independent lanes, and folds in the byte count.
- Android apps keep Ziran's native code cache: Kryon points
  `ZIRAN_NATIVE_CACHE` at the app's `cache/ziran-native` folder unless it is
  set, so a portable app's native code loads on later starts instead of
  compiling again.
- Smooth image meshes use the ordinary image filter, avoiding stalls when
  animated cells stretch a narrow source strip in opposing directions.
- Smooth pictures drawn without a prepared smooth copy, such as tiles cut
  from one sheet, use Cairo's good filter again: 0.6.0's best filter reached
  past the source rectangle and bled neighbouring tiles into the seams.
- Web builds with `KRYON_WEBGL2` render scenes on WebGL 2, and
  `SceneGpuRenderCanvas` presents them as a canvas image without readback.
- A desktop application that stops taking keys no longer stalls its window:
  once a pending input queue's head stays put across two polls, keys stop
  waiting in SDL's queue for room, so wheel, pointer and window events
  behind them still arrive.
- AVIF pictures decode faster on phones: 8-bit 4:2:0 and 4:2:2 colour
  converts across the cores with exactly the same bytes as before.

## [0.6.0] - 2026-11-18

- Kryon builds with Ziran 0.7.0.
- Web pages scroll by the browser's pixel wheel distance, mouse drags select
  text while scrollbar thumbs stay draggable, and selected text copies with
  Ctrl+C under a text cursor.
- Scenes add large pictures faster: a texture's texels are copied in one
  block instead of one at a time.
- Checking or building a package for a native target no longer fails in
  `page_events.zi`; its browser event bridge exists only on web targets.
- Packages can import `kryon/canvas_os` and `kryon/pixmap_image_owner` to
  supply bundle pictures on the web and Android.
- `make gallery-zx` exports all 101 gallery examples again. It binds the
  frame entry by Ziran's package-scoped module names, and an example that
  imports an export such as `kryon/modal` gets that export's own module.
- The website's Apps page plays every published Kryon web app in the
  browser: Werkzeug, Inner Breeze, Jungle Academy, Arcade, Shovey, Apfelwurm
  and Choano. Each Kryon Labs desktop app says why it has no browser build yet.
- The Cairo desktop host leaves out the sliver triangles of a warped image, too thin for Cairo to sample, instead of losing the whole frame.
- Android app libraries retain the JNI entries for local reminders and
  notification permission requests.
- Desktop JPEG export accepts images with alpha on Glycin-based systems.
- Native ZX player builds on macOS select Darwin host code.
- GLES scenes keep GPU shadow filtering on devices without hardware depth comparison filtering.
- Android GLES scenes reuse linked shader binaries across launches and compile
  shadow sampling only for lights with shadow maps.
- The shader benchmark measures eight shader workloads and four animated scenes with mean and p95 frame times.
- Android keeps the active frame budget while retained paint changes and backs off when paint becomes stable.
- Android keeps the active frame deadline while a pointer is held, including
  when an intermediate paint is unchanged.
- GPU scenes upload camera, lighting, and grade uniforms once per program
  per frame.
- 32-bit Android on PowerVR GE8320 reads completed scene frames without a
  slow copy from mapped GPU memory.
- Android presents at the native surface size, uses Lanczos for smooth image
  scaling, and uploads scene geometry correctly on 32-bit GLES devices.
- Desktop animations continue presenting text and images without input.
  Retained paint compares each bitmap's content revision, so changed pixels
  repaint through the same handle and identical frames still skip presentation.
- Multiple indexed meshes in one scene render correctly on CPU and GLES.
- GLES retains indices, UVs and other unchanged attributes for animated
  smooth-shaded meshes. Each material sampler keeps its texture on the first draw.

## [0.5.0] - 2026-11-17

- Kryon no longer includes raylib: its raylib hosts, raylib ZX player,
  `renderer-raylib` artifact and `backend = "raylib"` are removed, so no app
  built with Kryon downloads or links raylib. Desktop apps use the SDL2/Cairo
  host, browser pages the Canvas host and Android apps the NativeActivity
  host; native audio plays through Ziran's core audio modules and browser
  asset and PCM audio through WebAudio. Builds with Ziran 7fbc8152.
- The desktop host gives terminal-style apps what they used raylib for:
  `OpenDesktopWindow` opens undecorated, always-on-top or always-running
  windows, `kryon/desktop_activity`'s `DesktopWaitForActivity` sleeps until
  the program's own descriptors, such as a PTY, or window input are ready,
  `DesktopFrameRate` sets the drawing rate, `DesktopExitKey` ends the loop on
  a key, `PollMiddlePointer` reports the middle button,
  `DesktopSetFullscreen` and `DesktopToggleFullscreen` fill the screen, and
  `DesktopPrepareGlyphs` loads fallback fonts before a frame draws them.
- Windows desktop programs link fonts and pictures given as bytes: their
  files are temporary files there, as they are in-memory files on Linux.
  The image decoders keep their GdkPixbuf slots private to their module, so
  a program that links ws2_32 keeps Winsock's `connect`. Building with
  `--define KRYON_AVIF_LINKED` and `--define KRYON_WEBP_LINKED` binds the
  libavif and libwebp the program links instead of loading `libkryonavif`
  and the system libwebp.
- `kryon/scene_skin` adds indexed scene surfaces with four-influence skinning
  from bind data, and `kryon/watercolor` a portable paper/pigment material
  with a finishing pass for brush, grain, pigment pooling and pencil
  contours; both run on the CPU renderer and the translated GLES path.
  Desktop `LoadImagePixels` accepts an optional host path resolver for
  bundled assets.
- `SystemPaletteFollow()` (in `SystemThemeHostLinux` and the portable
  `SystemThemeHost`) applies the current palette, the host's while an app is
  embedded, once and again whenever the host changes it; one call per frame
  makes an embedded app follow its host's theme change. Applying
  (`SystemPaletteApply`) installs the style rules and sets the active theme's
  colours, so code drawing from `CurrentPalette()` follows too.
- `SystemTheme.SystemPaletteColors(palette)` expands a system palette, such as
  a host's, into a full theme `Palette`, so an app that draws from one matches
  its host completely.
- A program entry builds in a profile with `codegen = "zx"`: its bundle is
  the program's own `main`, with portable IR, so `kryon/app` gives it the ZX
  window. In a canvas profile the ZX canvas player binds `zx_window` to the
  page's canvas (`window_page_player.zi`), lowers the bundle to Wasm in the
  page and runs `main` there, each wait for an event yielding to the page, so
  `examples/window.zi`'s ZX shows its window in the browser; the editor's
  Wasm platform opens it there. Both players share `zx_window_bundle.zi`.
- `zx_window_host.zi` is the desktop's zx_window capabilities as a host
  library: `make zx-window-host` builds `build/lib/libkryon-desktop.so` and
  `make install-user` installs it and registers it as
  `~/.local/share/ziran/hosts/kryon-desktop.library`, so `zx2native` links
  `examples/window.zi`'s bundle into a Linux executable that opens its
  320x160 "Window" itself; the editor's Native Assembly, Binary and Open in
  window use that executable. `make zx-window-test` also runs it on a private
  Xvfb and checks that with no library registered `zx2native` stops at the
  gap naming `ZxWindowOpenHost`.
- Android apps can own their main loop: `kryon/android_runtime` polls
  NativeActivity lifecycle and input with `PollAndroidHost`, and
  `BeginAndroidFrame`/`EndAndroidFrame` paint and present Pixmap's
  device-pixel damage. Pixmap's `RasterSetTransform` and
  `RasterPointFromPixels` share one camera for painting and input.
- On Android, pictures an app ships decode from its APK assets the first time
  they are measured or drawn, through `kryon/pixmap_assets_android`, which the
  Frame host imports. `kryon/pixmap_typefaces_android` shapes text with
  Android's Paint and Canvas through the ordinary `typeface_source`
  capabilities, with system font fallback.
- The `renderer-desktop` artifact also builds for Android ARMv7 and AArch64:
  WebP, AVIF and Opus archives and the shared AVIF bridge, with no renderer.

## [0.4.0] - 2026-11-16

- Builds with Ziran 0.6.0. ZX apps use Ziran's executable format on every
  target, including browsers, and run as native code by default on Linux
  x86-64 and AArch64; the widget scenarios run natively in 8.6 µs (Button),
  9.6 µs (Text) and 13.1 µs (Input) per two frames, 121–185× faster than with
  Ziran 0.5.0. The `--vm` option is removed.
- Programs carry only the features they use. `App.Show(window)` opens a bare
  window and `App.Run(window, Draw)` adds frame input, scrolling, navigation
  and widget accessibility. Pictures, text input, scroll devices and Unicode
  grapheme tables come only with the widgets that use them, and empty desktop
  windows omit image decoding, smooth scaling, video and threads.
- Import widgets by package, such as `kryon/text`, and use `Text.Draw` and
  `Text.Props`. The `Kryon`/`Widgets` facades, the old props and widget
  aliases and the unused DOM web backend are removed; web projects use
  Canvas2D. `kryon/markdown_preview` and `kryon/markdown_editor` are
  exported.
- Portable apps run their window from a bundle: `ziran run window.zx` runs
  `App.Show` as native code in the desktop ZX player, and `kryon/bitmap_image`
  shows pictures a bundle makes at run time. A `KRYON_HOSTED_UI` bundle can
  import `kryon/menu`, `kryon/layout` and `kryon/toolbar` for their props and
  state only, and hosted widgets accept record and slice types named through
  the package or bare.
- The Linux desktop host can run an app's windows, input and raster without
  raylib, with `kryon/desktop_keyboard` for held and repeated keys, and says
  which step failed when it cannot open a window. `kryon/tree_frame` is public
  for programs with their own desktop host.
- Desktop audio: `kryon/audio_mixer` mixes sounds, streamed music and PCM
  streams with gain, pitch, loops and a mix tap, played through
  `kryon/audio_sink_sdl`, with Opus, WAV, MP3 and Ogg Vorbis decoders.
- Painted scenes gain moving shadows from window beams, daylight that follows
  latitude and time of day (`Scene.grade`, `kryon/daylight`), light through
  elliptical openings and `LightCookie` texture projection. CPU and GPU
  rendering match, and GPU scenes update only moved geometry
  (`FreezeGeometry`).
- Apps that Harmony embeds draw in Harmony's theme by default on Linux and
  follow its theme changes.
- Desktop, Canvas and Raylib share ordered input delivery, keeping keyboard
  layouts, repeats, IME text, gamepad navigation and focus loss. Fast
  modifier shortcuts keep their modifiers, test-driver keys retry after
  backpressure, and context menus align accelerator labels and checks. On the
  web the wheel scrolls what is under it even after a click elsewhere, and a
  `#canvas` with `data-transparent` has no background.
- Secure TextFields show a mask when no `display_value` is given; secrets stay
  out of paint, accessibility and clipboard data. A drag in a TextField keeps
  its selection when the pointer is released.
- SDL desktop and Linux Raylib hosts expose the PRIMARY selection, Linux apps
  can drag MIME data to other applications, and GTK painting is faster and
  follows display changes.
- `kryon build` supports `main` entries using `kryon/app` on desktop and web.
  Browser projects pick the correct target and native project builds link
  desktop system libraries automatically.
- Fixes: Raylib windows show exact colours where SDL 2 runs on SDL 3 through
  sdl2-compat; Android activities without Kryon's optional Java callbacks no
  longer break later JNI calls, and Android builds compile Ziran's runtime
  with the pinned toolchain; Kryon builds from its own pinned packages again,
  and renderer artifacts accept local overrides and no longer depend on the
  Makefile; hosted widgets bundle to ZX again; browser bundles with string and
  record host calls start; opening a bundle rewrites only changed assets; the
  time picker and desktop image examples build; Android renderer artifacts
  configure again; the website's live preview and gallery build again.
- On Android, `kryon/image_pixels` uses the linked WebP and AVIF codecs for
  scene textures, including on Android 9. If a texture requires the Android
  Bitmap fallback and the activity lacks `decodeBundleImage`, Kryon returns
  an empty image and logs the missing method once.

## [0.3.0] - 2026-11-12

- Hosted bundles can measure approved images without arbitrary file access.
- Custom hosts share application, frame and window APIs. Native icons retain
  upright RGBA pixels. Android custom Raylib hosts handle private files, touch,
  clipboard and resumed drawing correctly. Browser RGBA previews report
  whether encoding is available.
- Raylib hosts can opt into AVIF decoding without adding it to other builds.
- Hosted Widgets exposes supported contracts, including TimePicker and Spinbox,
  without pulling in retained widget trees.
- Redesigned the Guide, added a landscape Image example and theming guidance,
  and corrected Hold/Confirm progress borders.
- Graphical bundles launch through `ziran run app.zx` and `kryon run` after
  provider registration. `--vm` selects portable execution.
- Disabled EmptyState and Banner preserve their controls and reject activation.
  Carousel can disable navigation, gestures and actions together.
- Android generation preserves custom and ZX host entry contracts.
- Components have their own source folders while keeping public import names.
  Package imports expose extended components and their public types.
- Menu, TabBar, NavigationBar and Collapsible support caller-owned spring motion,
  smooth reversals and reduced motion. Closing menus release interaction while
  settling. Portable TabBar calls reject unsupported motion-state pointers.
- OneTimeCode centers digits, ScrubField aligns values and labels, and motion
  examples show step counters. The native gallery slows down when paint settles.
- Native bitmap caches support existing compiler pins. Smooth-image caches keep
  several sizes of a picture so covers stay sharp beside thumbnails.
- SwipeRow supports short and long actions in either direction, with distinct
  release results and equal-width previews.
- TextArea corrections preserve the caret viewport. Unchanged TextArea and
  TextField snapshots retain selection; pointer selection respects scrolling.
- Linux apps can apply GTK CSS locally or process-wide and temporarily select
  dark styling with nested restoration.
- Apps can handle F3. Modified Tab remains available; TabBar transfers focus
  when tabs are removed or disabled. Editable TextFields can reserve plain Tab
  for completion while Shift+Tab still moves focus.
- Breadcrumb supports keyed navigation, caller-owned selection and focus,
  Unicode labels and ancestor overflow.
- Raylib supports managed RGBA images with tint and updates, preserving session
  pictures across graphics resets.
- Linux apps can publish and read multi-format MIME clipboards, including large
  transfers, and release only their own publication.
- Icon and compact grids support multiple selection, keyboard navigation and
  scrolling to focus.
- Native apps carry assets into builds and installs so images load regardless
  of the working directory. Private install prefixes stay private.
- Linux apps can use GTK3 themes, icons and Pango text with markup and custom fonts.
- Browser test launches accept extra Chromium flags, including WebGL settings.
- DOM images report their size after decoding and stay visible; invalid images
  report unavailable.
- Raylib apps, including custom 3D hosts, can build as WebGL pages with assets.
- Painted labels and image paths are copied, so callers can release them at once.
- Web scenes retain recently used images and avoid decoding them every frame.
  Supported browsers use WebAssembly SIMD scaling, with a baseline elsewhere.
- Smooth images use Lanczos filtering across backends without blocking the UI.
  Repeated scene scaling reuses the result. Native AVIF decoding uses shared
  codecs and CPU acceleration on desktop and Android.
- Tall buttons keep rounded corners and large pictures. Neutral soft buttons
  use a light accent tint. Buttons and embedded modules preserve the surrounding
  theme, and title bars follow the app palette.
- Web touch and click coordinates account for safe-area padding. Buttons click
  once even if moved mid-press, and releasing an outside press over one cannot
  lock input.
- Kryon Labs app and module previews show availability and an editable gallery.
- Text rotates around a chosen origin at full size on native, web and Android.
- Apps can draw and update RGBA pixels through Image without files, and import
  image and SVG shape modules directly.
- X11 idle time includes typing in other apps. Custom cursors persist until
  changed; app-wide cursors cover all sessions. Bitmap cursors avoid flicker
  and light edges. Pointing cursors appear only over clickable targets.
- Widgets gain Material-style tones and states. Bottom navigation shows icons,
  labels, selection and focus clearly.
- Desktop and web gamepads support game and View/Select navigation. Modal titles
  are accessible.
- Custom Linux x86_64 hosts can opt into an SDL3 GPU renderer without a global
  SDL3 install. SDL pixel hosts use GPU painting for text and terminal cells,
  with CPU fallback.
- Styles and repeated labels resolve faster with less frame overhead.
- Android absolute file paths work without a running activity. Renderer packages
  link their image codecs automatically. Frames always respect system bars and
  cutouts; backdrops can request `AndroidWindowBounds`.
- Images load lossy, lossless, transparent and first-frame animated WebP on all hosts.
- Browser renderers share decoded image data and reuse image dimensions.
- CJK and symbol-heavy text renders faster through improved fallback-font reuse.
- Linux Raylib apps can wait for window or descriptor activity and prepare a
  screen's glyphs together. Shortcuts follow the active keyboard layout and
  retain modifiers during quick taps.
- Android apps save documents and extract assets correctly in private storage.
- Test-driver keys appear in frame input snapshots as well as key queues.
- Native profiles can link verified dependency support artifacts, including
  audio alongside their renderer. Game builders can share renderer validation,
  resources and target-specific link settings.
- MediaAudio supports Pause/Resume, retains paused PCM queues and reports
  remaining asset-music playback time on native, hosted and browser hosts.
- Android document pickers accept several file types without exposing all types.
  Document storage can be tested natively without a browser SDK.
- Media supports independent sound effects and looping music with gain, stop,
  unload and status controls. Suspended or unavailable audio reports explicitly;
  disposal and restart release the app's voices.
- Desktop and Canvas publish owned frame input snapshots while preserving widget
  queues and clearing state after callbacks, cancellation and teardown.
- Browser apps distinguish right and middle clicks. Captured areas preserve
  Space/Enter and follow drags released outside the area or over another control.
- Web `Press` reports tap positions; keyboard activation has no point.
  SwipeArea discards touches released while hidden.
- Updated images remain visible until replacement data decodes. Unchanged images
  are not decoded again on desktop, Android or web.

### Added

- `frame_snapshot.CaptureFrameInput` returns owned held, pressed, released and
  repeated keys, pointer state, focus and timing without consuming legacy queues.
  Hosted reads stay within the active session and frame.
- TextField state and edit helpers update caller-owned bytes, validating ranges,
  UTF-8 and capacity atomically, including overlapping replacements.
- Windows and pages use the install or package name. ZX players use a nearby
  title file or the bundle filename; unnamed programs use their filename.
- DirectionPad offers four-way touch input with immediate steps, hold repeats
  and sliding between directions, including hosted games.
- Android UI tests can inspect and drive debuggable apps through a private
  `kryon-test-driver` folder.
- Text supports underlined theme-colored inline links and pointer hit testing,
  including portable hosts.
- Line, Circle, Ring and Triangle add clipped, noninteractive shapes. Circles
  and Boxes support gradients across native, web, terminal and portable renderers.
- The optional test driver inspects widgets, text, bounds, focus and state;
  finds controls; clicks, types, presses keys and drags; and waits for frames
  to settle without screenshots. It also reports clipped wrapped text.
- Images can warp over a point grid for puppet-like animation, including
  mirrored grids and clipping. Callers can immediately reuse the point buffer.
  Portable bundles accept up to 256 points.
- Android profiles build, install and run APKs from the same app sources as
  desktop. Initial generation preserves customized Android files. Native apps
  support edge-to-edge drawing, touch, soft-keyboard editing, density, dark mode,
  orientation and an unhandled Back key that closes the app.
- Widgets can retain their own state between frames on their tree node.
- Press reports clicks, holds and pointer positions without painting. It takes
  keyboard focus and optionally captures drags while preserving scrolling.
- Grid helpers calculate responsive columns, rows, heights, cell bounds and
  hit testing for tiles, overlays and drag-and-drop.
- Android apps can use `kryon/http_transfer` without Java transport code, using
  platform TLS and certificate checks. Redirects and caches are refused and
  timeouts fail explicitly.
- TimePicker provides a themed 24-hour field and hour/minute popup with touch,
  fling, wheel and keyboard navigation. Optional Now, Done and Clear buttons
  update the live selection; the popup stays on screen. Portable hosts support it.
- TitleBar and ModalFrame support strong titles. ModalFrame can align its title
  with content or the left action.
- Text can reveal characters progressively without splitting Unicode characters,
  changing layout or hiding its full accessible label.
- Image smoothing can be enabled per picture; images without it keep sharp pixels.
- DismissibleOverlay reports backdrop or panel-background taps even when it
  cannot be dismissed.
- Scroll can reserve scrollbar space before content overflows, keeping columns
  stable. Inactive scrollbars ignore input.
- Overflowing TabBars support swipe and wheel scrolling without accidental
  selection; selected tabs leave neighbors partly visible. Reordering still works.

### Fixed

- Desktop numeric-keypad digits work, and simultaneous keys support multiplayer.
  Held arrow keys keep moving in browser apps and games.
- Warped-image animations stay smooth on slower browsers and refresh when images change.
- Accessibility preserves final replies on exit and requested coordinate spaces.
- Reading a clipboard image no longer stalls after copying text in the same app.
- Popups draw fully outside their parent scroll area or dialog. Dropdowns follow
  light and dark palettes.
- Android content stays clear of navigation bars, keyboards and cutouts on older
  phones too. APK fonts load by relative path, including before Android 10.
- Tab and Shift+Tab traverse enabled controls with visible focus. Enter and
  Space activate keyboard-focused controls; clicking hides the ring and returns
  those keys to the app.
- Dropdowns retain their own open state, highlighted row and scroll position.
  Applications supply the selection and can close the menu from code.

## v0.2.0 - 2026-09-20

### Added

- Android secure storage protects secrets with biometric or user-authenticated
  Keystore keys.
- Sweet becomes the default app palette, matching Sweet-Dark in dark mode.
  Go exposes Plan9, Xfce and Sweet palettes, full Default color roles and
  style-to-palette pairing.
- Scroll containers can leave room for borders, focus rings and state layers
  around their viewport.
- Patch releases follow successful master CI, with explicit minor/major tooling.
  Checksummed native tool archives accompany SDK and renderer downloads.
- Standalone and CI builds link notification and GTK tray dependencies correctly.
- `KRYON_MEM_DEBUG` enables application and font memory diagnostics.

### Changed

- Widget, layout, interaction, style and tree behavior is authored in Kry and
  compiled to C and Go. Clipboard callbacks use host-provided writers.
- Font reuse reduces memory and repeated rasterization while keeping large text
  crisp and native-script locale lists responsive.
- `KRYON_TRAY_GTK_DL` loads GTK only when the tray is used.
- Terminal provides a resizable shell with keyboard input, screen polling and
  basic cursor movement and erasing.
- `k2c --emit-krb` creates cartridges with generated C hosts and live bound state.
  `kryon-preview cartridge` previews Kry or KRB files without building an app host.
  Existing C projects keep working, and Raylib backend renaming works correctly.
- Kry adds `defer` for reverse-order cleanup on block exits, resolves imported
  type names across modules and accepts large functions without a fixed body cap.
  Compiler cleanup no longer leaks memory.
- Parsing reports multiple recoverable errors in source order rather than
  stopping at the first. Fatal errors still report earlier diagnostics.
- Standalone Kry apps can own their frame loop through `init`, `frame` and
  `shutdown` hooks, including nested rendering and inspection overlays.
- Platform helpers provide nonblocking processes, filesystem access and dynamic
  library loading to Kry apps. Windows currently has stubs.
- Krait moves to its own repository as the Kry-written IDE. It offers project
  opening, a file tree, editable tabs, saving, dirty markers, undo/redo and live
  previews. Console and problem panels run commands and show build diagnostics.
- Hot reload and run builds no longer freeze the IDE; completed builds load
  promptly. Source-editor copy, cut, paste and select-all work consistently,
  and large clipboard selections no longer truncate.
- Library objects link into PIE executables and dynamically loaded hosts.
- The bundled C IDE is removed. `kryon-app` becomes `kryon` for building,
  packaging, running, previewing and local Ksync; `kt ide` launches Krait.
- Slider accepts `value_text_override` for custom labels. Callers must add
  the argument; NULL keeps the numeric label.
- Shared titlebar, drawing and text-editing helpers reduce duplication.
  Preview documentation describes the actual build-and-reload flow.
- Linux and FreeBSD run local-test equivalents on pushes and pull requests.

### Removed

- Removed the unused Clay layout dependency.

## v0.1.90 - 2026-09-08

- Isolate auxiliary window focus state

## v0.1.89 - 2026-09-08

- Isolate native tab state per render host

## v0.1.88 - 2026-09-08

- Keep dropdown keys within popup ownership

## v0.1.87 - 2026-09-08

- Keep popup menu Escape within its owner

## v0.1.86 - 2026-09-08

- Focus native sliders from pointer presses

## v0.1.85 - 2026-09-08

- Add native Linux IBus text composition

## v0.1.84 - 2026-09-08

- Remove legacy UIForm aliases

## v0.1.83 - 2026-09-08

- Unify C text composition handling

## v0.1.82 - 2026-09-08

- Render native IME preedit accurately

## v0.1.81 - 2026-09-08

- Add native word text editing

## v0.1.80 - 2026-09-08

- Centralize C text navigation

## v0.1.79 - 2026-09-07

- Unify text selection navigation

## v0.1.78 - 2026-09-07

- Add multiline TextArea page navigation

## v0.1.77 - 2026-09-07

- Restore premultiplied paint layer compositing
- Make tab capacity audit resilient to dropped clicks
- Clean up default button rendering
- Open numeric editors on double click

## v0.1.76 - 2026-09-07

- Add temporary input to numeric controls

## v0.1.75 - 2026-09-07

- Paint layers preserve transparency and text alpha. Text areas highlight Kry directives.

## v0.1.74 - 2026-09-07

- Profile pictures use rounded tiles and a shared transparent icon atlas.
  Paint layers preserve correct transparency when composited.

## v0.1.73 - 2026-09-07

- Synthetic taps activate controls; drags do not. Translucent layers composite correctly.
- Disabled bottom-nav icons remain visible; default navigation styling improves
  while retro tokens remain. Locale dropdown controls replace stale compatibility APIs.

## v0.1.72 - 2026-09-07

- Isolate and clean dropdown state
- Modernize profile picture icons
- Add text editor buffer helpers

## v0.1.71 - 2026-09-07

- Remove stale scaling compatibility alias
- Keep ScaleUIPx compatibility
- Add closeable collapsible headers
- Ignore mod-wheel in text area scroll
- Rename UI scaler API
- Use semantic title font in title bar
- Clamp retained text area scroll
- Soften title bar surface

## v0.1.70 - 2026-09-07

- Add composed native tab scopes
- Fix scroll clipping under render scale

## v0.1.69 - 2026-09-07

- TabBar state is isolated per widget. Logical density, core scaling and
  picture bounds are corrected; scaling APIs and FreeBSD builds stay consistent.

## v0.1.68 - 2026-09-07

- Composite buttons share consistent behavior, and native TabBars use the same
  navigation rules with renderer-independent keyboard tests.

## v0.1.67 - 2026-09-07

- Add keyboard controls to native drag widgets

## v0.1.66 - 2026-09-07

- Add native multi-select keyboard navigation

## v0.1.65 - 2026-09-07

- Add native toggle keyboard navigation

## v0.1.64 - 2026-09-07

- Unify keyboard interaction for choice widgets
- Add native slider keyboard navigation

## v0.1.63 - 2026-09-07

- Defer Text painting inside retained layouts
- Keep canonical Text visible in mixed UI frames

## v0.1.62 - 2026-09-07

- Update theme example runtime snapshot
- Expose title bar opacity as a style token
- Add native menu keyboard navigation

## v0.1.61 - 2026-09-06

- Add native list box keyboard parity
- Clip Text only when its bounds require it
- Preserve outlined icons and live Text layering
- Refine Default guides and warning artwork

## v0.1.60 - 2026-09-06

- Text becomes the shared centered-label API, preserving immediate and
  retained paint order. Default gains a clean return control and MingCute warning icon.

## v0.1.59 - 2026-09-06

- Text retains clipping and paint order with consistent TextProps usage.
- Native tables support clipboard operations and keyboard navigation, keeping
  Tab within cells. Popups own shortcuts and accelerators; card dispatch keeps
  reorder-release ownership.
- Carousel navigation is refined. Optional Sync uses consistent names and
  shared crypto helpers while preserving wire compatibility.

## v0.1.58 - 2026-09-06

- Popups retain active gestures. Optional sync is renamed Daochi.
- Return targets grow, dropdowns show chevrons, bottom navigation fits its
  content, and reorder targets follow lifted items.
- Text and semantic fonts respect density. Libdraw supports texture regions.
- Spreadsheet evaluation recognizes string literals and error names.
- Shared carousel controls honor secondary button styling.

## v0.1.57 - 2026-09-06

- Restore focus across popup lifecycles

## v0.1.56 - 2026-09-06

- Add canonical context popup scope

## v0.1.55 - 2026-09-06

- Prefer bundled UI font and stabilize widget audit
- Unify native text and composed popup widgets
- Add spreadsheet string-literal and error-name scanners to kry_eval

## v0.1.54 - 2026-09-05

- Dropdowns stay in view; native widgets reach interaction parity.
- Libdraw supports retained-paint matrices, and Windows SDL builds avoid header conflicts.
- Quad math adds pi, log, hypot, inverse trig and accurate sinpi/cospi.
  Spreadsheet helpers add cell addressing and tolerant rounding.
- TextArea line handling preserves multiline clipboard selections.

## v0.1.53 - 2026-09-05

- Windows uses native file dialogs. Libdraw supports rotated text, correct
  drawing order, rectangle clipping, gradients and additive blending.
- Shared KIR emits portable scalar operations without cgo.

## v0.1.52 - 2026-09-05

- Added double-double arithmetic, compensated accumulation, quad QR and
  shared expression checking. QR initializes wide matrices correctly.
- Cmark builds with CMake 4; Raylib builds create their lock directory.
  Libdraw routing fallbacks apply only on native Plan 9.

## v0.1.51 - 2026-09-05

- Libdraw gains gradients and software blend modes; shared image builds work again.
- Modal action and bottom-navigation icons follow theme tint, including Go.

## v0.1.50 - 2026-09-05

- Common.mk: default FONT_FILES to the bundled Noto face for vendored apps
- Ui_text: log the resolved default font source, warn on bitmap fallback

## v0.1.49 - 2026-09-05

- Added zlib, XML and gzip helpers, plus immediate-mode plots and sliders.
- Android restores system bars after focus and uses display density for layout.
  Web asset startup is more reliable.
- App builds honor declared themes; example builds and sanitizer coverage work again.
  Site builds preserve renderer docs and validate icons.
- Slider remains the public API. Held deletion repeats without backend repeat events.

## v0.1.48 - 2026-09-04

- The text-area and slider example now uses a polished responsive layout while demonstrating multiline editing.

## v0.1.47 - 2026-09-04

- Cartridge builds now accept `Screen` declarations as supported layout containers.

## v0.1.46 - 2026-09-04

- Static release archives now validate correctly when optional post-quantum cryptography support is disabled.

## v0.1.45 - 2026-09-04

- The text-area and slider example is now included consistently in generated compatibility documentation and release packages.

## v0.1.44 - 2026-09-04

- Multiline text areas now start at the top, accept new lines with Enter, and keep the caret visible as text wraps.
- Large Default text areas keep normal field corners without changing button shapes.
- Added an interactive text-area and color-slider example.

## v0.1.43 - 2026-09-04

- Keep Win32 close hook safe across subclasses

## v0.1.42 - 2026-09-04

- Consume Win32 core window close requests
- Render: enable anti-aliasing by default

## v0.1.41 - 2026-09-04

- Added guide paging, swipe gestures and outlined buttons. Ksync sends real
  PUT/PATCH verbs. Failed font rasterizations stop retrying for the same size.
- Single-instance handling improves shutdown and stale-lock recovery, with
  SIGUSR2 backtrace diagnostics. Platform-specific symbols build correctly.

## v0.1.40 - 2026-09-03

- Polish the icon set

## v0.1.39 - 2026-09-03

- Images decode across backends, Canvas supports texture updates and radial
  gradients, and duplicate image APIs are fixed.
- Daochi/Ksync is optional. Kryon Labs gains redesigned previews, light/dark
  themes, platform icons and the crayon color system.
- Added and packaged `k2cpp` with C-backend parity; its `--plan9` flag is removed.
  `k2g` becomes `k2go` and `k2ir` becomes `k2kir`, including tooling references.
- Rejected single-instance launches no longer crash. Screenshot readback works
  when the usual OpenGL entry is unavailable.
- Scenes gain Light2D, direct Body2D transforms and velocities, sensor enter/exit
  signals that ignore static visitors, custom signal dispatch and correct world
  transforms before ready hooks.

## v0.1.38 - 2026-09-01

- Added responsive score and segmented controls, immediate buttons,
  dismissible overlays and native spritesheet helpers.
- Apps can register fonts from a text corpus; subsetting falls back to fonttools.
- Added per-user app data roots and local-network sync URLs. Android storage
  respects platform limits, and viewport and density scaling are corrected.
- Plan 9 apps gain shared build rules and safer compiler output for nested
  literals, arrays, casts, declarations, pointers and initializer expressions.
  Type resolution and prototype parsing handle more valid code and reject
  invalid forms correctly.
- Plan 9 supports sync crypto, pages, overlays, spritesheets, cursor visibility,
  file renaming, texture validity and missing math/limit helpers. Rendering
  gains texture updates and web-routing stubs.
- Canvas input avoids duplicate bubbled events, embeds resize to their frame,
  frames follow browser vsync and target FPS, and random values and texture
  filters match native rendering.
- Docs previews render directly on canvas; showcase deployment and Debian icons
  are refreshed. Automation accepts test options and ignores empty settings.
- KIR parsing handles recursive expressions correctly. Language tooling, GTK
  tray compatibility and build diagnostics improve; storage and cartridge
  tests link their required helpers.

## v0.1.37 - 2026-08-30

- Encrypted sync payloads are compressed. Canvas sizing and pointer input
  follow the window-sized layout.
- Added reusable app storage, Android services, screen/form helpers and
  post-frame callbacks.
- Canvas fixes render-target copies and audio decoding blocked by autoplay,
  supports app routing and avoids oversized font scratch allocations.
- Go exposes Primary, OnPrimary and SurfaceVariant theme colors.
- Native trays and action notifications use pure-Go DBus, register correctly
  with XFCE, serve menus and recover the session-bus address when needed.
- Context menus draw above content. Relative mouse mode falls back when raw
  mode fails. Debug logging covers taps and tray connections.
- Showcase project loading and banner refreshes work correctly. Compiler helper
  cleanup and headless/parity tests improve build reliability.

## v0.1.36 - 2026-08-30

- Unify Kryon site playground entry

## v0.1.35 - 2026-08-30

- Remove README link badges
- Refresh Kryon README header
- Harden KRB asset section loading
- Use host arrow cursor for libdraw windows
- Add checked JSON allocation helpers
- Add libdraw key name lookup

## v0.1.34 - 2026-08-30

- Serialize the cmark-gfm vendor rule with a grouped target
- Add page semantics and DOM route API

## v0.1.33 - 2026-08-29

- Skip rendering unchanged frames and reuse the present-path buffers
- Add rlActiveTextureSlot to the curated rlgl tier
- Build app sources and vendor deps before the linux arch compile

## v0.1.32 - 2026-08-29

- Deliver full clicks from the X11 backend so Button widgets respond

## v0.1.31 - 2026-08-29

- Implement TabBar in the pure-Go runtime

## v0.1.30 - 2026-08-29

- Make cursor-locked SDL mouse behave like GLFW disabled-cursor mode

## v0.1.29 - 2026-08-29

- Fix the X11 window runtime dropping all compat input reads
- Keep rlgl wrappers when audio is compiled out
- Resolve rlgl and raymath inputs next to raylib.h in the compat generator
- Add the 3D tier to the kryon surface
- Let headless links resolve the instance window hooks

## v0.1.28 - 2026-08-29

- Add custom-header variants to kry_http

## v0.1.27 - 2026-08-29

- Document explicit C extern handling
- Polish JavaScript lowering integration
- Add derived theme color helpers
- Add JavaScript lowering pipeline
- Handle unquoted web script cache busting
- Avoid duplicate Linux directory rules

## v0.1.26 - 2026-08-29

- Add DOM web backend smoke coverage

## v0.1.25 - 2026-08-29

- Install ripgrep in release workflow

## v0.1.24 - 2026-08-29

- Add Android window inset API
- Install ripgrep in Linux CI
- Degrade gracefully when instance lock is unavailable
- Select retained text fields on double click
- Add release and sanitizer check targets
- Track backend capabilities in preflight
- Harden Kryon boundaries and preflight checks
- Fix Android surface resize cache
- Sync frame size for platform UI

## v0.1.23 - 2026-08-28

- Use detected Android UI scale
- Scale default UI fonts

## v0.1.22 - 2026-08-28

- Improve Canvas2D backend parity
- Add Android prelude activity
- Fix Ksync crypto web warnings
- Fix Android UI DPI scaling

## v0.1.21 - 2026-08-28

- Unix and BSD home builds are recognized as source installs for updates.
- Added layout-aware key input, terminal UI primitives, a rich-text editor and
  reusable app framework helpers. Native Linux Raylib builds resolve backend names.
- Go exposes held keys, full theme settings and Plan9, Xfce and Sweet palettes.
  Theme IDs stay consistent with the native catalog.
- Sweet becomes the Default palette with matched colors; Xfce and Plan9 themes
  are added. Neutral tones remain unsaturated, and apps can supply system palettes.
- Android secure storage supports authenticated secrets. Dropdowns hide text
  input, modals handle release correctly and scroll views allow border bleed.
- The termi backend draws textures through sixel with visible fallback, caches
  unchanged pictures and batches output. Partial writes, viewport clipping,
  animation, translucent cell backgrounds, circles, thick lines, button states
  and mouse delivery work correctly. Sixel is enabled only on capable terminals.
  The terminal showcase is renamed ktrem; the earlier TUI backend is reverted.
- Icons decode as RGBA, monochrome icons become masks and bottom navigation
  preserves its colors and tint. Added eye and fingerprint icons and shared icon assets.
- Native Plan 9 gains libdraw rendering, system-theme support, installation,
  font scaling, image masks, fractional texture sources and audio mixing/streams.
  Icons retain colors, rounded corners match other backends and text respects
  hierarchy and clipping. Terminal panes resolve colors without platform conflicts.
- Plan 9 builds handle their compiler's literals, comments, headers, types and
  preprocessing correctly. Platform sources stay out of unrelated builds;
  physics-dependent scenes are excluded, and hosted theme builds work.
- Backend switches rebuild objects. Shared source generation uses the host
  compiler even during Android builds. Renderer and widget conformance matrices
  cover C, Go, Canvas, libdraw and cartridges and appear on the website.
- Rect is the canonical shape widget. Retained trees preserve button paint order;
  hosted apps isolate keyboard input and receive expanded lifecycle callbacks.
- Tables support drag-range, row and column selection, clipboard shortcuts and
  Escape to clear selection. Go tables support cells, double clicks and system themes.
- Text input stays active while focused and reopens when fields are tapped.
  Fields pan horizontally and can retain focus per input. Immediate fields,
  Material outlines, contrast and themed colors improve editing.
- Go compilation resolves local and cross-file helpers, direct extern calls,
  character buffers and fixed-buffer arguments. `#ui` hierarchy syntax and
  retained entries lower correctly; Go themes and long-text rendering match native.
- Added pane-tab wrappers. Android rotation synchronizes surface size and stops
  clipping UI. Canvas fixes guide clipping and uses top-origin font metrics.
- Removed unused Clay. Image decoder symbols remain private, and hosted/native
  builds avoid conflicting platform sources and stale intermediates.
- Go gains native X11 windows with Xauthority, focus, keyboard mapping and clear
  fallback errors. Native rendering supports registered fonts, themed surfaces,
  toolbar menus, workbook icons, screenshots and window compatibility helpers.
- Public APIs use consistent names for trees, navigation, profiles, menus,
  accelerators, geometry, canvas, syntax, modal actions, paragraphs, text, buttons,
  images and overlays. Internal draw functions and legacy widget, frame and
  generated-runtime aliases are hidden or removed, including the old cgo bridge.
- Go has a native host, frame renderer, direct widgets and layout/input helpers.
  Generated calls use the supported runtime, text editing and long labels remain
  stable, and per-field state is pruned when unused. Hosts preserve click behavior.
- Cross-platform links use OpenURI, including web and Android. Android disables
  single-instance locks; desktop apps gain scroll visibility and window fixes.
- Read-only text stays selectable without entering edit mode. TextField state
  and X11 input ungrabbing are corrected. Screenshot PNG buffers handle full rows.
- Added system locale detection, localized theme-picker fallbacks, Windows tray
  menus and registry-based theme detection. Foreground widgets own the cursor
  over disabled background controls.
- Terminal panes gain glyph-grid rendering, search/navigation, row reflow,
  scroll indicators, profiles and session records. Shared helpers encode keys,
  mouse and paste input, filter input, parse sixel and OSC commands, track modes
  and format terminal capability, cursor and status replies.
- Terminal clipboard helpers support selection, primary selection, OSC52 and
  paste. Titles and hyperlinks are sanitized; profile, theme, palette, selection
  and status colors resolve consistently. Legacy terminal aliases are removed.
- Prebuilt libraries check ABI versions. Theme refreshes are rate-limited and
  reuse unchanged Material schemes; tracing settings resolve once per process.
- Go exposes vsync. TextArea caches layout and heights, stops painting outside
  its viewport and supports shared clipboard buffers.
- Desktop apps use the system style and UI font by default. Aero is removed;
  styled cover images follow the selected UI style. Default bottom-nav icons
  retain contrast and Material tabs have corrected spacing.
- Menu bars draw above content and switch correctly between open menus; popup
  activation closes them correctly. Runtime downloads and text decoding are fixed.
  X11 windows can remain sticky across workspaces.
- Canvas applies cursors to page targets and fixes Guide rendering. WebAudio
  supports sound and music controls; file save, directory and filename helpers,
  focus queries and audio/image availability APIs are exposed.
- Added reusable terminal panes, app runtime and Linux desktop-integration
  helpers. Installed metadata names match, and Android notification priority works.
- Window drags start at the press, capture the pointer, follow global movement
  and apply the final release motion. SDL close requests and direct close polling work.
- Self-update helpers check, download, verify and apply updates, with an
  embeddable lifecycle and Go bindings. AppImage updates select the architecture.
  GTK tray loading ignores handshake activations and detects embedding correctly.
- Canvas fixes failed font loads and supplies frame diagnostics. Default locale
  detection and localized labels are exposed across hosts.
- Dropdowns accept slow or slightly moving clicks, dismiss on Escape/focus loss
  and open above the field when needed. Canvas draws rounded rectangles, rings,
  tinted images and clipboard mirrors. The Linux framebuffer cartridge backend
  is removed.
- The website gains a live examples gallery, author-linked app showcases,
  page navigation and runtime/widget/backend matrices. Showcase data and banners
  refresh consistently; Kry language docs and benchmarks are consolidated.

## v0.1.20 - 2026-08-18

- HTML5 Canvas2D rendering is available without entering native builds.
  Go reaches runtime API parity.

## v0.1.19 - 2026-08-18

- Screen-only apps receive runnable C entries; Canvas stays out of native builds.
  Screenshots share a PNG writer and optional diagnostics. Docs gain a permanent
  HTML renderer and clearer compiler/widget naming.

## v0.1.18 - 2026-08-18

- Audio-free builds omit cartridge audio and optional compatibility wrappers.
- Go supports typed declarations, arrays, goto/labels and the full widget set.
- Cartridges can use the Linux framebuffer and Dropdown controls. Software
  rendering tracks dirty regions; native screenshots write correct PNG checksums.

## v0.1.17 - 2026-08-18

- Docs: add FEATURE_MATRIX.md mapping widgets and features across targets and backends
- Ci: drop the renderer KRY_SW_SRCS overrides

## v0.1.16 - 2026-08-18

- Software rendering decodes PNG textures; cartridge tests link the PNG helper.
  Trays can change emblems and send actionable notifications. Secondary windows
  have build and test coverage.

## v0.1.15 - 2026-08-18

- Ci: run workflow actions on Node 24 runtimes

## v0.1.14 - 2026-08-18

- Retained frames work without a graphics context. Double clicks select all
  text, focus traverses retained controls and font atlases stay stable while typing.
- Secondary windows keep correct orientation and synchronized textures without
  readback; SDL uses shared GL contexts, with unsafe Linux paths disabled.
- Apps default to one instance and event-driven frame pacing. Static archives
  are indexed reliably. Go supports window flags, minimum sizes, host bridges,
  enums, switch statements and complete loop headers.

## v0.1.13 - 2026-08-18

- Web tools build the current cartridge compiler and shared KIR parser.
  macOS renderer builds use the required toolchain without recursive builds
  or inherited Make settings; case-sensitive file naming avoids collisions.
- Tray icons resolve absolute and themed paths on GNOME without fixed buffers.
- Retained text editing and mouse/keyboard selection work across lowerings;
  font atlases rebuild only between frames. Added Wayland auxiliary windows.
- C generation preserves anonymous enums and supports legacy declarations.

## v0.1.12 - 2026-08-17

- Fix AppIndicator icon lookup on Wayland
- Ui: retain selected font for deferred text
- Go: expose retained declarative UI
- Ui: add retained declarative tree and typed events

## v0.1.11 - 2026-08-17

- Ui: make text rendering event efficient

## v0.1.10 - 2026-08-17

- Ui: add fixed-subset font sources

## v0.1.9 - 2026-08-17

- Registered fonts retain their type names and cache more sizes. Mixed UI,
  emoji and icon fonts stay responsive without repeatedly scanning whole atlases.
- Text uses exact-size rasters; idle windows sleep until input. GTK themes can
  be sampled from CSS before initialization, and missing DPI queries log once.

## v0.1.8 - 2026-08-17

- FreeBSD builds resolve Go, curl, GTK, notification and ports dependencies;
  Go supports arm64 native linking. Release tooling automates versions and
  publishes all command-line tools.
- Go gains a reusable grid editor, stateful controls, collection/dialog widgets,
  secure password fields, text-area focus, theme-style selection, window
  activation and Raylib drawing helpers. Editor state remains valid across calls.
- Deferred overlays finalize at frame end. Multiline selection survives
  scrolling, line selection persists, tables use neutral colors and pictures
  are exposed to callers.
- Cartridges gain registered storage/audio capabilities and state initializers
  that render without a custom host. Added profile, habits, statistics, practice,
  manual and settings screen examples.
- Cartridge text uses antialiased Noto atlases with native font metrics and DPI
  scaling. Dropdown shapes, text placement and software output match the native
  reference, with cross-engine visual comparison tools.
- Screenshots capture the displayed frame correctly, support software GL and
  ES2, write valid PNGs and preserve absolute output paths.
- A shared KIR frontend drives Kry-to-C, Go and cartridge compilation. It handles
  multiline arrays and globals, comments, typed declarations, mutations,
  conditional regions, string continuations and app metadata.
- Cartridges support animated geometry, screen navigation, dropdowns, text
  input, scrolling, range controls, checkboxes and toggles. Bound state and guard
  expressions, including arithmetic and else branches, update within the cartridge.
- Referenced PNG, JPEG, BMP and WebP images embed as pixels. Circle and ring
  geometry, float fields and formatted values extend the cartridge format.
- Web and SDL players run cartridges; browser players open any KRB. A software
  rasterizer supports alternative hosts and headless comparisons. Preview tools
  render Kry or KRB to PNG without a full application host.
- Default controls use subtle fixed-radius rounding instead of full pills.
  Font sources resolve embedded assets before filesystem paths, and saved frame
  state preserves UI scale.
- Scenes support live JSON inspection with quick restarts, reusable removed-node
  slots and application-defined node kinds with property callbacks. Public scene,
  property, signal and transform APIs drop redundant prefixes.
- Added Node2D, Camera2D, Sprite2D, TileMap and AudioSource, keyframe animation,
  AnimationPlayer and AnimatedSprite2D. Box2D physics is optional; physics-free
  builds link correctly. Scene properties, signals and language syntax are supported.
  The UI Sprite widget becomes Image.
- C generation fixes one-line blocks, character literals, local names, switch
  cases, typed declarations, private functions, function references, array
  parameters and module aliases. Long initializers no longer truncate names.
- Imports preserve header forms and visibility. Named/anonymous enums, structs,
  typedefs, externs, globals and C-style parameters emit correctly and in usable
  order. Cross-module and local calls resolve without rewriting member calls.
- KIR joins multiline expressions and compound literals correctly, handles
  preprocessor branches and nested enum blocks, and skips inactive imports.
  The shared pipeline replaces the legacy compiler; `kc` becomes `k2c`.
- Compiler app-host output includes project headers and runnable entries.
  Build tools use canonical platform paths and stable targets. Static libraries
  and build outputs are platform-specific; stale Raylib objects no longer cause
  symbol collisions.
- System-theme defaults, GTK CSS palette reading and browser dark-mode detection
  improve theme selection. GTK trays load lazily, and font/memory diagnostics
  help investigate heavy apps.
- Added PickerDialog, wrapping Spinbox values, custom Slider labels and public
  ButtonRow sizing. Android Back closes Guide overlays. Theme switches fit their
  labels, and theme-picker labels remain valid.
- Extra native windows use ordinary widgets, with positioning, drag, click and
  right-click queries; unsupported platforms provide safe stubs.
- Terminal supports PTYs and feeding external output. The synthetic filesystem
  exposes the live UI engine and input injection for tooling.
- HTTP supports streaming partial reads. Shared JSON/HTTP helpers and
  cross-platform notifications become available; vendor-specific AI transport
  moves out of Kryon. Browser builds work without curl.
- Browser presentation can use a 2D canvas on WebKitGTK/software GL, creates its
  graphics context before hiding the source canvas and offers activation diagnostics.
  Web DPI stays in CSS pixels.
- Ksync improves crypto, JSON, deletion and transport, provides default transport
  helpers and a local development backend, and replaces the Lyra API names.
- Reusable Column/Row nodes provide flex-style positioning. Measurement lives on
  declarative widget nodes and uses shared widget operations.
- DPI uses the best available estimate and updates with density changes.
  Titlebars, toasts, centered text and dropdown resize use consistent scaling;
  theme spacing and pane-tab hit testing are corrected.
- Default and selectable native styles share platform defaults. Material toggles,
  buttons and classic focus are corrected, icons preserve color, and tree labels
  and syntax colors suit the theme. GTK supplies native file dialogs.
- Text context menus render above content. Focus stays persistent and exclusive
  across fields and text areas; cursor ownership follows UI intent. Keyboard
  shortcuts respect layout, bulk paste works and tree input capture is corrected.
- Font initialization is controllable, subsetting is shared and fallback fonts
  load dynamically without changing typefaces for already covered glyphs.
- The bundled C IDE is removed in favor of a standalone IDE. Its start page,
  recent projects, preview inspection, toolbar, source selection and clipboard
  fallback improve; builds expose a nonblocking console and problem panels.
  The `kt` runner supports project scenarios and visual targets.
- Compiler diagnostics, error recovery, expression typing and `defer` improve.
  Deferred cleanup stays within its switch case, and widget calls in conditions
  retain click-to-source inspection.
- Windows API names and thread helper declarations no longer collide or fail
  to build. Android notifications and web stubs use matching signatures.
- The website gains a browser IDE, readable text, clearer buttons, community
  links and current language samples, with corrected platform icons and CI links.

## v0.1.7 - 2026-07-26

- Kry removes redundant verbs and compiler widget-name rewriting; widgets
  are library APIs. TextField and TextArea share exclusive focus.
- The Platforms grid adds Windows, macOS and FreeBSD.

## v0.1.6 - 2026-07-25

- Tray icons resolve installed hicolor paths. Site cards, release builds,
  recent projects and IDE preview inspection, toolbar and text state are corrected.
  Docs gain a modern retro theme and the IDE gains a start page.
- Kry supports uninitialized and typed locals, argument locals, local/named
  enums, structs, switches, while/break, bitwise assignments, multidimensional
  arrays and anonymous scopes. App locals remain available in functions.
- Multiline functions, state initializers, block headers and expressions work,
  including leading/operator continuations; postfix operators stay together.
  Function and type capacity limits are raised.
- Modules use explicit paths, can contain only types and generate project headers.
  C exports, extern declarations, aliases, defines and compile-time macros are
  supported; recursive expansion is guarded.
- Function values resolve in expressions without rewriting locals incorrectly.
  Text shorthand, nil initializers and continuation parsing are corrected.
- Added platform threads, runtime asset synchronization and web intrinsics.

## v0.1.5 - 2026-07-21

- Add the Kryon logo as the site and IDE app icon.
- Add C-close Kry language direction docs.
- Add automatic release tagging when the checked-in Kryon version changes.

- Replace Kry `include` syntax with explicit `cimport` for C headers.
- Use cleaner changelog version headings without brackets.

- Allocate Kry compiler state on the heap to avoid Linux runner stack crashes.

## v0.1.4 - 2026-07-18

- Add Clay as a vendored layout dependency for future Kryon UI work.
- Remove the vendored fontchop dependency and chopped bitmap font assets.

## v0.1.3 - 2026-07-16

- Add public Kryon version metadata for release artifacts.
- Move font atlas generation to the vendored fontchop submodule.
- Replace the single implicit UI font with a registered font selection API.
- Improve generated icon asset formatting and desktop/embedded fallbacks.

- Add Markdown support and text area UI.
- Add pointer release helper APIs and modal layer input capture.

## v0.1.2 - 2026-07-12

- Add web file dialog loading.

## v0.1.1 - 2026-07-12

- Fix release CI vendor builds.

## v0.1.0 - 2026-07-12

- Add Kryon release automation.
