# Kryon

Kryon is a UI library written in Ziran. Its source lives under `src/ui/` and
imports Ziran modules normally. The Ziran compiler, `.zir` representation, and
`.zib` bundle format belong to the separate `../ziran` repository. Kryon has no
`.kry` runtime or language compiler.

## Build

With the Ziran repository next to this one:

```sh
make
make test
```

`make` checks the modules listed in [src/ui/modules.txt](src/ui/modules.txt),
writes checked `.zir`, generates C, C++, and Go, and compiles the native outputs.
The current C archive is `build/ziran/libkryon.a`; generated headers are in
`build/ziran/c/`. The first portable platform binding is in
`build/ziran/libkryon_host.a` and maps `frame_pacing:ApplyTargetFPS` to the
selected backend's `SetTargetFPS`. `make test` also runs the Ziran source,
saved-IR, and portable bundle tests. No display is started.

## Migration status

The checked library currently covers geometry, layout, accessibility, focus,
input and text input policy, canvas transforms, scroll, menu, color picker,
Button, Checkbox, Slider, Toggle, material layers, theme, style values, and
selected Image, Separator, Progress, and window placement decisions. Other runtime and widget
source has been moved into `.zi` files in `src/ui/`; those modules still need
type, host interface, and backend work before they can join `modules.txt`.

The current archive is a policy subset. Complete widget composition, rendering,
remaining platform host adapters, full `.zib` capability execution, and
downstream app builds are still migration work. The old C and Go host code is
retained only as platform implementation material and is not part of the default
build.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the intended library
boundary and [Ziran's implementation status](../ziran/docs/IMPLEMENTATION_STATUS.md)
for language and portable runtime gaps.
