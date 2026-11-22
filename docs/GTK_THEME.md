# GTK3 theme rendering on Linux

Native GTK3 theming for a Linux application: query styles, raster CSS
backgrounds, icons, text and native widgets, and manage providers and
invalidation. Read this when your app should look like the desktop GTK theme.

Import `kryon/GtkStyleLinux` only in a Linux native application that links GTK3.
This module reads the current GTK settings, screen CSS providers, icon theme and
Pango fonts. It renders offscreen; the application supplies its selectors, layout,
input and accessibility. Call its functions on the GTK owning thread, and call
`GtkStylePoll()` from a non-GTK event loop to deliver up to 32 pending GLib events.

Pass an ancestor-to-leaf slice of `GtkStyleNode` values.

| `GtkStyleNode` field | Meaning |
| --- | --- |
| `widget_type` | For example `"GtkButton"` |
| `element`, `name` | CSS element and CSS ID |
| `classes: []string` | CSS classes |
| `state: u32` | `GtkStyleNormal`, `GtkStyleActive`, `GtkStylePrelight`, `GtkStyleSelected`, `GtkStyleDisabled`, `GtkStyleFocused` or `GtkStyleChecked`; combine flags for compound states |
| `providers: []GtkStyleProviderAttachment` | Optional. Each attachment carries a live `provider: u64` handle and native `priority: u32`, and applies only to that node's GTK context, including ancestor contexts. At equal priority, GTK gives context providers precedence over screen providers, using its own selector rules |
| `sibling_count`, `sibling_index` | Optional homogeneous sibling group (1–64) and zero-based index for native child selectors; zero count leaves position unspecified. Header measurement builds the same column group, so first/middle/last CSS borders affect its real child allocations |

### Style query and raster calls

| Call | Behavior |
| --- | --- |
| `GtkStyleQuery(nodes, scale)` | Returns `GtkStyleMetrics`: `available`, `foreground` and `background` as RRGGBBAA, `border`, `padding` and CSS `margin` in logical pixels, a copied NUL-terminated `font_family: [256]u8`, logical-pixel `font_size: float64`, Pango `font_weight` and `font_style`, and screen `dpi`. GtkTreeView selectors also expose native `vertical_separator` and `horizontal_separator` style properties. |
| `GtkStyleRaster(nodes, width, height, scale)` | Paints the CSS background and frame. Its bounds are the CSS border box; callers lay out CSS margins separately. |
| `GtkStyleIndicatorRaster(nodes, width, height, scale, indicator)` | Asks GTK to draw the last node's check box (0), radio option (1) or submenu arrow (2, pointing right, or left in a right-to-left context) with `gtk_render_check`, `gtk_render_option` or `gtk_render_arrow`. |
| `GtkStyleRasterRegion(nodes, width, height, scale, x, y, crop_width, crop_height)` | Renders a crop of the original logical CSS box, preserving gradients and borders. Logical boxes can reach 8192 pixels per axis; the allocated crop retains the ordinary 4096-axis and 4,194,304-pixel bitmap limits. |
| `GtkIconRaster(name, size, scale, nodes, force_size = true, styled = false)` | Resolves a named icon using the current GTK icon search path and theme inheritance. PNG, SVG and symbolic icons use GTK's loader; symbolic colors come from the supplied style context. Clear `force_size` to retain the theme's natural artwork, as GtkImage does. Set `styled` with the image's full CSS ancestry to render through GTK's icon surface painter, including native transforms, shadows, direction and state. `GtkIconSize(category)` resolves GTK's menu/toolbar/button size categories. `GtkUserDirectory(index)` returns GLib's borrowed XDG user-directory path. |

### Text, cells and native widgets

