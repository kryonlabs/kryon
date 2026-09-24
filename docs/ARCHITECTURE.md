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
| `src/ui/modules.txt` | Complete maintained UI module inventory |
| `src/backend/*.zi` | Host ABI declarations that generate C headers during the build |
| `src/backend/*_host.c`, `image_software.c` | Raw platform effects, font measurement, and rasterization |
| [Ziran](https://github.com/kryonlabs/ziran) checked out at `../ziran` | Language implementation and generic execution |

Every maintained UI source is in `modules.txt`. The checked build has no
handwritten Kryon headers; C hosts include headers generated from `.zi`.

Host adapters provide observations and effects through declared interfaces.
Widget state, policy, layout, and draw decisions belong in `.zi`, including for
desktop, browser, and portable hosts. Platform adapters may use C or Go where
the operating system requires it; they must not duplicate widget policy.

## Current build

`make` compiles the [checked module list](../src/ui/modules.txt) to `.zir`, C,
C++, and Go, then builds `build/ziran/libkryon.a` and the initial portable host
adapter `build/ziran/libkryon_host.a`. `make test` runs the current
source, saved-IR, and portable bundle tests. The checked modules cover
geometry, layout, DPI scaling decisions, accessibility, focus, canvas transforms, drag, checked swipe state and pointer ownership effects, and
scroll interaction, frame pacing, retained tree commit and instance lifetime
decisions, nested retained tree scopes, paragraph layout policy, text input and text row decisions,
checked Page and Section containers with automatic or explicit child bounds and returned
platform metadata, plus Heading and ParagraphText through the checked Text
widget and retained semantic roles,
Flow row scope and child placement, and Link paint, pointer activation, and
returned URL effects,
checked Column, Row, Stack, Group, Screen, and Grid retained scopes with
automatic placement for zero-positioned children and explicit placement otherwise,
checked Scroll viewport scopes with caller owned offsets and deltas, content
bounds, and inherited child paint and input clipping,
checked TreeView row composition, selection, keyboard navigation, scroll values,
draggable scrollbar, viewport clipping, and semantic row nodes from a caller
owned item slice,
checked PanedView split state, retained handle drag, pane rectangles, and
drop zone policy,
checked Toast lifetime, UTF-8 safe truncation, styled layout, and paint from
caller owned state and host time observations,
checked Collapsible header and close interaction, caller owned open/hidden
values, tree focus navigation from a borrowed ordered header slice, and paint,
checked SegmentedControl layout from a borrowed option slice and checked Button
children using the Segment KSS style kind,
checked TabBar composition from borrowed tabs, caller owned selection and
scroll state, retained pointer input, keyboard navigation, close and reorder
intents, KSS Tab and TabClose roles, and queued icon and text paint,
window placement and drag policy,
caller owned route list and stack policy through portable slices,
Router navigation, hash matching, and URL effect decisions through caller owned
route and state values,
text and accessibility contracts, keyboard accelerator contracts, theme and style values,
theme and orientation preference decisions from host observations,
portable built-in theme labels,
Button, Checkbox, Slider, Toggle, menu, color picker, material layers, and
selected Image and Progress paint decisions, plus Bevel and Separator line rendering.
The checked standard `Button(ButtonProps)` composes KSS state styling, label
measurement, retained pointer activation, and queued shape, clipped image, and
clipped text paint. `ButtonProps.image` uses portable `ImageProps` asset or
texture values and Ziran image fit policy. Glyph and raw texture icons, menu,
split, loading animation, material layers, keyboard focus, and immediate input
still need migration.

`Scroll(ScrollProps)` opens a clipped retained child scope and returns the
viewport, shifted content bounds, and a clamped scroll offset. The caller
stores that value and supplies keyboard deltas or explicit pointer and wheel
observations. Kryon resolves KSS scrollbar faces, queues track and thumb
paint, and returns drag ownership and consumption decisions. The platform
still supplies observations and retains drag ownership between frames; native
host integration remains open.

The checked `Checkbox(CheckboxProps)` accepts and returns portable values,
uses the retained pointer router, resolves KSS box and label roles, and queues
the box, mark, and clipped label. Its committed tree node carries the selected
state. The caller stores a returned value for the next frame; hosts supply raw
input, glyph measurements, and raster effects. Keyboard focus and native host
integration remain open.

The checked `Radio(RadioProps)` returns the activated option id while the
caller owns the checked value. It resolves KSS Ring, Mark, and Label roles,
uses retained pointer input, records selected state, and queues ring, fill,
hover layer, and clipped label paint. Selection animation, ripple, keyboard
focus, and native host integration remain open.

The checked `Slider(SliderProps)` and `DiscreteSlider(DiscreteSliderProps)`
compose caller owned scalar values, horizontal or vertical track geometry,
KSS Track, Fill, Label, and Thumb roles, retained drag input, and queued shape
and clipped label paint. An app composes multiple values from its own arrays
and `SliderCellBoundsFor()`. Angle editing, step buttons, numeric text editing,
limit labels, keyboard control, richer materials, and native host integration
remain open.

The checked `Spinbox(SpinboxProps)` owns its parent/value styling, two retained
Button child controls, step and wrap behavior, and decimal glyph layout in
Ziran. The caller stores the returned value. Native text editing, locale
formatting, and host integration remain open.

The checked `Selectable(SelectableProps)` composes a caller owned boolean,
retained pointer input, selected tree state, KSS styling, and queued row/text
paint. Keyboard activation and native host integration remain open.

The checked `TabBar(TabBarProps, []Tab)` accepts borrowed tab values with stable
keys and optional `ImageProps` icons. It reports selection, close, middle and
double click, and reorder events for the caller to apply. Scroll, drag, and
double click history are caller owned; the host supplies raw input and time.
Mouse drag panning and platform focus integration remain open.

The checked `Fieldset(FieldsetProps)` resolves KSS frame style, measures its
title from host supplied glyph metrics, and queues frame, title cover, and
clipped text paint from Ziran. Retained child layout composition remains open.

The checked `route.zi` module now owns list deduplication, allowed-set or
per-item-mask filtering, first-unused selection, moving, and stack transitions.
Applications keep route arrays and occupied counts; Ziran slices carry their
capacity, and no C callback or route storage pointer crosses the API.

The checked `locale_policy.zi` module keeps locale matching and catalog
fallback in Ziran. Catalog storage and platform preference discovery stay
outside the module; ordinary slices and strings cross its API.
The checked locale parser reads immutable catalog strings, normalizes line
endings in entry bodies, and writes entries and languages into caller supplied
span and byte slices. The parser has no allocator or file access.

The checked `Toggle(ToggleProps)` composes plain and labeled switches from
portable values. It resolves track, fill, label, and thumb KSS styles, uses
retained pointer activation, and queues rounded shapes and clipped labels.
The caller owns the returned value; the committed tree exposes its selected
state. Animated transitions, material layers, keyboard focus, and native host
integration remain open.

`Text(TextProps)` now resolves style, measures and wraps words, positions lines,
and queues clipped text and strikethrough paint from Ziran. The platform
provides raw byte slices, glyph metrics, and a raster callback that honors the
chosen clip. Letter spacing, parent style inheritance, and selectable text
interaction remain open.
The checked `Image(ImageProps)` composition now resolves KSS class styles,
uses asset dimensions or a supplied texture handle, selects source and fit,
and queues an image command with clip, radius, tint, origin, and rotation.
The platform host measures asset dimensions and rasterizes that command;
`ImageCanvasRasterizer()` is a headless RGBA8 implementation of that contract
using caller-supplied assets. Concrete desktop and browser adapters remain to
be connected.
Progress can also emit its rounded track, fill, border, and text through
separate generic raster shape and text capabilities. Its checked composition
module selects the font and positions the label from raw glyph measurements;
its checked style module resolves track, fill, and label roles from a portable
rule table. A checked parser bridge collects KSS rules into that table and
pauses for host-provided imports. The host only slices source bytes and
supplies imported text; Kryon resolves selectors and widget styles. The
one-argument Progress painter uses an installed Ziran rule table and Ziran
default faces. A persistent `.zib` instance keeps that rule table across
frames. Separator resolves its Line and Label roles, measures labels, and
emits line and text effects from checked Ziran. The portable retained tree
now owns node identity and Progress and Separator submissions through
`BeginTree()` and `EndTree()`. Widgets lower their paint decisions to a
checked generic command queue during submission. EndTree rejects an overfull
tree or paint queue before committing and emits raster effects only after
commit; it does not import individual widget painters.
`tree_input.zi` now hit tests committed nodes, keeps press ownership by stable
identity across tree reordering, and emits consumable activation on release.
`pointer_input.zi` imports raw pointer samples through the reusable host
binding. Button, Checkbox, Toggle, and Radio use this path for retained
pointer activation. Slider nodes retain drag
ownership across tree reordering and pointer movement outside their bounds;
the route reports held and release coordinates with the original grab offset
and cancels a disabled drag. PanedView uses this route for its handle and
returns pane rectangles and the updated split to its caller.

The checked `TextField(TextFieldProps)` consumes raw pointer and keyboard
samples, resolves KSS styles, emits text, selection, and caret paint, and
returns a UTF-8 byte replacement range plus cursor and focus state. The caller
owns the string and applies edits. A secure field supplies its display mask;
platform text services and IME integration remain host and app work.

The checked `TextArea(TextAreaProps)` also owns visual row wrapping, pointer
selection, vertical and page navigation, scrolling, multiline replacement
intents, Ziran/C/Make syntax coloring, composition underlines, and clipped
text, selection, and caret paint. The caller stores the string and editing
state. IME event handling, clipboard commands, and native host integration
remain migration work.

Remaining input modes, including keyboard focus, still need migration.
Ancestor viewport clipping works for retained child paint and input, but
full widget and platform integration remains incomplete.

All maintained `src/ui/*.zi` modules are in the checked build. Native window
hosts, platform text services, and downstream app integration are incomplete.
A `.zib` containing Kryon code links only the modules an application imports
and requires host capabilities explicitly.
`frame_pacing.zi` declares a scalar timer capability when its commit function
is linked. The adapter binds it to the selected backend's `SetTargetFPS`; a
bundle test links Ziran's public host archive and exercises the call with a
timer stub, without opening a display. `raster.zi` declares a typed record
line capability shared by Bevel and Separator; its adapter calls the platform's
line renderer. `raster_shape.zi` and `raster_text.zi` declare rounded rectangle
and UTF-8 text effects for Progress; their adapters pass draw calls to the
embedding renderer. `font_metrics.zi` declares width and line height effects
for the platform font rasterizer. `pointer_input.zi` declares the raw pointer
sample capability; `PointerBinding()` accepts current device coordinates and
button transitions. Other input and rendering capabilities still need host
adapters.

## Completion requirements

- Every maintained widget and its state, style, and layout behavior is a
  checked `.zi` module and joins the build.
- Native C, C++, Go, browser, and portable hosts execute equivalent supported
  behavior through declared effects.
- The public widget API is importable from applications without compiler rules
  keyed to Kryon names.
- Downstream applications build and pass their behavior gates with Ziran source,
  saved `.zir`, and linked `.zib` where applicable.
