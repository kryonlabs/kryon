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
the raster path. `StyleRules` is an owned 320-rule value table; the checked
`ProgressFacesFor()` resolves track, fill, and label roles by class and cascade
priority. `InstallStyleRules()` installs the owned table, and the checked
one-argument `Progress(props)` paints from that table with Ziran-defined
default faces. Lower-level `PaintProgressFromRules()` still accepts explicit
defaults and rules. The pointer-backed style loader remains unfinished.
`BeginTree(screen_key, bounds)` begins a portable retained submission, and
`EndTree()` commits and paints it. During an open submission,
`Progress(props)` registers a Progress node; raster effects are emitted only
after the tree commits. Outside a tree, `Progress(props)` paints immediately.
`ProgressProps.key` and `SeparatorProps.key` provide stable identity; zero
uses the submission
position. `TreeCount()` and `TreeNodeAt(index)` inspect committed nodes.
Identity survives repeated `BundleInstanceRun()` calls, including bounds
changes. Submission is limited to 1024 nodes; `EndTree()` returns false,
preserves the previous tree, and emits no paint effects if that limit is
exceeded. Progress and Separator have portable retained paint paths so far;
retained layout and input routing remain unfinished. Because `EndTree()`
currently dispatches both widget painters, a portable host binding a retained
tree must provide the line, rounded shape, text, and glyph metric effects used
by those painters even when a frame uses only one widget type.
`Separator(props)` resolves Line and Label KSS roles in checked Ziran. It
positions an unlabeled vertical or horizontal line, or measures and paints a
label followed by a line. It paints immediately outside a tree and submits a
node for deferred paint inside one.
`BeginStyleRules()` and `ParseStyleRules()` collect checked KSS parser output
into that table. Parsing stops at `NeedImport` so the caller can use
`ProvideStyleRulesImport()` or `FailStyleRulesImport()` before continuing.
`StyleRulesParse.overflow` reports when a 321st rule would exceed the table.
Portable hosts that import the KSS parser bind `KssStringSliceBinding()` for
source byte ranges. A portable bundle's installed rules last for one
`BundleRun`; another run begins with an empty rule table. A
`BundleInstance` keeps those rules between runs, allowing a frame entry to
install a style sheet once and paint subsequent frames from it.
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