| Call | Behavior |
| --- | --- |
| `GtkWidgetMeasure(nodes, text = "", sort_arrow = false, allocation_width = 0, allocation_height = 0, options = .{})` | Returns `available` and minimum/natural width and height for GtkButton, GtkEntry, GtkMenuBar, GtkMenuItem, GtkToolbar, GtkToolButton, GtkStatusbar or a horizontal GtkPaned. It constructs an offscreen hierarchy with the supplied classes, names and providers. Tree-view header selectors use a real GtkTreeViewColumn, including its label and native sort arrow. Menu bars include one item with the supplied label; icon toolbars include one item with the supplied icon name at GTK's large toolbar size. For these containers, `item_x/y/width/height` report GTK's representative child allocation. Zero allocation dimensions use the natural size. Header `item_*` fields describe its label, and `arrow_*` describe its visible sort image. For entries, `text` optionally names the primary icon; `item_x/y` are native layout offsets, `item_width` is the remaining text-area width, `item_height` is the layout height, and `arrow_*` is the icon area. Status bars use `text` as their message and return the label allocation, which follows the actual supplied message, native alignment and ellipsis, so cache it with the message text. The offscreen root is never shown. Entry allocation realizes an offscreen backing surface so GTK can supply its text-area bounds; it creates no visible window. `GtkWidgetMeasureOptions.margin` overrides the final widget's template margins on edges selected by `margin_edges`; the default zero mask preserves all native margins. Combine `GtkWidgetMarginLeft/Right/Top/Bottom`, or use `GtkWidgetMarginVertical` or `GtkWidgetMarginAll`; values use logical pixels. A paned widget includes two empty visible children, so its preferred width measures the native separator without adding application content widths. |
| `GtkCellTextMeasure(nodes, text)`, `GtkCellTextMeasureMany(nodes, texts, width_chars = -1, ellipsize = false)` | Measure actual GtkCellRendererText requisitions in a native GtkTreeView hierarchy. The batch reuses one renderer and reports component-wise maximum minimum/natural dimensions; renderer padding, Pango metrics and optional end ellipsis are included. |
| `GtkTextMeasure(nodes, text, width, scale, font = "", markup = false)` | Returns `available`, logical-pixel `width`, `height` and `baseline`. A zero width measures natural size; a positive width wraps by word or character. |
| `GtkTextRaster(nodes, text, width, height, scale, align, font = "", markup = false, ellipsize = false, snap_y = false)` | Paints Pango text and GTK text shadows onto transparency, vertically centered and clipped to the box. Alignment is 0 left, 1 center or 2 right. An empty `font` uses the computed CSS font; a Pango description overrides it for application settings such as a clock font. Set `ellipsize` for a single line with native Pango end ellipsis instead of wrapping. Set `snap_y` for integer vertical positioning, matching GTK's widget and cell allocations. |

Text layouts use GTK's widget Pango context, including explicit GTK font
settings. Unresolved Cairo font options inherit the native display backend's
defaults, queried from an empty compatible surface, before rasterization.
This preserves the glyphs used by realized GTK widgets when drawing to RGBA.

Native GTK backend adapters can use `GtkNativeReady()` and
`GtkNativeWidgetNodeApply(widget, node)` on the GTK owning thread. The latter
validates selectors/providers and applies them to a borrowed live GTK widget;
the adapter owns its widget lifecycle. `GtkNativeSurfaceBitmap(surface, width,
height)` copies a borrowed Cairo ARGB32 image surface into an owning bitmap.
It checks status, type, format, exact dimensions, stride and allocation bounds;
release the returned buffer with `GtkStyleBitmapRelease`.

### Native scrolled windows

`kryon/GtkScrollLinux` owns offscreen GTK scrolled windows for native scrollbar
layout, adjustments and drawing. `GtkScrollCreate(nodes)` accepts a `GtkWindow`
root, optional `GtkBox`/`GtkGrid` ancestors and a final `GtkScrolledWindow`;
it returns a bounded owning handle, or zero. Release it with
`GtkScrollRelease(handle)`. `GtkScrollCancel(handle)` cancels an owned drag
on focus loss or hiding; valid handles are idempotent. All calls stay on GTK's
owning thread.
`GtkScrollUpdate(handle, props)` takes viewport/content dimensions, offsets,
enabled axes, overlay policy and optional pointer state. Its metrics expose the
actual viewport, bar allocations, opacity, adjustment ranges and slider limits.
Use those allocations for content layout rather than reserving a fixed gutter.
`GtkScrollWheel(handle, delta, horizontal = false)` applies the native discrete
wheel distance immediately. Positive deltas move toward the start. Its page-based
unit follows GTK and its adjustment keeps fractional offsets across notches;
the returned metrics expose the updated values in the same call.
`GtkScrollRaster(handle, horizontal = false, scale = 1)` returns an owning
straight RGBA bitmap for the allocated native bar, at scale 1–4; release it with
`GtkStyleBitmapRelease`. Draw it over content at its reported bar allocation.
`GtkScrollRasterOver(handle, backdrop, horizontal = false, scale = 1)` accepts
a borrowed RGBA backdrop matching the scaled allocation. GTK draws
directly over it on an owned native compatible surface, preserving window
curve coverage and per-layer alpha rounding. The result replaces that backdrop.
`GtkPainterScrollOver(painter, handle, bounds, horizontal = false)` supplies the
painter's current backdrop and clips that replacement to its visible area.
`GtkNativeWidgetRaster(widget, width, height, scale, backdrop)` provides
the same bounded raster operation for a borrowed allocated GTK widget. The
caller retains widget ownership; only newly allocated surfaces are read.

