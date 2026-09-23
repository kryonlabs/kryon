# Kryon architecture

Kryon is an ordinary Ziran library. Ziran owns the language, checker, `.zir`,
native backends, `.zib` linker, and portable runtime. Kryon owns widget APIs,
composition, layout, interaction, styling, accessibility, and rendering policy.
Applications import Kryon modules from `src/ui/`. Neither Ziran's compiler nor
its portable loader recognizes widget names.

## Source and host boundary

| Location | Responsibility |
| --- | --- |
| `src/ui/*.zi` | Kryon declarations and reusable UI behavior |
| `src/ui/modules.txt` | Modules that pass the current full native build |
| `src/backend/`, other C and Go host code | Raw platform input, windows, font measurement, rasterization, storage, and device services |
| `../ziran` | Language implementation and generic execution |

`modules.txt` is a temporary migration inventory. The old build discovered
every UI source file automatically; once every maintained `.zi` module builds,
the library build should discover those files directly and remove this list.

Host adapters provide observations and effects through declared interfaces.
Widget state, policy, layout, and draw decisions belong in `.zi`, including for
desktop, browser, and portable hosts. Platform adapters may use C or Go where
the operating system requires it; they must not duplicate widget policy.

## Current build

`make` compiles the [checked module list](../src/ui/modules.txt) to `.zir`, C,
C++, and Go, then builds `build/ziran/libkryon.a` and the initial portable host
adapter `build/ziran/libkryon_host.a`. `make test` runs the current
source, saved-IR, and portable bundle tests. The checked modules cover
geometry, layout, DPI scaling decisions, accessibility, focus, canvas transforms, drag, swipe, and
scroll interaction, frame pacing, retained tree commit and instance lifetime
decisions, paragraph layout policy, text input and text row decisions,
window placement and drag policy,
text and accessibility contracts, keyboard accelerator contracts, theme and style values,
portable built-in theme labels,
Button, Checkbox, Slider, Toggle, menu, color picker, material layers, and
selected Image and Progress paint decisions, plus Bevel and Separator line rendering.
Progress can also emit its rounded track, fill, border, and text through
separate generic raster shape and text capabilities. Its checked composition
module selects the font and positions the label from raw glyph measurements;
its checked style module resolves track, fill, and label roles from a portable
rule table. A checked parser bridge collects KSS rules into that table and
pauses for host-provided imports. The host only slices source bytes and
supplies imported text; Kryon resolves selectors and widget styles. Callers
still supply default faces and rule data.

The other `.zi` modules were moved from the previous implementation and are
not yet in the checked build. Native widget rendering and downstream app
integration are incomplete. The current C archive is therefore a subset of
the intended Kryon library. A `.zib` containing Kryon code must link only the
modules an application imports and require host capabilities explicitly.
`frame_pacing.zi` declares a scalar timer capability when its commit function
is linked. The adapter binds it to the selected backend's `SetTargetFPS`; a
bundle test links Ziran's public host archive and exercises the call with a
timer stub, without opening a display. `raster.zi` declares a typed record
line capability shared by Bevel and Separator; its adapter calls the platform's
line renderer. `raster_shape.zi` and `raster_text.zi` declare rounded rectangle
and UTF-8 text effects for Progress; their adapters pass draw calls to the
embedding renderer. `font_metrics.zi` declares width and line height effects
for the platform font rasterizer. Broader rendering and pointer-bearing widget
capabilities still need portable host contracts.

## Completion requirements

- Every maintained widget and its state, style, and layout behavior is a
  checked `.zi` module and joins the build.
- Native C, C++, Go, browser, and portable hosts execute equivalent supported
  behavior through declared effects.
- The public widget API is importable from applications without compiler rules
  keyed to Kryon names.
- Downstream applications build and pass their behavior gates with Ziran source,
  saved `.zir`, and linked `.zib` where applicable.
