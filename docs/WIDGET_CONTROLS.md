# Widget controls

## Toast actions

`Toast.Draw(session, Toast.Props) -> Toast.ToastResult` accepts an optional
`action_label: string`. A new nonempty `message` captures that label in
`Toast.ToastState.action_label`; keep passing `result.state` on later frames.
An empty label retains the existing passive toast.

The action is a standard Button, with its label exposed to accessibility and
normal pointer, Tab, Enter and Space activation. `Toast.ToastResult.action_clicked`
is true only on the submission that consumes the activation. The caller owns
the action and dismissal; set `clear = true` when it should disappear.
A replacement message replaces the action; expiry and clearing remove both.

## Bottom navigation

`Navigation.Draw(session, props, items)` defaults to a full-width 80-unit tonal
bottom bar. Reserve `Navigation.NavigationBarHeight(props)` above the bottom of the view.
For an embedded preview or panel, use `compact = true` with `bounds`;
`fit_items = true` fits the shortcuts into that rectangle.
Each `Navigation.NavigationBarItem` accepts an optional `icon_shape` (the standard Shape
format) or `image`; the icon is 24 units inside a 64 by 32 selection pill, with
its label below. With neither icon, the label is centered inside the pill.
Item keys, route results, disabled hit testing and custom style rules retain
their existing contracts. Pointer hover/press and keyboard focus draw state
layers; keyboard focus adds a ring. `compact` retains the scrolling dock/rail
layout used by Breeze, including its footer and drag/close behavior.

## Buttons

Installed system palettes give buttons the full (capsule) shape. A button
taller than 56 units is a tile, such as a launcher or keypad tile, and takes
20-unit card corners instead of becoming a stadium; `pill` and `circle` still
force a capsule. Neutral soft buttons are tonal: a light tint of the palette's
accent over the background. A labeled `image` in a tile fills the spare
height instead of staying at the styled icon size: up to 64 units above the
label, or up to 40 units beside it. A vector `icon_shape` still takes
precedence over an `image` and keeps the styled size.

A button draws only the icons its program imports. A vector `icon_shape` and
pixels handed over as `image.texture` always draw. A file picture in
`image.asset_path` draws once the program imports `kryon/image`, and a
built-in `icon_type` once it also imports `kryon/icon`. A button without an
icon carries no picture decoder, asset path or icon atlas. The same holds for
pictures a toolbar, title bar or modal header hands to its buttons.

## Breadcrumbs

`Breadcrumb.Draw(session: Session, props: Breadcrumb.Props, items: []Breadcrumb.BreadcrumbItem)
-> Breadcrumb.BreadcrumbResult` is exported by `kryon/breadcrumb`. Items borrow
`label: string` for the frame and carry a unique, stable, nonzero `key: u64`
and `disabled: bool`. No labels are interpreted as paths.

Props contain `key`, `bounds`, `id`, `class_name`, `active_index` (-1 for none),
`disabled`, `focused`, and `focused_index`. Enter/Space in `trigger_input`
activates the caller's focused item; `focused_index = -1` addresses overflow,
where Down also opens the menu. Native retained button focus works normally.
The widget never changes the caller's selection or focus. Read
`activated_index` (-1 for none), `activated_key` (0 for none), and `changed`:
repeat activation still reports the item, with `changed = false`.

`open` and `close` request overflow transitions; internal open state persists.
`overflow_label` defaults to an ellipsis. `menu_input: DropdownMenuInput`
provides Up/Down/Home/End, commit and Escape. Result `open`, `opened`, `closed`
reports menu state. Pointer, native Escape/focus dismissal and scrolling use
standard Popup and Scroll behavior. Submit after earlier page controls.

Widths use resolved Button styles and font advances. The tail and active
ancestor remain visible, compressed when necessary; all other hidden ancestors
remain in original order in a viewport-constrained, virtualized overflow.
Labels are clipped/fitted without slicing UTF-8. Borrowed lists of at least
2,048 items are supported without submitting offscreen rows. Zero, negative,
or fully clipped bounds submit no controls. Keys, not indices or labels,
preserve retained item identity and menu highlight across reorder.

## Context menus

