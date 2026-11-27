# Widget controls

Ready-made interactive controls: toasts, bottom navigation, buttons,
breadcrumbs, context menus, cards, swipe rows, the workspace pager, cover
carousels, loading surfaces, rotated text, paged reading and the multi-select
grid. Applications place each control through its typed TSX component from
`kryon/tsx/*`; the props, results and actions are the checked native
declarations. Read the section for the control you are placing or styling.

## Toast actions

`Toast({session, props})` from `kryon/tsx/toast` returns the checked
`ToastResult` (from `kryon/toast`) and accepts an optional
`action_label: string`. A nonempty `message` captures that label in
`ToastState.action_label`; keep passing `result.state` on later frames.
An empty label gives a passive toast.

The action is a standard Button, with its label exposed to accessibility and
normal pointer, Tab, Enter and Space activation. `ToastResult.action_clicked`
is true only on the submission that consumes the activation. The caller owns
the action and the dismissal; set `clear = true` when the toast should
disappear. A replacement message replaces the action; expiry and clearing
remove both.

## Bottom navigation

`NavigationBar({session, props, items})` from `kryon/tsx/navigation` defaults
to a full-width 80-unit tonal bottom bar. Reserve `NavigationBarHeight(props)`
from `kryon/navigation` above the bottom of the view. For an embedded preview
or panel, use `compact = true` with `bounds`;
`fit_items = true` fits the shortcuts into that rectangle.

Each `NavigationBarItem` accepts an optional `icon_shape` (the
standard Shape format) or `image`; the icon is 24 units inside a 64 by 32
selection pill, with its label below. With neither icon, the label is centered
inside the pill. Item keys, route results, disabled hit testing and custom
style rules keep their contracts. Pointer hover/press and keyboard focus draw
state layers; keyboard focus adds a ring. `compact` keeps the scrolling
dock/rail layout used by Breeze, including its footer and drag/close behavior.

## Buttons

Installed system palettes give buttons the full (capsule) shape. A button
taller than 56 units is a tile, such as a launcher or keypad tile, and takes
20-unit card corners instead of becoming a stadium; `pill` and `circle` still
force a capsule. Neutral soft buttons are tonal: a light tint of the palette's
accent over the background. A labeled `image` in a tile fills the spare
height instead of staying at the styled icon size: up to 64 units above the
label, or up to 40 units beside it. A vector `icon_shape` takes precedence
over an `image` and keeps the styled size.

A button draws only the icons its program imports. A vector `icon_shape` and
pixels handed over as `image.texture` always draw. A file picture in
`image.asset_path` draws once the program imports `kryon/image`, and a
built-in `icon_type` once it also imports `kryon/icon`. A button without an
icon carries no picture decoder, asset path or icon atlas. The same holds for
pictures a toolbar, title bar or modal header hands to its buttons.

## Breadcrumbs

`Breadcrumb({session, props, items})` from `kryon/tsx/breadcrumb` returns the
checked `BreadcrumbResult`; its `Breadcrumb.Props` and `BreadcrumbItem` come
from `kryon/breadcrumb`. No labels are interpreted as paths.

| `BreadcrumbItem` field | Meaning |
| --- | --- |
| `label: string` | Borrowed for the frame |
| `key: u64` | Unique, stable and nonzero; keys, not indices or labels, preserve retained item identity and menu highlight across reorder |
| `disabled: bool` | Skips activation |

| `Breadcrumb.Props` field | Meaning |
| --- | --- |
| `key`, `bounds`, `id`, `class_name` | Standard placement, identity and styling |
| `active_index` | Current item, -1 for none |
| `disabled` | Disables the bar |
| `focused`, `focused_index` | Keyboard focus; `focused_index = -1` addresses overflow, where Down also opens the menu |
| `trigger_input` | Enter/Space activates the caller's focused item |
| `open`, `close` | Request overflow transitions; internal open state persists |
| `overflow_label` | Label for the collapsed overflow; defaults to an ellipsis |
| `menu_input: DropdownMenuInput` | Up/Down/Home/End, commit and Escape in the overflow menu |