`Scroll.Props.external_scrollbar` and `GridMulti.Props.external_scrollbar` let a
caller supply native scrollbar visuals and input. They preserve the complete
supplied viewport while retaining wheel, keyboard reveal, content dragging and
momentum; the caller synchronizes the resulting offset with its native bars.
`Scroll.Props.immediate_wheel` and `GridMulti.Props.immediate_wheel` opt out of
wheel gliding while retaining drag behavior. The default retains wheel gliding.
`ScrollWheelTake(session, key, bounds, scrolls, horizontal = false)` consumes
the targeted wheel once; horizontal mode includes the sideways wheel.
`GridMulti.Props.preserve_scroll_on_focus` retains a native view's manual offset
when focus returns. Its default keeps the focus reveal; cursor,
navigation, initial-open, layout and viewport changes still reveal the item.
An explicit valid `TreeSetCursor` shape also applies to disabled controls;
loading controls retain their loading cursor policy. Without an override,
disabled controls continue using `CursorDisabled`.

Desktop `PollPointer` and `PollSecondaryPointer` expose the gathered frame
sample, including focus-loss release and cleared held state. Raw SDL collection
stays separate from this published view. On X11, physical button sampling also
recovers transitions missed while another window owns focus; queued event edges
still preserve taps completed between polls. Other SDL drivers keep their
event sampling.

### Composited raster regions

`GtkStyleRasterRegionOver` takes the same box/crop coordinates as
`GtkStyleRasterRegion`, followed by a borrowed straight-alpha RGBA backdrop
matching the scaled crop. CSS background and frame layers draw directly over
that backdrop with Cairo's native rounding. Rendering uses a newly allocated
surface compatible with the GDK window backend, with an image-surface fallback.
The backdrop uploads with Cairo SOURCE; readback maps only that owned surface.
Device scale is normalized before allocation so the supplied CSS scale is
applied once, independently of `GDK_SCALE`. The returned bitmap is owned;
release it with `GtkStyleBitmapRelease`. An empty backdrop means transparent.
`GtkPainterStyleOver(painter, nodes, bounds)` draws those layers on the current
painter pixels and replaces the clipped crop without applying alpha twice.
Toolbar `GtkWidgetMeasure` results include the actual child button rectangle
in `control_*` and image rectangle in `arrow_*`, relative to the toolbar.

### File icons, sizes and dates

`GtkFileIconRaster(path, size, scale, nodes)` resolves a local file's GIO icon
through the active GTK theme, including MIME inheritance, special user folders
and the filesystem root. `GtkFileDescription(path, output)` returns the native
localized content description. `GtkFileLinkTarget(path, output)` queries the
link itself and returns its raw target without following it. These byte-path
APIs accept up to 4095 bytes without NUL; text helpers write NUL-terminated
output and return its byte length, or -1 on failure. The caller must escape
raw filenames and link targets before displaying them. `GtkImageRaster(path,
width, height)` uses installed GdkPixbuf loaders and preserves aspect ratio.
`GtkFormatFileSize(size, output)` uses GLib IEC units. `GtkFormatFileDate(unix_time,
output, style = .LocaleDate, now = -1, domain = "")` uses the local timezone and
locale date format. Its `GtkFileDateStyle.RelativeDate` option uses local calendar
days: Today, Yesterday, weekday names for two through six days ago, and the locale
date otherwise. Zero or invalid timestamps use Unknown in relative mode.
Relative labels use the native gettext catalog named by `domain`, or the
application's default catalog when empty. A negative `now` uses the current time;
an explicit timestamp permits deterministic comparisons and midnight refreshes.
The same formatter is available without GTK initialization as
`kryon/FileDateLinux.FormatFileDate`, with its `DateStyle` enum.
`GtkFileSystemFree(path)`
returns available bytes from GIO, or -1 when unavailable.
`GtkSerializedIconRaster(value, size, scale, nodes)` resolves a serialized GIcon
without discarding its native identity or fallback names. `GtkContentDescription`
accepts a GIO content type and the same output-buffer contract as file descriptions.

### The painter composer

`kryon/GtkPainterLinux` composes these rasters into one managed `Image` per
surface. Own one `GtkPainter`; call `GtkPainterBegin(*painter, session, bounds)`,
`GtkPainterPaint(*painter, nodes, bounds, text = "", kind = 0, align = 0, revision = 0)` and
`GtkPainterEnd(*painter, key)` inside a retained frame; it returns upload success.
`GtkPainterBitmap(*painter, bounds, bitmap)` composites a borrowed native bitmap
at its exact pixel dimensions, with the current clip and draw order. It copies
no ownership into the tile cache; the caller can release or reuse the bitmap
immediately. Invalid dimensions/buffer lengths return false without painting.
Kinds are CSS surface (0),
ellipsized text (1), themed icon (2), local file icon (3), image thumbnail (4),
Pango markup (5), serialized GIcon (6), styled naturally sized themed icon (7),
wrapped text (8), and the theme's own check box (9), radio option (10) and
submenu arrow (11) for the last node, such as `menuitem > check:checked`.
Text snaps to integer positions; thumbnails and natural icons center their
actual bitmap within the allocation. Cached RGBA layers follow Cairo's 8-bit
premultiplied OVER rounding. `revision` invalidates cached file pixels
when caller metadata changes. `clip` is a logical rectangle within the surface. The
painter caches at most 256 CPU tiles and 32 MiB, invalidating them when GTK
settings/providers change. It uploads only changed pixels. Call
`GtkPainterRelease(*painter)` before closing its session; release is idempotent.
Input, semantic labels, layout and file actions remain the application's
responsibility. This module requires an ordinary Kryon raster host.