`ContextMenu.Draw(session, props: ContextMenu.Props, items: []ContextMenu.ContextMenuItem)
-> ContextMenu.ContextMenuResult` is exported by `kryon/context_menu`. The caller
keeps `open` and the pointer position (`x`, `y`); the result reports
`activated_id` (0 for none) and `closed`. Submit it after the page's other
controls.

`ContextMenu.ContextMenuItem` fields, in order: `id: s32` (positive; reported when
chosen), `label: string`, `disabled: bool`, `separator: bool` (a divider; the
other fields are ignored), `inline: bool` (a compact cell sharing a row with
its inline neighbours), `parent: s32` (the id of the row whose submenu holds
this one; a row others name shows › and opens the submenu instead of
reporting), `accelerator: string`, `checkable: bool`, and `checked: bool`.
New fields are appended, so positional `.{id, label, disabled, separator}`
initializers keep their meaning.

`accelerator` is a shortcut label such as `"Ctrl++"`, drawn right-aligned in
the row in the theme's muted text colour, or in the disabled colour on a
disabled row, before any ›. The panel widens so each row's label, a 32-unit
gap and its accelerator fit. The menu only shows the label; the caller
handles the shortcut.

A `checkable` row draws a 14-unit check box at the row's left inset: empty
when unchecked, with a check mark when `checked`. When any row of a panel
is checkable, every label in that panel starts after the box column so the
labels line up, as in GTK menus; panels without check rows keep the plain
inset. Choosing a check row reports its id like any command, and the caller
flips `checked`. Check rows keep the option role and expose `checked` as the
row's selected state. Menus have no icons.

## Cards

`Card.Draw(session, Card.Props)` draws a filled, elevated or outlined Material surface.
Import it as `Card :: #import "kryon/card";`. Set `variant` to `CardFilled`
(the default), `CardElevated` or `CardOutlined`, and supply `bounds` and a stable
`key`. The returned `content` rectangle has 16 units of padding, reduced for
small bounds. Draw your content in that rectangle; Card does not open a scope.

Cards are decorative by default. `clickable` enables the standard Button's
pointer, keyboard, focus and disabled behavior; `activated` reports activation.
An optional `label` is drawn centered and announced by the clickable surface.
Selected and explicit state props retain their usual meaning. Installed Card
style rules apply by id, class and emphasis (soft, filled and outline for the
three variants). Elevated cards and floating menus/dialogs use two fixed,
1-unit contact strips, with no blur, offscreen bitmap or animation timer.

A ListBox with no explicit row height uses 40 units. Its icon size does not
set its row height. Explicit `row_height` and style `offset_y` still win.

## Swipe rows

`SwipeRow.Draw(session, props, state)` follows a horizontal drag, paints the revealed
strip and returns its content `offset` plus state for the next frame. Right
uses `start`, left uses `end`; an action needs `icon_shape`. `threshold` is a
share of row width (zero means 0.35). Optional `end_threshold` overrides it
for left swipes; zero uses the effective shared threshold. A release emits `swiped_start` or
`swiped_end`; `clicked` reports a separate tap or keyboard activation, while
`right_clicked` and `pointer_x`/`pointer_y` expose a secondary-button release.
The app chooses what a tap does. Vertical scrolling does not activate swipe actions.

Optional `long_start` and `long_end` actions add a second stage in either
direction. `long_threshold` is a width share (zero means 0.5) and must exceed
that direction’s effective short threshold; otherwise that stage is ignored. Its strip
color/icon appears at that distance. Release emits `swiped_long_start` or
`swiped_long_end`, exclusively with the short results, using the final distance
even after crossing and retreating. Each chosen action's `removes` controls
whether the row settles back. Without long actions, existing behavior remains.
Positive `start_width` or `end_width` replaces that side's row-width share with
a fixed bounds-unit travel span; zero retains row-width behavior. A two-stage
strip divides that span into equal first and second containers and clamps the
drag at the configured span.
For short-swipe completion and long-swipe editing, set equal `start_width` halves,
`threshold = 0.5`, `end_threshold = 0.35` to retain row-width left deletion,
`long_threshold = 1.0`, put completion in `start` and edit in `long_start`, and
handle their separate results. A tap need not complete the row.

## Workspace pager and centered panel groups