The result reports `activated_index` (-1 for none), `activated_key` (0 for
none), `changed`, and the menu state `open`, `opened`, `closed`. Repeat
activation still reports the item, with `changed = false`. The widget never
changes the caller's selection or focus. Native retained button focus works
normally. Pointer, native Escape/focus dismissal and scrolling use standard
Popup and Scroll behavior. Submit after earlier page controls.

Widths use resolved Button styles and font advances. The tail and active
ancestor remain visible, compressed when necessary; all other hidden ancestors
remain in original order in a viewport-constrained, virtualized overflow.
Labels are clipped/fitted without slicing UTF-8. Borrowed lists of at least
2,048 items are supported without submitting offscreen rows. Zero, negative,
or fully clipped bounds submit no controls.

## Context menus

`ContextMenu({session, props, items})` from `kryon/tsx/context_menu` returns
the checked `ContextMenuResult`; its props and items come from
`kryon/context_menu`. The caller keeps `open` and the pointer
position (`x`, `y`); the result reports `activated_id` (0 for none) and
`closed`. Submit it after the page's other controls.

| `ContextMenuItem` field | Meaning |
| --- | --- |
| `id: s32` | Positive; reported when chosen |
| `label: string` | Row text |
| `disabled: bool` | Draws and reports as disabled |
| `separator: bool` | Divider row; the other fields are ignored |
| `inline: bool` | Compact cell sharing a row with its inline neighbours |
| `parent: s32` | Id of the row whose submenu holds this one; a row others name shows › and opens the submenu instead of reporting |
| `accelerator: string` | Shortcut label such as `"Ctrl++"`, right-aligned in the muted (disabled rows: disabled) text colour before any ›; the menu only shows it, the caller handles the shortcut |
| `checkable: bool` | Draws a 14-unit check box at the row's left inset |
| `checked: bool` | Check state of a checkable row; the caller flips it after a choice reports the id |

New fields are appended in order, so existing item literals keep their
meaning. The panel widens so each row's label, a
32-unit gap and its accelerator fit. When any row of a panel is checkable,
every label in that panel starts after the box column so labels line up, as in
GTK menus; panels without check rows keep the plain inset. Check rows keep the
option role and expose `checked` as the row's selected state. Menus have no
icons.

## Cards

`Card({session, props, children})` from `kryon/tsx/card` opens a filled,
elevated or outlined Material surface as a padded column: while the result is
`opened`, the component draws the `children` inside the column and closes it
afterwards. Set `variant` (a `CardVariant` from `kryon/card`) to `CardFilled`
(the default), `CardElevated` or `CardOutlined`. Without a height the card
grows to its
content; with `bounds` it keeps them, and `content` is the area inside its
16 units of padding.

Cards are decorative by default. `clickable` makes the card one press target
with pointer, keyboard, focus and disabled behavior; `activated` reports activation.
An optional `label` names the clickable card for assistive technology.
Selected and explicit state props keep their usual meaning. Installed Card
style rules apply by id, class and emphasis (soft, filled and outline for the
three variants). Elevated cards and floating menus/dialogs use two fixed,
1-unit contact strips, with no blur, offscreen bitmap or animation timer.

A ListBox with no explicit row height uses 40 units. Its icon size does not
set its row height. Explicit `row_height` and style `offset_y` still win.

## Swipe rows

`SwipeRow({widget: {session, props}, state})` from `kryon/tsx/swipe_row`
follows a horizontal drag, paints the revealed strip and returns its content
`offset` plus state for the next frame.

| `SwipeRow` prop | Meaning |
| --- | --- |
| `start`, `end` | Action shown per direction; an action needs `icon_shape` |
| `threshold` | Completion as a share of row width; zero means 0.35 |
| `end_threshold` | Left-swipe override; zero uses the effective shared threshold |
| `long_start`, `long_end` | Optional second stage per direction |
| `long_threshold` | Width share (zero means 0.5); must exceed that direction's effective short threshold or the stage is ignored |
| `start_width`, `end_width` | Positive value replaces that side's row-width share with a fixed bounds-unit travel span; zero keeps the share. A two-stage strip divides the span into equal containers and clamps the drag at the span |
| `removes` | Per chosen action: whether the row settles back |

