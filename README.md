# Kryon

Documentation for Kryon 0.4.0 and Unreleased changes on master.

Linux desktop and portable hosts can decode local videos with synchronized audio
using the optional GStreamer service exported as `kryon/Video` and
`kryon/VideoLinux`. Display the returned frame with `Image.Draw(Image.Props)`.
Applications own playback controls and access checks; an optional expiring lease
revokes both audio and decoded frames when authorization is no longer renewed.
`make test-focus TEST=ziran_video_linux_test` checks synthetic moving frames, paused seeking, speed, volume,
replay and lease revocation with a silent audio sink and no display access.

Kryon is a UI library written in Ziran. Its source lives under `src/ui/` and
imports Ziran modules normally. The Ziran compiler, `.zir` representation, and
`.zx` bundle format belong to the separate
[Ziran](https://github.com/ziranlang/ziran) repository.

## Build

Install the `ziran` command first (`make install-user` in a
[Ziran](https://github.com/ziranlang/ziran) checkout), then build Kryon:

```sh
make
make test
```

The Makefile builds against the Ziran revision locked in `ziran.lock`, found
with `ziran pkg path ziran`. To build against a local Ziran checkout, name it
in an ignored `ziran.local.toml` `[overrides]` table or set `ZIRAN_ROOT`.

`make` checks the modules listed in [src/ui/modules.txt](src/ui/modules.txt),
writes checked `.zir`, generates C and Go, and compiles the native outputs
from Ziran source.
The current C archive is `build/ziran/libkryon.a`; C headers are generated
from Ziran declarations into `build/ziran/c/`. Kryon has no handwritten
source headers. Hosts provide declared
effects such as raster lines, and `CompositionQueue` carries raw IME events to
the checked text widgets.

Charts, tables, trees, and source coloring are separate Git packages that
depend on Kryon: [Plot](https://github.com/kryonlabs/plot),
[DataViews](https://github.com/kryonlabs/data-views) (`TableView` and
`TreeView`), and [Syntax](https://github.com/ziranlang/syntax). Add one with
`ziran add URL` and import it as `plot/Plot`, `data_views/TableView`, or
`syntax/Syntax`. Kryon exports the building blocks such packages draw with
(`session`, `tree`, `surface`, `paint_queue`, `style`, and the others listed
in `ziran.toml`); see [writing a widget package](docs/API.md#widget-packages).

Style sheets are the separate [CSS](https://github.com/kryonlabs/css)
package: it parses and formats a subset of CSS, installs the rules. Kryon keeps the rule table and
style resolution, so an app that builds its rules in code needs no parser.

Kryon exports `geometry` and `drawing_props` for programs that need drawing
values without importing widgets. Imports and use determine what a program
carries. `App.Show(window)` carries only the window, events and presentation;
`App.Run(window, Draw)` also brings the frame loop. Portable ZX windows run
through the native desktop provider by default. See the [API](docs/API.md).

`make test` builds the test host and runs the Ziran source, saved-IR, and
portable bundle tests with four concurrent jobs. Set `TEST_JOBS=1` to run them
serially or `TEST_JOBS=8` on a larger machine. For a quick edit loop, use
`make test-focus TEST=link_widget`; the filter matches test script names and
skips the full library rebuild. Test subprocesses have no display access.
`make sanitize-test` runs the behavior tests with AddressSanitizer and
UndefinedBehaviorSanitizer. Keep one-off experiments in `build/scratch/`;
`make clean-scratch` removes that and any other `build/` entry that no target
writes, once it has been idle for a day.
See [composition input](docs/COMPOSITION_INPUT.md) for the checked IME event
contract and host queue lifetime.
See [cursor input](docs/CURSOR.md) for the checked cursor decision and platform
effect contract.
See [theming](docs/THEMING.md) for palette and style customization.
See [zoom](docs/ZOOM.md) for Ctrl and mouse-wheel zoom of a whole application.
See [clipboard state](docs/CLIPBOARD.md) for the portable clipboard value and
host effect contract.
See [frame replay](docs/FRAME_REPLAY.md) for committed UI and paint capture
with scripted input on a headless host.
The [hello world example](examples/README.md) is one small Ziran program with
an explicit `main`: it imports Kryon's app and text packages, opens a window,
and draws the label `Hello, world!`.
For Ziran app manifests, the terminal backend, and `kryon run`, see
[Ziran projects](docs/PROJECTS.md).
Portable desktop profiles build a `.zx` with embedded assets; a downloaded
file launches with `ziran run app.zx` after installing the desktop provider. See [Zx applications](docs/PROJECTS.md#zx-applications).

## Website and widget gallery

`make site` packages the Canvas website, browser compiler, preview host and
widget gallery into `docs/site/`. Emscripten is required; activate
emsdk or set `EMCC`. `make site-gallery` builds only the gallery, and
`make site-gallery-test` prepares editable examples and exercises the gallery in
a private Chromium/Xvfb session.

The gallery at `widgets.html` uses the example package in `examples/gallery/`.
Its core scenes are shared by the desktop, web and review hosts. Examples are
grouped by installed module and category, including core UI and 2D drawing.
Optional package integrations are registered only while their dependencies
are declared. Each selected example includes editable Ziran source and a live
preview, using the browser compiler and the gallery editor. Plot and DataViews
supply charts, tree, Kanban and an interactive TableView. `make site` generates the scene metadata and editable
snippets directly from these examples. The Pages workflow builds and tests the
full website from fixed source revisions before publishing.

## Current status

The [feature matrix](docs/FEATURE_MATRIX.md) records package and host coverage.
The Canvas2D browser host is covered by private headless Chromium, WebAudio,
and Wasm tests. See [accessibility](docs/ACCESSIBILITY.md) for platform coverage.

Kryon commits land upstream first. Applications then update their
`ziran.lock` with Ziran's package commands. This repository's tests
verify Kryon itself, not every downstream application build.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the library boundary and
[Ziran's implementation
status](https://github.com/ziranlang/ziran/blob/master/docs/IMPLEMENTATION_STATUS.md)
for language and portable runtime gaps.

`kryon/breadcrumb` serves `Breadcrumb.Draw` for keyed label navigation, with measured
ancestors and an accessible, virtualized overflow menu. Applications own
selection, focus and navigation; see [the API](docs/API.md#breadcrumbs).

## Documentation

- [API reference and widget packages](docs/API.md)
- [Architecture](docs/ARCHITECTURE.md) and [repository boundaries](docs/BOUNDARIES.md)
- [Projects](docs/PROJECTS.md), [hosts](docs/BACKENDS.md) and [Canvas](docs/canvas.md)
- [Feature matrix](docs/FEATURE_MATRIX.md) and [widget verification](docs/WIDGET_CONFORMANCE.md)
- [Input components](docs/INPUT_COMPONENTS.md), [overlays](docs/OVERLAYS.md) and [compact navigation](docs/navigation.md)
- [Component loading](docs/COMPONENTS.md), [window focus](docs/window-focus.md) and [accessibility](docs/ACCESSIBILITY.md)
- [Raster workers](docs/THREADING.md) and [performance measurements](docs/PERFORMANCE.md)
