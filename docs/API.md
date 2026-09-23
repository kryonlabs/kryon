# Kryon API

Kryon is a collection of ordinary Ziran modules under `src/ui/`. An
application imports only the modules it needs. The modules listed in
[`modules.txt`](../src/ui/modules.txt) currently pass Kryon's C, C++, and Go
build; other `.zi` files are moved source awaiting conversion.

## Use a checked module

Pass the Kryon source directory as an ordinary module path:

```sh
../ziran/build/bin/ziran check --root app \
    --module-path ../kryon/src/ui app/main.zi
```

For example, a non-graphical program can import layout policy without
initializing a window:

```zi
#module "main"
#import "scroll"

Answer :: () -> i32 #export {
    return ScrollClamp(120, 80)
}
```

The same module can be built into C, C++, or Go, serialized as `.zir`, or
linked into a `.zib`. Ziran owns those commands and formats. Kryon has no
compiler or bundle loader. A `.zib` links Kryon code only when imported by the
program; effects that need a platform host must be supplied by declared host
interfaces.

## Current surfaces

The checked modules cover geometry, layout, styling, themes, accessibility,
focus, input, scroll, canvas transforms, popup policy, and several widget
measurement and paint decisions. [`modules.txt`](../src/ui/modules.txt) is the
precise build inventory. [`ziran_*_test.sh`](../tests/ziran_moved_modules_test.sh)
files show source, saved-IR, and bundle use of those modules.

`build/ziran/libkryon.a` is the current C archive; generated headers are in
`build/ziran/c/`. Portable host bindings are declared in
`include/kryon_portable_host.h` and built into `build/ziran/libkryon_host.a`.
`FramePacingBinding()` uses the platform's `SetTargetFPS`;
`RasterLineBinding()` accepts a line renderer for Bevel and Separator lines.
`RasterRoundedRectangleBinding()`,
`RasterRoundedRectangleOutlineBinding()`, and `RasterTextBinding()` accept
shape and UTF-8 text renderers for the checked Progress paint path. Shape and
text effects live in separate modules, so a line-only Go host needs only the
line interface.
`MeasureGlyphWidthBinding()` and `MeasureGlyphLineHeightBinding()` accept
font metric callbacks for the checked Progress composition path. Both callbacks
receive the requested typeface as borrowed UTF-8 bytes.
The caller passes required bindings to Ziran's `BundleRun`. Build and test
with `make` and `make test` from the Kryon repository. No display is started.

## Migration boundary

This library currently covers selected widget policy and line rendering.
Progress layout, fill and border visibility, radius, and label color decisions
are checked Ziran functions. `PaintProgress()` emits its track, fill, outline,
and label through declared raster effects. `PrepareProgress()` selects the font
and uses host glyph measurements; `PaintProgressProps()` combines that with
the raster path. Callers currently supply three resolved `ProgressFaces`.
Style sheet lookup by class and the complete one-argument `Progress` widget
are still being migrated.
`ImageProps` now carries portable strings. Image fit, source selection, draw
eligibility, and default tint decisions are checked Ziran functions. Text fields
in `TextProps`, `LinkProps`, `ToastProps`, `SeparatorProps`, `RadioProps`,
`FieldsetProps`, `ProgressProps`, and `RouterRoute` now use Ziran strings. Other
props still contain pointer-based state or collections. Full `Text(TextProps)`,
`Image(ImageProps)`,
widget composition, broader platform rendering and host linking, and downstream
application integration are unfinished. Modules outside `modules.txt` may
still contain syntax or imports from the retired implementation and are not
supported API yet. See [architecture](ARCHITECTURE.md) and
[boundaries](BOUNDARIES.md) for the intended ownership and completion gates.