| `SwipeRow` result | Meaning |
| --- | --- |
| `offset` | Current content offset |
| `swiped_start`, `swiped_end` | Release past that side's short threshold |
| `swiped_long_start`, `swiped_long_end` | Second-stage release; exclusive with the short results, using the final distance even after crossing and retreating |
| `clicked` | Separate tap or keyboard activation; the app chooses what a tap does |
| `right_clicked`, `pointer_x`, `pointer_y` | Secondary-button release |

Vertical scrolling does not activate swipe actions. For short-swipe completion
and long-swipe editing, set equal `start_width` halves, `threshold = 0.5`,
`end_threshold = 0.35` to keep row-width left deletion, `long_threshold = 1.0`,
put completion in `start` and edit in `long_start`, and handle their separate
results. A tap need not complete the row.

## Workspace pager and centered panel groups

`WorkspacePager({session, props, windows})` from `kryon/tsx/workspace_pager`
draws miniature workspace tiles from the caller's real count, active workspace
and desktop window geometry. It returns the clicked workspace index, or -1.
`Cell`, `WindowBounds` and `Width` (checked declarations in
`kryon/workspace_pager`) expose the same geometry for sizing and verification.
The default is
two rows; window workspace -1 is sticky, and rectangles are clipped to each
tile.

`Layout.CenteredBandsFor(bounds, width)` reserves a geometrically centered band
and returns disjoint leading and trailing rectangles. The center stays fixed
when the surrounding content changes; an oversized center is limited to the
available width and side groups can scroll within their remaining rectangles.

## Cover carousels

`Carousel({session, props, items, state})` from `kryon/tsx/carousel` is the
full-cover browser. Keep a `CarouselState` behind a `NativeRef`, pass
`CarouselItem` values with portrait and landscape art, an optional source
crop, an optional logo and caption, and an action label. A zero `source` uses
the whole selected
image. It returns the activated item index, or -1. Swipe, wheel, arrows and
Enter/Space share one selection; only that selection exposes an action.
The caller keeps game launching and any footer controls. `CarouselArtwork`
paints the same cover composition for a native loading screen and accepts the
same optional source crop, defaulting to the whole image.

## Loading surfaces

`LoadingScreen({session, props})` from `kryon/tsx/loading_screen` covers app
starts and longer operations.

| `LoadingScreen.Props` | Meaning |
| --- | --- |
| title, detail | Title and localized detail; their class IDs allow text styling |
| `icon_shape`, `icon_asset` | App icon |
| `has_progress`, `completed`, `total` | Set only for measured work; shows a slim standard progress bar. The surface never invents a percentage |
| `reduced_motion` | Keeps the quiet arc still |
| `now` | Drives the arc |

The responsive composition uses the current canvas, surface, border and accent
tokens in native and hosted apps. Use `loading_indicator` for small inline
waits, with the same theme accent.

## Rotated text

`Text.Props.rotation` rotates text clockwise in degrees about `bounds` top left
plus `Text.Props.origin` (logical units). Zero keeps horizontal text; an
`origin` of `{x: bounds.width / 2.0, y: bounds.height / 2.0}` rotates about
the box centre. Layout and font sizes stay unchanged. Native and Canvas rasters
preserve the same pivot; local text clipping rotates with the text, while
ancestor clipping stays in its original coordinate space.

## Reading pages

`text_measure.TextPage(session, props, start)` returns a UTF-8 byte range
(`start`, `end`, `next`) that fits `props.bounds`. Draw `text[start:end]` with
the same Text props and continue at `next`. Only separating whitespace is
skipped; a viewport shorter than a line reports no progress. Apps keep their
own content anchors and page indexes so reflow preserves the reading place.

`Text.Props.line_gap` supplies extra units between lines. `typeface` names a
registered face or a font asset. Hosted fonts pass the same asset policy as
images; absolute host paths are refused. Native and Canvas hosts select the
face for measurement and for queued paint, restoring the interface font
afterward.

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
Button measures the label's line box before positioning the glyphs and its
optional detail. A positive `line_height` with no unit is a pixel height:
Text uses `Text.Props.line_gap` and Button the pixel height.

Cairo/Pango and Canvas use their platform's font matching and synthesis when a
family lacks the requested face. Canvas tracking requires
`CanvasRenderingContext2D.letterSpacing`. Pixmap's bundled regular outlines
apply tracking between extended graphemes. Android's selected Pixmap typeface
uses Paint's native shaping and letter spacing. Android API 28 and later use
numeric weights. Earlier versions group them into normal and bold, and Android
maps oblique to italic. CSS relative weights and angle-based oblique are
outside this API.