`workspace_pager.Draw(session, props, windows)` draws miniature workspace tiles
from the caller's real count, active workspace and desktop window geometry.
It returns the clicked workspace index, or -1. `Cell`, `WindowBounds` and `Width`
expose the same geometry for sizing and verification. The default is two rows;
window workspace -1 is sticky, and rectangles are clipped to each tile.

`Layout.CenteredBandsFor(bounds, width)` reserves a geometrically centered band
and returns disjoint leading and trailing rectangles. The center stays fixed
when the surrounding content changes; an oversized center is limited to the
available width and side groups can scroll within their remaining rectangles.

## Cover carousels

`kryon/carousel` provides `Carousel.Draw(session, props, items, state)` for a
full-cover browser. Keep a `Carousel.CarouselState`, pass `Carousel.CarouselItem` values with
portrait and landscape art, an optional source crop, an optional logo and
caption, and an action label. A zero `source` uses the whole selected image.
It returns the activated item index, or -1. Swipe, wheel, arrows and
Enter/Space share one selection; only that selection exposes an action.
The caller keeps game launching and any footer controls. `CarouselArtwork`
paints the same cover composition for a native loading screen and accepts the
same optional source crop, defaulting to the whole image.

## Loading surfaces

`kryon/loading_screen` provides `LoadingScreen.Draw(session, LoadingScreen.Props)`
for app starts and longer operations. Supply a title, localized detail and
an app icon (`icon_shape` or `icon_asset`). Its responsive composition uses
the current canvas, surface, border and accent tokens in native and hosted
apps. A quiet arc moves with `now`; `reduced_motion` keeps it still. Set
`has_progress`, `completed` and `total` only for measured work, to show a
slim standard progress bar instead. It never invents a percentage. The same
surface is exported by `kryon/loading_screen`; title/detail class IDs allow text styling.
Use `loading_indicator` for small inline waits, with the same theme accent.

## Rotated text

`Text.Props.rotation` rotates text clockwise in degrees about `bounds` top left
plus `Text.Props.origin` (logical units). Zero keeps horizontal text; `origin =
Vector2.{bounds.width / 2.0, bounds.height / 2.0}` rotates about the box centre.
Layout and font sizes stay unchanged. Native and Canvas rasters preserve the same pivot; local text clipping rotates with the text,
while ancestor clipping stays in its original coordinate space.

## Reading pages

`text_measure.TextPage(session, props, start)` returns a UTF-8 byte range
(`start`, `end`, `next`) that fits `props.bounds`. Draw `text[start:end]` with
the same Text props and continue at `next`. Only separating whitespace is
skipped; a viewport shorter than a line reports no progress. Apps keep their
own content anchors and page indexes so reflow preserves the reading place.

`Text.Props.line_gap` supplies extra units between lines. `typeface` names a
registered face or a font asset. Hosted fonts pass the same asset policy as
images; absolute host paths are refused. Native and Canvas hosts select the face
for measurement and for queued paint, restoring the interface font afterward.

### Native Text typography

Stylesheets set typography on `Text` and `Button` through `style.StyleData`. Set the
matching `control_props.StyleField` bit for each value.

| CSS property | StyleData fields | Supported values |
| --- | --- | --- |
| `font-weight` | `StyleFontWeight`, `font_weight: s32` | `normal` (400), `bold` (700), numbers from 1 to 1000. |
| `font-style` | `StyleFontStyle`, `font_style: FontStyle` | `FontStyleNormal`, `FontStyleItalic`, and `FontStyleOblique`. |
| `letter-spacing` | `StyleLetterSpacing`, `letter_spacing: float32` | `normal` (0) or a signed logical pixel length, including fractions, bounded to ±1024 logical pixels. |
| `line-height` | `StyleLineHeight`, `line_height: float32`, `line_height_unit: LineHeightUnit` | `LineHeightNormal` with value 0 uses the natural glyph height. `LineHeightFactor` multiplies the font size by the value (so 125% is 1.25). `LineHeightLength` takes a logical pixel length. Both numeric units accept zero. |

Explicit line height replaces the interline gap and centers the natural glyph
box within each line. Text measures and paints the same rounded line height.
Button measures the label's line box before positioning the glyphs and its optional
detail. Legacy styles with a positive `line_height` and no unit keep the existing
widget behavior. Text uses `Text.Props.line_gap`, and Button uses the legacy pixel height.

