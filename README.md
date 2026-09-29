# Kryon

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

KSS parsing, formatting, and installation live in the optional `src/kss/`
package. `make kss` builds `build/ziran/libkryon_kss.a`; the core UI library
keeps style resolution but does not import the parser. Ziran projects add the
KSS module path automatically.

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
`make test` also fuzzes KSS: `make fuzz-kss FUZZ_SEED=N FUZZ_COUNT=M` runs
other sheets, and a failing sheet is kept in `build/kss-fuzz/` with a command
that shrinks it (see [tests/kss_fuzz.zi](tests/kss_fuzz.zi)).
`make sanitize-test` runs the behavior tests with AddressSanitizer and
UndefinedBehaviorSanitizer. After editing `styles/kryon/classic.kss`, run
`make style-packs` to regenerate its embedded module; `make test` fails while
they differ. Keep one-off experiments in `build/scratch/`;
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
