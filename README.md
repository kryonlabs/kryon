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
`SetTargetFPS`, and shared raster lines use a caller-supplied line renderer.
`make test` also runs the Ziran source,
saved-IR, and portable bundle tests. No display is started.

## Migration status

The checked library currently covers geometry, layout, accessibility, focus,
input and text input policy, canvas transforms, scroll, menu, color picker,
Button, Checkbox, Slider, Toggle, Bevel and Separator line rendering, material
layers, theme, style values, built-in theme labels, and selected Image, Progress,
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