Cairo/Pango and Canvas use their platform's font matching and synthesis when a
family lacks the requested face. Canvas tracking requires `CanvasRenderingContext2D.letterSpacing`.
Pixmap's bundled regular outlines apply tracking between extended graphemes.
Android's selected Pixmap typeface uses Paint's native shaping and letter spacing.
Android API 28 and later use numeric weights. Earlier versions group them into
normal and bold, and Android maps oblique to italic. CSS relative weights and
angle-based oblique are outside this API.

`Text.Props.wrap = Text.Wrap.TextWrapAnywhere` wraps at extended Unicode grapheme
boundaries, including inside unbroken usernames. Combining marks, emoji ZWJ
sequences and flags stay whole. Spaces are preserved and CR, LF and CRLF end
lines. A single grapheme wider than the bounds stays whole on its own line;
nonpositive widths disable wrapping. The default `TextWrapAuto` still wraps at
word boundaries and keeps oversized Latin words intact. Supply enough height
for the resulting lines.

## Multi-select grid

`GridMulti.Draw(session: Session, props: GridMulti.Props, items: []GridMulti.GridMultiItem,
selected: []bool) -> GridMulti.GridMultiResult` is exported by `kryon/grid_multi`.
Each borrowed item has `key: u64`, `label: string` and an
optional `image: Image.Props`. Import `kryon/icon` and `kryon/image` for
`ImageForIcon(icon_type: s32, bounds: Rectangle, alt_text: string) -> Image.Props`
and `IconType`. For example, assign
`ImageForIcon(cast(s32)IconType.IconHome, Rectangle.{}, "Home")` to an item's
image; the grid assigns its destination bounds. Ship the referenced `icons/`
atlases as ordinary application assets. Images use the ordinary Image renderer, including
assets, previews and session-owned textures. Bounds and child image keys are
assigned by the grid. Use unique nonzero item keys to retain identity across
reordering; zero falls back to the current index. Remap caller state when the
application reorders or replaces items.

Keep the mutable selection slice (exactly one bool per item), `cursor`,
`anchor` and `scroll_offset` between calls. Items, labels and image data are
borrowed for the call; normal Image asset/texture lifetimes still apply.
The grid never retains the selection slice. Mismatched storage returns
`valid = false` without changing selection. Empty matching slices are valid,
with cursor/anchor -1 and zero scroll. `changed` reports selection changes;
`clicked_index` reports every accepted pointer click, even when selection
is unchanged; keyboard selection never sets it. `activated_index` reports Enter;
ordinary pointer clicks and Space do not activate. Hosts currently expose no
canonical double-click activation signal; callers retain their pointer-click
action policy. Space only toggles selection. Missing actions use -1.
`selected_count` includes offscreen items. Keep application actions in the caller.

`GridMulti.GridMultiLayout.GridMultiIcon` (default) measures columns from available width,
places the image above the label, and scrolls vertically. Compact measures rows
from height, places a smaller image before the label, and scrolls horizontally.
Set `cell_width`, `cell_height`, `image_size`, `gap`, `padding`, `scale`, `bounds`,
a stable session-unique widget `key` (including across embedded host roots), and optional style `class_name`/focus `id`. Default logical
cells are 96 by 96 (Icon) and 180 by 32 (Compact). Partial cells and their image
and label children clip to the viewport. Set `invisible = true` to retain
selection, input, scrolling and accessibility while supplying native painting. Use the normal retained frame for image
clipping and pointer routing.

Supply edge/repeat decisions through `navigation: GridMulti.GridMultiInput` and modifiers
through `control` and `shift`; keyboard input requires `focused` and an enabled
grid. Icon Left/Right moves within a row, Up/Down by measured columns; Compact
Up/Down moves within a column, Left/Right by measured rows. Incomplete final
rows/columns clamp to their last item. Ctrl arrows move focus alone, Ctrl click
and Space toggle, Shift extends from the persistent anchor, and Ctrl+Shift adds
a range. Home/End select the first/last item with the same modifiers; Enter
selects and activates the cursor. Navigation, a newly focused cursor and viewport
resize reveal focus; ordinary frames retain manual scrolling. `scroll_delta`
adds caller movement, while the standard Scroll handles sampled wheel/drag input.
Disabled grids accept no input. Unfocused grids still accept pointer input.