Large surfaces use an exact, shared 64-pixel region atlas while retaining the
managed image limits. The CPU canvas is bounded to 16,777,216 pixels and 8192
pixels per axis; the atlas holds at most 1024 unique regions (4,194,304 pixels).
`GtkPainterEnd` returns false if allocation or the bounded unique-region budget
cannot represent the surface. Callers must handle that result.

Set `markup = true` to validate and render Pango markup, including styled spans,
bold text and escaped characters. Malformed markup fails without painting.
`GtkMarkupPlain(markup: string, output: []u8) -> s32` uses the same parser to
decode a plain label for accessibility. It returns the UTF-8 byte count excluding
the terminating NUL, or -1 for invalid input or insufficient space. Failure
leaves the output unchanged; success writes a terminating NUL. This helper
requires no display. Parsing performs no file, network or application actions.

Raster calls return an owning `GtkStyleBitmap` with physical `width`, `height`
and straight RGBA8 `rgba` bytes. Upload using `BitmapImageCreate`, then release
the CPU buffer with `GtkStyleBitmapRelease(*bitmap)`. Do not copy owning values.
Release is safe on an empty bitmap and resets its fields. Failure returns an
empty bitmap or metrics with `available = false`, including when no display is
available. Background color metrics alone cannot represent a CSS gradient;
use the raster call for its actual appearance.

Scale is 1–4. Style paths have 1–32 nodes and up to 32 classes per node;
selector strings and font descriptions are at most 255 bytes with no NUL.
Text must be valid UTF-8 without NUL and at most 65,536 bytes. Raster axes are
at most 4096 physical pixels with a 4,194,304-pixel total limit; icon edges are
at most 2048 physical pixels. Icon names cannot be paths.

### CSS providers, dark preference and invalidation

`GtkStyleProviderCreate(css: string) -> u64` parses CSS with GTK's native parser,
without installing it. Zero means failure. `GtkStyleProviderInstall(handle,
priority: u32) -> bool` installs that provider on this process's default GTK
screen. Repeating the same installation is a no-op. A provider can also be
attached to several node contexts at different priorities without screen
installation. `GtkStyleProviderReplace(handle, css) -> bool` parses a fresh
provider before replacing the active one. Parser errors, invalid lengths,
embedded NUL, input above 1,048,576 bytes, and recoverable allocation failures
leave the previous provider and generation unchanged. Empty CSS is valid. GTK/GLib's
own fatal allocation policy remains in effect. CSS may reference resources;
applications choose which CSS they trust.

`GtkStyleProviderRelease(handle) -> bool` removes its screen installation and
releases owned refs. Remove attachments using that handle before the next query;
a stale attachment rejects the whole path. Zero, stale and double release return
false. There are at most 64 live providers and 32 attachments per node. Handles
are process-local, never reused, and must not be persisted. Release works after
display environment variables change, without reinitializing GTK.

`GtkStyleDarkPreferencePush(value: bool) -> u64` temporarily sets this process's
`gtk-application-prefer-dark-theme`. `GtkStyleDarkPreferenceRestore(token) ->
bool` removes that scope. The newest live scope wins, even when older scopes
are restored out of order. The last restore reinstates the original value and
releases the settings reference; the next push captures a fresh original.
There are at most 64 live scopes. Zero/stale tokens fail without changing the
setting. These calls do not modify desktop configuration files or other
processes. Coordinate writes to this same property while scopes are active.

`GtkStyleGeneration() -> u64` changes immediately on successful provider
installation, replacement or release and effective preference changes. Creating
an unattached provider and failed calls do not change it. Cache query, raster,
text and icon results with this generation and the complete path (including
attachments, priorities and states). The bridge itself retains no result cache.
Native GTK settings notifications invalidate generation synchronously, without
retaining a settings owner. A default-settings identity change invalidates
on the next bridge call. Default icon-theme `changed` notifications also
invalidate generation. Unrelated GLib callbacks leave caches valid.
`GtkStylePoll()` returns nonzero once after API invalidation
as well as for delivered events. Call it before checking cached styles. All
provider/settings APIs share the GTK owning thread rule and fail safely without
a display. This opt-in Linux module adds no GTK dependency to portable, WASM,
Windows or Go application imports.
