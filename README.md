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
source headers. `build/ziran/libkryon_host.a` contains C fixtures used only by
the portable tests; it is not a platform library. Hosts provide declared
effects such as raster lines, and `CompositionQueue` carries raw IME events to
the checked text widgets.

Plot lives in the optional `src/plot/` package. `make plot` builds
`build/ziran/libkryon_plot.a`; apps importing `plot_widget` add
`src/plot/` to their Ziran module paths. The `kryon` project command adds
this path automatically.

TableView and TreeView live in the optional `src/data_views/` package.
`make data-views` builds `build/ziran/libkryon_data_views.a`; apps add that
directory to their Ziran module paths when importing either widget. The
`kryon` project command adds the path automatically.

KSS parsing, formatting, and installation live in the optional `src/kss/`
package. `make kss` builds `build/ziran/libkryon_kss.a`; the core UI library
keeps style resolution but does not import the parser. Ziran projects add the
KSS module path automatically.

The optional `src/syntax/` package tokenizes Ziran, C, and Make text into
caller owned color spans for TextArea. `make syntax` builds
`build/ziran/libkryon_syntax.a`; core TextArea paints spans without importing
the tokenizer. Ziran projects add this module path automatically.

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
See [composition input](docs/COMPOSITION_INPUT.md) for the checked IME event
contract and host queue lifetime.
See [cursor input](docs/CURSOR.md) for the checked cursor decision and platform
effect contract.
See [style picker](docs/STYLE_PICKER.md) for caller-owned pack selection.
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
The [open-work plan](plan/README.md) lists unfinished migration tasks,
including the C portable test fixtures. Kryon does not currently offer a web
backend; see [web host status](docs/WEB_STATUS.md).

Downstream applications migrate their own sources and platform hosts at their
own pace. Kryon commits land upstream first; apps then move only their clean
`vendor/kryon` submodule pointer. This repository's tests verify Kryon itself,
not every downstream application build.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the library
boundary and [Ziran's implementation status](https://github.com/ziranlang/ziran/blob/master/docs/IMPLEMENTATION_STATUS.md)
for language and portable runtime gaps.
