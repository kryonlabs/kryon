# Kryon API

This reference covers 0.4.0 and Unreleased changes on master.

Kryon is an ordinary Ziran package. An application imports the modules it
uses; no widget names are recognized by the Ziran compiler or `.zx` loader.
The public package imports are declared in [`ziran.toml`](../ziran.toml).

Reusable component sources are grouped under `src/ui/<component>/`; shared
tree, input, theme, and paint infrastructure stays in `src/ui/`. Applications
import only the packages they use. For example, use `kryon/text`,
`kryon/button`, `kryon/button_group`, or `kryon/progress_ring`; checklist tasks
and swipe actions are available from `kryon/onboarding_checklist_task` and
`kryon/swipe_props`.

With `KRYON_HOSTED_UI`, the same named widget packages expose their hosted
contracts. Retained-only components such as Page and Toast require a local
tree. Hosted `kryon/menu` keeps the menu's props, types and state; its menu
bar `Draw` comes only with a local tree. Pointer and keyboard sampling remain
separate imports.

Rendering hosts that retain pixels can call `CommitFrameDamaged(session)` to
commit the tree and compare paint while leaving changed commands queued. An
unchanged result needs no raster work; a partial result supplies its damage
rectangle. The host clears and clips that area before `PaintFlushArea`, or
clears its whole surface before `PaintFlush`. `EndFrameDamaged` keeps its
existing behavior of emitting a full repaint immediately.

`frame_transition.FrameSurfaceCopy(surface)` copies a borrowed retained frame
into an owned surface before its target is reused. Capture before committing the destination
frame, draw with `FrameTransition`, and call `FrameSurfaceRelease` when the
transition ends. An invalid source or failed allocation returns an empty surface.

`TreeSubmittedCount(session)` and `TreeSubmittedNodeAt(session, index)` expose
the current frame's submitted geometry while building. Native skins can paint
retained controls from it while input continues using the committed tree.

| Import | Surface |
| --- | --- |
| `app` | `Window`, `Frame`, and `Run` for a small executable application |
| `text`, `button`, `image`, `page`, ... | One named package per widget, with `Props`, `Draw`, and widget-specific results |
| `semantic` | Semantic kinds for document-oriented applications |
| `PageRoute` | Browser route path, hash, and history (`GetRoutePath`, `PushRoute`, `GetRouteOrigin`, ...) for Canvas profiles |
| `geometry`, `drawing_props`, `session`, `tree`, `surface`, `paint_queue`, `style`, `style_sheet`, `control_props`, `font_metrics`, `scroll`, `semantic`, `tree_input`, `widget_kind` | Building blocks for widget packages |

## Where to look

| Topic | File |
| --- | --- |
| Widget packages catalog | [WIDGET_PACKAGES.md](WIDGET_PACKAGES.md) |
| Buttons, cards, menus, lists and other controls | [WIDGET_CONTROLS.md](WIDGET_CONTROLS.md) |
| Timed, retained and widget motion | [MOTION.md](MOTION.md) |
| Style rules, clipboard integration and drag source | [STYLE_RULES.md](STYLE_RULES.md) |
| GTK3 theme rendering on Linux | [GTK_THEME.md](GTK_THEME.md) |
| Image, tile map and shape rendering | [IMAGE_RENDERING.md](IMAGE_RENDERING.md) |
| Applications, window hosts and input delivery | [APPS.md](APPS.md) |
| Hosted widget packages (`KRYON_HOSTED_UI`) | [HOSTED_WIDGETS.md](HOSTED_WIDGETS.md) |
| Desktop ZX player, host parts, compiled canvas and Android frames | [APP_HOSTS.md](APP_HOSTS.md) |
| Game touch controls | [GAME_CONTROLS.md](GAME_CONTROLS.md) |
| Asset audio | [AUDIO.md](AUDIO.md) |
| Document persistence and local notifications | [PERSISTENCE.md](PERSISTENCE.md) |
| Scene materials, GLSL translation and GPU rendering | [SCENE_RENDERING.md](SCENE_RENDERING.md) |
| Vector paths, text pixels, light cookies, grades and watercolor | [PAINTING.md](PAINTING.md) |
| glTF 2.0 and VRM models | [MODELS.md](MODELS.md) |
| AVIF compression and RGBA previews | [BACKENDS.md](BACKENDS.md#avif-compression-and-rgba-previews) |
| Renderer performance benchmark | [PERFORMANCE.md](PERFORMANCE.md#renderer-performance-benchmark) |
| HTML and CSS mapping | [HTML_TO_KRYON.md](HTML_TO_KRYON.md) |
| Measured flex/grid layout and the native CSS subset | [HTML_TO_KRYON.md](HTML_TO_KRYON.md) |
| Runnable HTML and CSS example | [HTML_EXAMPLES.md](HTML_EXAMPLES.md) |

## Embedded Markdown documents

Import `kryon/markdown_editor` for a WYSIWYG Markdown text area, and
`kryon/markdown_images` for reusable image-reference scanning and insertion.
`markdown_images.Scan(text)` returns borrowed destinations, alt text and
`markdown_preview.ImageSpan` byte ranges, including inline images, escaped
labels, balanced URL parentheses and angle-bracket destinations. Code spans
and fenced code are excluded. `Embed(destination, alt)` creates an escaped
image line. The application owns uploads, authenticated downloads and decoded
assets; pass resolved spans in `MarkdownEditor.Props.images` and matching image
blocks in `area.blocks`. Unresolved references remain editable Markdown.

`MarkdownEditor.Props.folds` holds line-aligned source ranges with `start`,
`body_start`, `body_end`, `end` and `expanded`. A collapsed range occupies one
28-pixel disclosure row; an expanded range reveals its original body and images.
The app owns the disclosure buttons and state. `MarkdownEditor.Prepare(props)`
returns the same formatted `TextArea.Props` used by `Draw`, for measurement and
positioning controls. Folded content remains in the original value, so edits,
selection and copying retain document byte offsets.
