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
from Ziran source. It does not build the handwritten C test host.
The current C archive is `build/ziran/libkryon.a`; C headers are generated
from Ziran declarations into `build/ziran/c/`. Kryon has no handwritten
source headers. Portable platform bindings are in
`build/ziran/libkryon_host.a`: shared raster lines use a caller-supplied line
renderer, and `CompositionQueue` carries raw IME events to the checked text
widgets.

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

## Migration status

The checked library currently covers geometry, layout, accessibility, focus,
input and text input policy, portable clipboard state, text-buffer edits, and UTF-8 cursor
boundaries, checked TextField and TextArea composition with caller-owned text
and edit and clipboard intents, checked composition event decisions and
TextField/TextArea preedit paint, caller supplied TextArea color spans, canvas
transforms, cursor shape and priority decisions, scroll, menu, color picker,
and a caller-owned style picker composed from the checked Dropdown,
value based swipe gesture state and pointer ownership effects,
Button, Checkbox, Slider, Toggle, Dropdown, Toolbar, TitleBar, NavigationBar,
checked TabBar composition, keyboard selection, scrolling, close actions,
middle and double click, and reorder intents,
checked NavigationBar configuration editor composition, route editing, and
keyboard selection,
Bevel and Separator line rendering, material
layers, theme, style values, built-in theme labels, and selected Image, Progress,
checked ModalFrame layout, backdrop dismissal, title and close actions,
checked ActionModal message layout, wrapped buttons, and dismissal,
checked TreeView composition and row input from portable item values,
checked Scroll child scopes with caller owned scroll values and viewport clips,
checked PanedView composition and handle dragging with returned pane bounds,
checked Toast lifetime, text truncation, layout, and paint,
checked Collapsible header interaction and tree keyboard navigation,
checked SegmentedControl layout, selection, and styled Button children,
portable theme and orientation preference decisions,
form row layout, app shell sizing,
capability policy, safe area geometry, and window placement decisions.
Every retained UI module is listed in `modules.txt`; the header-dependent
legacy implementation has been deleted.

Kryon's implementation source is 100% current Ziran: every file under `src/`
is a checked `.zi` module or the module inventory, the source inventory
rejects any new handwritten implementation file, and `make test` verifies the
library through source, saved IR, and portable bundle tests on C, C++, Go,
and `.zib`. The C test hosts and the C frame replay tool are still used by
the present test harness and must be replaced in Ziran rather than extended.
The old KRB renderer and static C package were removed with their
header-dependent implementation, and the Android Java launch bridge now
belongs to the applications that need it.

Downstream cutover status: the verified Kryon HEAD builds through a
downstream app's vendored pointer (`vendor/kryon`) with its Ziran library
gate and module-path link tests, exactly as this repository's `make test`
does. Applications migrate their own sources and platform hosts at their own
pace; Kryon commits land upstream first and apps move only the clean
submodule pointer.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the intended library
boundary and [Ziran's implementation status](https://github.com/ziranlang/ziran/blob/master/docs/IMPLEMENTATION_STATUS.md)
for language and portable runtime gaps.
