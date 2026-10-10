# Kryon

Linux desktop and portable hosts can decode local videos with synchronized audio
using the optional GStreamer service exported as `kryon/Video` and
`kryon/VideoLinux`. Display the returned frame with `Image(ImageProps)`.
Applications own playback controls and access checks; an optional expiring lease
revokes both audio and decoded frames when authorization is no longer renewed.
`make video-test` checks synthetic moving frames, paused seeking, speed, volume,
replay and lease revocation with a silent audio sink and no display access.

Kryon is a UI library written in Ziran. Its source lives under `src/ui/` and
imports Ziran modules normally. The Ziran compiler, `.zir` representation, and
`.zib` bundle format belong to the separate
[Ziran](https://github.com/ziranlang/ziran) repository. Kryon has no
`.kry` runtime or language compiler.

## Build

For a fresh clone, place Ziran next to Kryon:

```sh
git clone https://github.com/ziranlang/ziran.git ../ziran
make
make test
```

`make` checks the modules listed in [src/ui/modules.txt](src/ui/modules.txt),
writes checked `.zir`, generates C, C++, and Go, and compiles the native outputs
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

Style sheets are the separate [KSS](https://github.com/kryonlabs/kss)
package: it parses and formats `.kss` text, installs the rules, and ships the
classic, lightfield, and material style packs. Kryon keeps the rule table and
style resolution, so an app that builds its rules in code needs no parser.

The optional [Game2D](https://github.com/kryonlabs/game2d) Ziran package owns
the raylib game API. Kryon keeps its geometry, drawing values, UI primitives,
and raylib UI host. Kryon exports `geometry` and `drawing_props` so Game2D can
share their types without pulling in widget modules. The raylib game adapter
uses a C ABI and does not support native Go.

`make test` builds the test host and runs the Ziran source, saved-IR, and
portable bundle tests with four concurrent jobs. Set `TEST_JOBS=1` to run them
serially or `TEST_JOBS=8` on a larger machine. For a quick edit loop, use
`make test-focus TEST=link_widget`; the filter matches test script names and
skips the full library rebuild. Test subprocesses have no display access.
`TAIJI_DIR=/path/to/taiji make pixmap-surface-plan9-test` also runs the pixel
oracle through native Plan 9 `8c`/`8l`, from source and saved IR, in a private
headless guest. Its build uses 512 MiB and allows 900 seconds; override those
with `KRYON_PLAN9_MEMORY` and `KRYON_PLAN9_TIMEOUT` when needed.
`TAIJI_DIR=/path/to/taiji make pixmap-measure-plan9-test` runs the text-unit
and width oracle through the same native guest. Both oracles also run from
source and saved IR on C, C++, Go, Rust, Python and portable bundles.
`make sanitize-test` runs the behavior tests with AddressSanitizer and
UndefinedBehaviorSanitizer. Keep one-off experiments in `build/scratch/`;
`make clean-scratch` removes that and any other `build/` entry that no target
writes, once it has been idle for a day.
See [composition input](docs/COMPOSITION_INPUT.md) for the checked IME event
contract and host queue lifetime.
See [cursor input](docs/CURSOR.md) for the checked cursor decision and platform
effect contract.
See [style picker](docs/STYLE_PICKER.md) for caller-owned pack selection.
See [zoom](docs/ZOOM.md) for Ctrl and mouse-wheel zoom of a whole application.
See [clipboard state](docs/CLIPBOARD.md) for the portable clipboard value and
host effect contract.
See [frame replay](docs/FRAME_REPLAY.md) for committed UI and paint capture
with scripted input on a headless host.
The [Ziran example](examples/README.md) builds a `.zib` that imports Kryon and
runs a retained `Button` press and release through a separate Ziran SVG host
without a display.
The same directory also has a plain `Text(TextProps)` hello world desktop
example; `make -C examples desktop-text-test` renders it on a private display.
For Ziran app manifests, the terminal backend, and `kryon run`, see
[Ziran projects](docs/PROJECTS.md).
Portable desktop profiles build a `.zib` with embedded assets; a downloaded
file launches with `kryon run app.zib`. See [Zib applications](docs/PROJECTS.md#zib-applications).

## Current status

The [feature matrix](docs/FEATURE_MATRIX.md) records package and host coverage.
The Canvas2D browser host is covered by private headless Chromium, WebAudio,
and Wasm tests. Its semantic DOM profile is covered by a separate private
headless Chromium document test. The [open-work plan](plan/README.md) lists
the remaining platform-accessibility work.

Downstream applications migrate their own sources and platform hosts at their
own pace. Kryon commits land upstream first; apps then move only their clean
`vendor/kryon` submodule pointer. This repository's tests verify Kryon itself,
not every downstream application build.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the library boundary and
[Ziran's implementation
status](https://github.com/ziranlang/ziran/blob/master/docs/IMPLEMENTATION_STATUS.md)
for language and portable runtime gaps.
