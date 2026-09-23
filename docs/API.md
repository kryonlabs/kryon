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
measurement and paint decisions, including `Text(TextProps)`.
The checked `Button(ButtonProps)` path now submits a retained interactive node,
resolves KSS class and state rules, measures a label and optional `ImageProps`
asset or texture, and queues shape, clipped image, and clipped text paint.
The image uses the same fit rules as `Image(ImageProps)`; `icon_placement`
selects its side of the label. It consumes an activation routed from the previous
committed tree and returns `1` for that frame. The host supplies raw pointer
samples, glyph and raster callbacks, and asset dimensions when an image path is
used; it does not implement Button state policy.
This path currently covers text and image Buttons. Glyph and raw texture icons,
menu, split,
loading animation, material layers, keyboard focus, and immediate input still
need migration. `ButtonProps` has a portable string label and no C pointers;
menu items and mutable menu state need a separate value-based surface.

The checked `Checkbox(CheckboxProps)` accepts a portable `checked` value and
returns `{checked, changed}` after a retained pointer activation. It records
the resulting selected state on its tree node, resolves KSS box and label
roles, and queues the box, check mark, and clipped label. Hosts supply raw
pointer samples, glyph metrics, and raster callbacks; callers keep the value
returned by Checkbox for the next frame. The old pointer and flag fields are
not part of this checked API. Keyboard focus, ancestor input clipping, and
native widget integration remain open.

[`modules.txt`](../src/ui/modules.txt) is the
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
`TextSliceBinding()` supplies borrowed byte ranges for checked `Text` line
composition; the host does not choose wrap points or positions.
`RasterTextClippedBinding()` supplies a glyph raster callback that must honor
the clip rectangle selected by `Text`, Button, and Checkbox. Some other widget
labels still use `RasterTextBinding()` without a clip.
The caller passes required bindings to Ziran's `BundleRun`. Build and test
with `make` and `make test` from the Kryon repository. No display is started.

For a headless RGBA8 target, `include/image_canvas.h` provides `ImageAsset`,
`ImageCanvas`, and `ImageCanvasRasterizer()`. The caller owns the straight-alpha
pixel buffers and asset table, then binds the resulting `ImageRasterizer` with
`ImageWidthBinding()`, `ImageHeightBinding()`, and `RasterImageBinding()`.
Assets can be found by path or texture ID. The software host applies source
crop, nearest-neighbor scaling, clip, rounded corners, rotation, and tint.
Link it with `libkryon_host.a` and `-lm`; the
[`ziran_image_canvas_test.sh`](../tests/ziran_image_canvas_test.sh) example runs
the same widget from source and saved `.zir` bundles into an in-memory canvas.

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
`Progress(props)` registers a Progress node and queues its generic paint
commands; raster effects are emitted only after the tree commits. Style
resolution and glyph measurement happen during submission. Outside a tree,
`Progress(props)` paints immediately.
`ButtonProps.key`, `ProgressProps.key`, and `SeparatorProps.key` provide stable identity; zero
uses the submission position. `TreeCount()` and `TreeNodeAt(index)` inspect
committed nodes.
Identity survives repeated `BundleInstanceRun()` calls, including bounds
changes. Submission is limited to 1024 nodes; `EndTree()` returns false,
preserves the previous tree, and emits no paint effects if that limit is
exceeded. The generic queue holds 4096 paint commands and likewise rejects an
overfull frame before commit. Progress, Separator, Text, and Image have portable
retained paint paths. `TreeSetInteractive()` marks an eligible submitted node;
after commit, `TreePointerUpdate(PointerFrame)` hit tests committed nodes in
reverse paint order, blocks click-through at disabled controls, and owns press
and release activation. `TreeHoveredAt()`,
`TreePressedAt()`, and `TreeTakeActivationAt()` expose that state. The host
supplies raw pointer samples. Button uses this path for retained pointer
activation; retained layout, keyboard focus, clipping of input by ancestors,
and broader interaction routing remain unfinished.
`EndTree()` links the generic line, rounded shape, and text raster effects;
the image raster effect is also linked for generic paint commands. Glyph
metric and asset dimension requirements follow the widgets the application
imports.
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
`Image(ImageProps)` is now a checked Ziran widget. It accepts an asset path or
a supplied texture handle, resolves class styles, chooses source and fit,
queues a clipped and optionally rounded image draw, and retains semantic alt
text on its node. The platform supplies asset dimensions and executes the raw
image raster command through declared host effects. A missing asset paints its
alt text or the fallback label. The headless software host covers RGBA output;
broader image materials, accessibility event publication, and concrete
desktop/browser image hosts still need migration.
`Text(TextProps)` is a checked Ziran widget. It resolves KSS text rules,
measures glyphs, wraps at word boundaries, positions lines with horizontal and
vertical alignment, clips glyphs and strikethrough to its bounds, and submits
semantic text to the retained tree. The host supplies byte slices, glyph
metrics, and raw clipped text rasterization. Letter spacing, parent foreground
inheritance, and selectable text interaction still need migration.
Text fields in `TextProps`, `LinkProps`, `ToastProps`, `SeparatorProps`, `RadioProps`,
`FieldsetProps`, `ProgressProps`, and `RouterRoute` now use Ziran strings. Other
props still contain pointer-based state or collections. Remaining widget
composition, broader platform rendering and host linking, and downstream
application integration are unfinished. Modules outside `modules.txt` may
still contain syntax or imports from the retired implementation and are not
supported API yet. See [architecture](ARCHITECTURE.md) and
[boundaries](BOUNDARIES.md) for the intended ownership and completion gates.