`Text.Props.wrap = Text.Wrap.TextWrapAnywhere` wraps at extended Unicode
grapheme boundaries, including inside unbroken usernames. Combining marks,
emoji ZWJ sequences and flags stay whole. Spaces are preserved and CR, LF and
CRLF end lines. A single grapheme wider than the bounds stays whole on its own
line; nonpositive widths disable wrapping. The default `TextWrapAuto` still
wraps at word boundaries and keeps oversized Latin words intact. Supply enough
height for the resulting lines.

## Multi-select grid

`GridMulti({session, props, items, selected})` from `kryon/tsx/grid_multi`
returns the checked `GridMultiResult`; its `GridMulti.Props`, `GridMultiItem`
and result records come from `kryon/grid_multi`.

| `GridMultiItem` field | Meaning |
| --- | --- |
| `key: u64` | Unique and nonzero to retain identity across reordering; zero falls back to the current index |
| `label: string` | Row label |
| `image: Image.Props` | Optional image; bounds and child image keys are assigned by the grid |

Build images with `ImageForIcon(icon_type, bounds, alt_text)` and `IconType`
from `kryon/icon` (import `kryon/image` too); `kryon/tsx/icon` wraps the same
helper into `Image` props. Ship the referenced `icons/` atlases as ordinary
application assets. Images use the ordinary Image renderer, including assets,
previews and session-owned textures.

Keep the mutable selection slice (exactly one bool per item), `cursor`,
`anchor` and `scroll_offset` between calls. Items, labels and image data are
borrowed for the call; normal Image asset/texture lifetimes still apply. The
grid never retains the selection slice. Remap caller state when the
application reorders or replaces items.

| `GridMulti.Props` | Meaning |
| --- | --- |
| `cell_width`, `cell_height` | Logical cell size; defaults 96 by 96 (Icon) and 180 by 32 (Compact) |
| `image_size`, `gap`, `padding`, `scale` | Layout metrics |
| `bounds`, `key`, `class_name`, `id` | Placement, stable session-unique widget key (including across embedded host roots), style class and focus id |
| `invisible` | Retains selection, input, scrolling and accessibility while the host paints natively; use the normal retained frame for image clipping and pointer routing |
| `navigation` | Edge/repeat decisions for keyboard input |

| `GridMulti.GridMultiLayout` | Behavior |
| --- | --- |
| `GridMultiIcon` (default) | Measures columns from available width, places the image above the label, scrolls vertically |
| Compact | Measures rows from height, places a smaller image before the label, scrolls horizontally |

| `GridMultiResult` | Meaning |
| --- | --- |
| `valid` | False on mismatched storage, without changing selection; empty matching slices are valid, with cursor/anchor -1 and zero scroll |
| `changed` | Selection changed |
| `clicked_index` | Every accepted pointer click, even when selection is unchanged; keyboard selection never sets it |
| `activated_index` | Enter activation; ordinary pointer clicks and Space do not activate. Hosts expose no canonical double-click activation, so callers keep their pointer-click action policy |
| `selected_count` | Includes offscreen items |

Missing actions use -1; keep application actions in the caller.

Keyboard input requires `focused` and an enabled grid; modifiers arrive
through `control` and `shift`.

| Input | Effect |
| --- | --- |
| Icon Left/Right, Up/Down | Moves within a row, then by measured columns |
| Compact Left/Right, Up/Down | Moves by measured rows, then within a column |
| Ctrl arrows | Move focus alone |
| Ctrl click, Space | Toggle selection; Space never activates |
| Shift | Extends from the persistent anchor |
| Ctrl+Shift | Adds a range |
| Home/End | Select first/last item with the same modifiers |
| Enter | Selects and activates the cursor |

Incomplete final rows/columns clamp to their last item. Navigation, a newly
focused cursor and viewport resize reveal focus; ordinary frames keep manual
scrolling. `scroll_delta` adds caller movement, while the standard Scroll
handles sampled wheel/drag input. Disabled grids accept no input. Unfocused
grids still accept pointer input.
