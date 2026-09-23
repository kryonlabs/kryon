# Kryon

Kryon is a UI library written in Ziran. Its source lives under `src/ui/` and
imports Ziran modules normally. The Ziran compiler, `.zir` representation, and
`.zib` bundle format belong to the separate
[Ziran](https://github.com/kryonlabs/ziran) repository. Kryon has no
`.kry` runtime or language compiler.

## Build

For a fresh clone, place Ziran next to Kryon:

```sh
git clone https://github.com/kryonlabs/ziran.git ../ziran
make
make test
```

`make` checks the modules listed in [src/ui/modules.txt](src/ui/modules.txt),
writes checked `.zir`, generates C, C++, and Go, and compiles the native outputs.
The current C archive is `build/ziran/libkryon.a`; generated headers are in
`build/ziran/c/`. Portable platform bindings are in
`build/ziran/libkryon_host.a`: frame pacing maps to the selected backend's
`SetTargetFPS`, shared raster lines use a caller-supplied line renderer, and
`CompositionQueue` carries raw IME events to the checked text widgets.
`make test` also runs the Ziran source,
saved-IR, and portable bundle tests. No display is started.
See [composition input](docs/COMPOSITION_INPUT.md) for the checked IME event
contract and host queue lifetime.
See [cursor input](docs/CURSOR.md) for the checked cursor decision and platform
effect contract.
See [style picker](docs/STYLE_PICKER.md) for caller-owned pack selection.
See [clipboard state](docs/CLIPBOARD.md) for the portable clipboard value and
host effect contract.

## Migration status

The checked library currently covers geometry, layout, accessibility, focus,
input and text input policy, portable clipboard state, text-buffer edits, and UTF-8 cursor
boundaries, checked TextField and TextArea composition with caller-owned text
and edit and clipboard intents, checked composition event decisions and
TextField/TextArea preedit paint, Ziran/C/Make syntax coloring, canvas
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
checked PanedView composition and handle dragging with returned pane bounds,
checked Toast lifetime, text truncation, layout, and paint,
checked Collapsible header interaction and tree keyboard navigation,
checked SegmentedControl layout, selection, and styled Button children,
portable theme and orientation preference decisions,
form row layout, app shell sizing,
capability policy, safe area geometry, and window placement decisions. Other runtime and widget
source has been moved into `.zi` files in `src/ui/`; those modules still need
type, host interface, and backend work before they can join `modules.txt`.

The current archive includes selected widget behavior and shared line rendering
path. Complete widget composition and rendering,
remaining platform host adapters, full `.zib` capability execution, and
downstream app builds are still migration work. The old C and Go host code is
retained only as platform implementation material and is not part of the default
build.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the intended library
boundary and [Ziran's implementation status](https://github.com/kryonlabs/ziran/blob/master/docs/IMPLEMENTATION_STATUS.md)
for language and portable runtime gaps.
