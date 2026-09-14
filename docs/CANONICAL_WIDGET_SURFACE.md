# Canonical Widget Surface

This is the shared review document for Kryon's public widget and node names.
It is meant to be easy to edit while we decide what stays, what gets renamed,
and what must move out of native code into `.kry`.

## Decision Rules

- One concept gets one public name.
- New public widget behavior starts in `.kry`.
- Native C, Go, JS, and C++ code should support generated `.kry` widgets, not
  define parallel user-facing widget APIs.
- Compatibility layers are migration debt to delete. Do not add new public
  compatibility names.
- User-facing names should be direct domain names, without project prefixes.
- Variants belong in props when they are the same concept.
- Breaking changes are allowed for the cleanup.

## Status Key

| Status | Meaning |
|---|---|
| `.kry canonical` | This should be implemented as a first-class `.kry` widget or node. |
| `.kry support` | Generated `.kry` policy/data used by a host-owned interaction or runtime helper, not a standalone public widget. |
| `Native canonical` | This is low-level drawing/layout/runtime surface that may remain native. |
| `Internal support` | Runtime support only; not a canonical public widget. |
| `Composite candidate` | Keep as `.kry` composition if it is reusable, otherwise fold into callers. |
| `Rename review` | Public name needs a naming decision. |
| `Removed` | Deleted from public headers/codegen/runtime surfaces. |

## Runtime `.kry` Modules

These modules already exist in `runtime/` and are part of the current widget
surface review:

| Module | Current role | Decision |
|---|---|---|
| `runtime/bevel.kry` | Bevel line geometry policy | `.kry canonical` |
| `runtime/button.kry` | Button composition, input, metrics, content, and fallback paint policy | `.kry canonical` |
| `runtime/button_props.kry` | Button props | `.kry canonical` |
| `runtime/canvas.kry` | Canvas transform and hit-test policy | `.kry canonical` |
| `runtime/canvas_props.kry` | Canvas props and result | `.kry canonical` |
| `runtime/canvas_grid.kry` | CanvasGrid line policy | `.kry canonical` |
| `runtime/card.kry` | Card composition | `.kry canonical` |
| `runtime/card_props.kry` | Card props | `.kry canonical` |
| `runtime/checkbox.kry` | Checkbox paint, row/text layout, and flag policy | `.kry canonical` |
| `runtime/checkbox_props.kry` | Checkbox props | `.kry canonical` |
| `runtime/collapsible.kry` | Collapsible metrics/header geometry, pointer, close, and keyboard policy | `.kry canonical` |
| `runtime/collapsible_props.kry` | Collapsible props | `.kry canonical` |
| `runtime/color_picker.kry` | ColorPicker channel, swatch, and color policy | `.kry canonical` |
| `runtime/color_picker_props.kry` | ColorPicker props | `.kry canonical` |
| `runtime/control_props.kry` | Shared control props | `.kry canonical` |
| `runtime/drawing_props.kry` | Shared drawing props and paragraph spec data | `.kry canonical` |
| `runtime/dropdown.kry` | Dropdown composition, option/index normalization, popup, row, scrollbar, keyboard intent, navigation, and indicator geometry policy | `.kry canonical` |
| `runtime/dropdown_props.kry` | Dropdown rich item data and props | `.kry canonical` |
| `runtime/drag_drop.kry` | DragDrop source/target lifecycle decision policy | `.kry canonical` |
| `runtime/drag_drop_props.kry` | DragDrop props and role enum | `.kry canonical` |
| `runtime/drag.kry` | Drag component layout, text paint geometry, typed keyboard input, and value/keyboard policy | `.kry canonical` |
| `runtime/drag_props.kry` | Drag props | `.kry canonical` |
| `runtime/grid.kry` | Grid composition | `.kry canonical` |
| `runtime/grid_props.kry` | Grid props | `.kry canonical` |
| `runtime/group.kry` | Group bounds/content policy | `.kry canonical` |
| `runtime/guide.kry` | Guide overlay layout, arrow geometry, and step policy | `.kry canonical` |
| `runtime/guide_pager.kry` | Internal pager footer layout and page transition policy | `.kry support` |
| `runtime/icon.kry` | Icon bounds/size policy | `.kry canonical` |
| `runtime/image.kry` | Image fit and placeholder layout policy | `.kry canonical` |
| `runtime/image_props.kry` | Image props | `.kry canonical` |
| `runtime/input.kry` | Input step-button default, component layout, value/step, and temp-edit activation policy | `.kry canonical` |
| `runtime/input_props.kry` | Input props and shared numeric value kind | `.kry canonical` |
| `runtime/instance.kry` | Generated widget instance identity helpers | `.kry support` |
| `runtime/inspect.kry` | Inspector edit/resize geometry and handle-size policy | `.kry support` |
| `runtime/fieldset.kry` | Fieldset layout/paint policy | `.kry canonical` |
| `runtime/fieldset_props.kry` | Fieldset props | `.kry canonical` |
| `runtime/focus.kry` | Focus ring, activation, and debug overlay geometry policy | `.kry support` |
| `runtime/layout.kry` | Column/Row/Stack content and child placement policy | `.kry canonical` |
| `runtime/layout_props.kry` | Column/Row/Flow layout props | `.kry canonical` |
| `runtime/link.kry` | Link bounds, interaction, activation, state, and color policy | `.kry canonical` |
| `runtime/link_props.kry` | Link props | `.kry canonical` |
| `runtime/list_box.kry` | ListBox layout/navigation and row paint geometry policy | `.kry canonical` |
| `runtime/list_box_props.kry` | ListBox props | `.kry canonical` |
| `runtime/material.kry` | Material layer assembly with typed `MaterialKind` policy | `.kry canonical` |
| `runtime/menu.kry` | Menu metrics, geometry, selectable/keyboard navigation, bar open/index policy, and group pointer open/close decisions | `.kry canonical` |
| `runtime/menu_props.kry` | Menu item/group/result data and props | `.kry canonical` |
| `runtime/list_box_multi.kry` | ListBox multi-selection row/keyboard navigation/selection policy | `.kry canonical` |
| `runtime/navigation_bar.kry` | Navigation bar default-height, item interaction, paint, and configuration layout/count/default policy | `.kry canonical` |
| `runtime/navigation_bar_props.kry` | NavigationBar props and result | `.kry canonical` |
| `runtime/node2d_props.kry` | Game2D scene/node declaration props, defaults, node props, and enums | `.kry canonical` |
| `runtime/node_registry_props.kry` | Public node registry flags | `.kry support` |
| `runtime/scene_tree_props.kry` | Game2D scene node kind and flag values | `.kry support` |
| `runtime/paint.kry` | Paint/drawing helpers | `.kry support` |
| `runtime/paned_view.kry` | PanedView split/layout/handle geometry, drag lifecycle, and change policy | `.kry canonical` |
| `runtime/paned_view_props.kry` | PanedView props | `.kry canonical` |
| `runtime/page.kry` | Page, Section, Heading, ParagraphText, and Flow composition | `.kry canonical` |
| `runtime/page_props.kry` | Page/Section/Heading/Paragraph props | `.kry canonical` |
| `runtime/paragraph.kry` | Paragraph metrics/default line-gap, layout spacing, line-step, height, line stride, alignment, and selectable line-index/local-offset policy | `.kry canonical` |
| `runtime/plot.kry` | Plot geometry and text policy | `.kry canonical` |
| `runtime/plot_props.kry` | Plot props and mode names | `.kry canonical` |
| `runtime/popup_policy.kry` | Popup mode/input record/Escape-close policy | `.kry canonical` |
| `runtime/popup_props.kry` | Popup props | `.kry canonical` |
| `runtime/primitive.kry` | Background/Box/Line/Circle/Ring/Triangle primitive geometry policy | `.kry canonical` |
| `runtime/profile_header.kry` | Profile header geometry/text placement plus profile image picker geometry and selection/click policy | `.kry support` |
| `runtime/progress.kry` | Progress layout policy | `.kry canonical` |
| `runtime/progress_props.kry` | Progress props | `.kry canonical` |
| `runtime/radio.kry` | Radio paint/layout policy | `.kry canonical` |
| `runtime/radio_props.kry` | Radio props | `.kry canonical` |
| `runtime/reorder.kry` | Reorder metrics, handle/placeholder paint geometry, target-index, lifecycle gates, and result normalization policy | `.kry support` |
| `runtime/reorder_props.kry` | Reorder support state, data, and result records | `.kry support` |
| `runtime/router_props.kry` | Router routes, state, props, and result | `.kry canonical` |
| `runtime/rows.kry` | Info/form/button row sizing, wrapping, and layout fallback policy | `.kry canonical` |
| `runtime/segmented_control.kry` | SegmentedControl layout, gap, font fallback, wrapping, and segment sizing policy | `.kry canonical` |
| `runtime/segmented_control_props.kry` | SegmentedControl props and result | `.kry canonical` |
| `runtime/selectable.kry` | Selectable paint/layout and toggle policy | `.kry canonical` |
| `runtime/selectable_props.kry` | Selectable props | `.kry canonical` |
| `runtime/separator.kry` | Separator/Bullet layout and paint policy | `.kry canonical` |
| `runtime/separator_props.kry` | Separator props | `.kry canonical` |
| `runtime/slider.kry` | Slider composition, component/editor/hit layout, text paint geometry, and value/keyboard policy | `.kry canonical` |
| `runtime/slider_props.kry` | Slider props | `.kry canonical` |
| `runtime/spinbox.kry` | Spinbox button-width default, layout, and value policy | `.kry canonical` |
| `runtime/spinbox_props.kry` | Spinbox props | `.kry canonical` |
| `runtime/scroll.kry` | Scroll measurement, sizing, wheel, content-drag decision, thumb drag offset, scrollbar drag decision, and ensure-visible policy | `.kry canonical` |
| `runtime/scroll_props.kry` | Scroll props | `.kry canonical` |
| `runtime/style.kry` | Style helpers | `.kry canonical` |
| `runtime/style_picker_props.kry` | StylePicker props and option/selection/dropdown state policy | `.kry canonical` |
| `runtime/style_sheet.kry` | Style sheet evaluation helpers | `.kry canonical` |
| `runtime/surface.kry` | Surface/container helpers | `.kry canonical` |
| `runtime/swipe.kry` | Swipe begin, drag, release, lifecycle, direction, default, and progress policy | `.kry support` |
| `runtime/swipe_props.kry` | Swipe support state, data, and result records | `.kry support` |
| `runtime/tab_bar.kry` | TabBar sizing, scroll, keyboard index, reorder marker/drag lifecycle, and double-click decision policy | `.kry canonical` |
| `runtime/tab_bar_props.kry` | TabBar props | `.kry canonical` |
| `runtime/terminal_pane.kry` | TerminalPane font, content, grid clamp, and scroll indicator metrics policy | `.kry support` |
| `runtime/text.kry` | Text composition, selectable highlight geometry, and double-click line-selection policy | `.kry canonical` |
| `runtime/text_props.kry` | Text props | `.kry canonical` |
| `runtime/text_input.kry` | TextField/TextArea defaults, metrics, scroll, wrap thresholds, caret/IME stroke metrics, paint geometry, buffer-limit, cursor normalization, navigation, selection state/paint-span policy, double-click/pan decisions, text-buffer mutation/range/bracket policy, focus ownership, platform text-input sync, and edit-intent policy | `.kry canonical` |
| `runtime/text_input_props.kry` | TextField/TextArea props and text input style enums | `.kry canonical` |
| `runtime/theme.kry` | Theme data/helpers and typed `ThemePolicy` resolution | `.kry canonical` |
| `runtime/title_bar.kry` | TitleBar effective state, layout, reservation, and paint geometry policy | `.kry canonical` |
| `runtime/title_bar_props.kry` | TitleBar props | `.kry canonical` |
| `runtime/toggle.kry` | Toggle composition | `.kry canonical` |
| `runtime/toggle_props.kry` | Toggle props | `.kry canonical` |
| `runtime/toolbar.kry` | Toolbar, bottom icon row, and icon slider popup metrics/geometry/style-size/close policy | `.kry canonical` |
| `runtime/toolbar_props.kry` | Toolbar and bottom icon row props/results | `.kry canonical` |
| `runtime/toast.kry` | Toast request/render decision, duration, layout, text-placement, and truncation policy | `.kry canonical` |
| `runtime/toast_props.kry` | Toast props | `.kry canonical` |
| `runtime/transition_fade.kry` | Transition fade alpha/easing policy | `.kry support` |
| `runtime/transition_props.kry` | Transition phase enum names | `.kry support` |
| `runtime/modal.kry` | Modal layout, frame geometry, message line-gap, outside-dismissal, prompt availability/focus fallback, prompt input/result, and action row policy | `.kry canonical` |
| `runtime/modal_props.kry` | Modal props and action props | `.kry canonical` |
| `runtime/overlay.kry` | Internal dismissible overlay viewport and dismissal policy | `.kry support` |
| `runtime/tree_view.kry` | TreeView row/window, paint geometry, marker text, and row-selection decision policy | `.kry canonical` |
| `runtime/tree_view_props.kry` | TreeView props | `.kry canonical` |
| `runtime/table_view.kry` | TableView layout, scroll, scrollbar, cell geometry, header/row pointer decisions, keyboard selection, activation, clear-selection, resize lifecycle/width, and clipboard intent policy | `.kry canonical` |
| `runtime/table_view_props.kry` | TableView row and props | `.kry canonical` |
| `runtime/widget_kind.kry` | Retained tree node kind and internal flag values | `.kry support` |

## Current Implementation Audit

This is the current migration truth, not the desired final state. A name in the
registry is only considered `.kry`-backed when its reusable behavior, props, or
layout policy lives in `runtime/*.kry` and native code only adapts host input,
text measurement, painting, storage, or platform services.

| Group | `.kry`-backed today | Still native-only or compatibility |
|---|---|---|
| Text and drawing | `Text` style resolution, centered row text placement, selectable range normalization/highlight geometry/double-click line-selection policy, `Paragraph` metrics/default line-gap/layout spacing/line-step/height/line-stride/alignment/selectable line-index/local-offset policy, `ParagraphSpec` generated data, `Background`/`Box`/`Line`/`Circle`/`Ring`/`Triangle` geometry policy, `Bevel` line geometry, `Icon` bounds/size policy, `Image` canonical props/name and placeholder layout, clean drawing primitive names (`Box`, `Circle`, `Ring`, `Triangle`) | icon sheet/drawing host support, paragraph parsing/line storage/drawing, selectable text ownership/drawing |
| Actions | `Button`, `Card`, `Link`, `Button` menu/split/arrow/info options; button fallback/terminal paint constants | helper button variants belong in `ButtonProps` or composition; invisible hit testing and rasterization are host support |
| Inputs | `Checkbox` paint/row/text/flag policy, `Dropdown` option/index normalization, popup/row/scrollbar/keyboard-intent/navigation/indicator policy, `DropdownOption`, `Drag` component layout/text paint/value policy, `Input` step-button default, component/step-button layout, value, and temp-edit activation policy, `Progress`, `Radio`, `SegmentedControl`, `Selectable`, `Slider` component/editor/hit layout, text paint geometry, and value/keyboard policy, `Spinbox` button-width/layout/value policy, `TextField`/`TextArea` defaults/metrics/paint geometry/buffer-limit/cursor normalization/navigation/edit intent/selection state/paint-span/double-click/pan/focus/text-buffer/range decision policy, `Toggle`, `Button` swatch props, `ColorPicker` layout/swatch/color policy | text composition, raw string storage/memmove/scanning, and platform text services |
| Layout | `Column`/`Row`/`Stack` content and child placement policy, `Group` bounds/content policy, `Screen` viewport fallback bounds policy, `Grid`, `Fieldset` layout policy, `PanedView` split/layout/change geometry and drag lifecycle policy, `Collapsible` header geometry plus pointer/close/keyboard decisions, `Separator`, `Scroll` measurement/sizing/wheel/content-drag/scrollbar-drag decision/thumb-drag/ensure-visible/clip geometry policy, shared `Surface`/`Style`/`Material` policy, `Reorder` metrics/handle geometry/placeholder paint geometry/target-index/lifecycle gate/result policy, `ReorderState`/`ReorderItem`/`ReorderList`/`ReorderListResult` generated support records | scroll/list/table begin-end wrappers; scroll pointer ownership storage and reorder pointer ownership storage remain host support |
| Collections | `Canvas` transform/hit-test policy, `CanvasGrid`, drag/drop source/target lifecycle decision policy, `ListBox` layout/navigation/row paint geometry/multi-selection policy, `Plot` geometry/mode/text policy, `TreeView` row/window/paint geometry, marker text, and row-selection decision policy, `TableView` layout/scroll/scrollbar/cell geometry, header/row pointer decisions, keyboard selection, activation, clear-selection, resize lifecycle/width, and clipboard intent policy | drag/drop payload storage |
| Navigation | `NavigationBar` default-height variant, item interaction, paint/config layout/count/default policy, `TabBar` sizing/scroll/keyboard intent/keyboard-index/reorder marker/drag lifecycle/double-click decision policy, `Toolbar`, bottom icon row, and icon slider popup metrics/geometry/close policy, `TitleBar` effective state/layout/reservation/paint geometry policy, `Menu` geometry/keyboard navigation and group pointer open/close decision policy, `MenuItem`/`MenuGroup`/`MenuResult` data | retained menu open/focus/input state, router/link helpers |
| Overlays | `Popup` mode/input/Escape-close policy, internal dismissible-overlay viewport/dismissal policy, `Focus` ring geometry and keyboard activation policy, `Guide` overlay layout/arrow/step/keyboard-input policy, guide pager layout/page/keyboard-input policy, swipe begin/drag/release/lifecycle decision policy, `SwipeGesture`/`SwipeSpec`/`SwipeResult` generated pager support records, `Modal` layout/frame/outside-dismissal/prompt availability/focus fallback/prompt-input/result/action policy, `Toast` request/render decision, duration/layout/text-placement/truncation policy, transition fade alpha/easing policy, `StylePicker` public props and option/selection/dropdown state policy, profile header geometry/text placement/click policy, profile image picker geometry/selection policy, inspector edit/resize geometry and handle-size policy | theme picker, inspector state/input, and profile image rendering/input host support; swipe pointer ownership storage remains host support |
| Terminal | `TerminalPane` font fallback, content bounds, grid clamp, and scroll indicator metrics policy | terminal emulator state, PTY/session IO, ANSI parsing, text measurement, clipboard/selection, input sampling, and drawing remain native terminal host support |
| Game2D | `Scene`, `Node2D`, `Camera2D`, `Sprite2D`, `AnimatedSprite2D`, `TileMap`, `CollisionShape2D`, `Area2D`, `Body2D`, `AnimationPlayer`, `AudioSource`, and `Light2D` public props/enums/defaults; `NodeKind*` and `NodeFlag*` support values | Scene ownership, lifecycle, physics/audio handles, rendering, and runtime node mutation remain native Game2D support. |

The remaining migration target is the native support around text editing and
content wrappers: `TextField` and `TextArea` own defaults/metrics/wrap/caret
stroke/paint geometry/buffer-limit/cursor normalization/navigation/edit-intent, focus/platform
text-input sync, text-buffer mutation/range/bracket decisions, and selection
range/movement/collapse/select-all and selection paint-span policy in `.kry`,
but raw string storage/memmove/scanning, IME, selection ownership/drawing, and
rich text reflow/rendering remain host work.

## Registry Surface Audit

This table is the authoritative shared list of public node names from
`src/ui/ui_node_registry.c`. Keep one row per registry name so rename feedback
has a single place to land.

| Public name | Registry group | Detail | Runtime `.kry` source | State | Migration note |
|---|---|---|---|---|---|
| `Background` | `Display` | Fill | `runtime/primitive.kry` | `.kry-backed` | Viewport bounds and app fallback policy are `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `Display` | Label | `runtime/text.kry`, `runtime/text_input.kry` | `.kry-backed` | Keep one `Text(TextProps)` surface; retained tree typography uses resolved KSS font sizes directly; selectable range normalization uses shared `.kry` text-input policy, and selectable highlight/double-click line-selection policy is `.kry`. |
| `Paragraph` | `Display` | Rich text | `runtime/paragraph.kry`, `runtime/drawing_props.kry`, `runtime/text.kry`, `runtime/text_input.kry` | Partly `.kry-backed` | Metrics/default line-gap, layout spacing, line-step, height, line-stride, alignment, selectable line-index/local-offset and double-click line-selection policy, `ParagraphSpec` data, and selectable range normalization are `.kry`; text parsing, line-break array ownership, icon shaping, selection ownership/drawing, and drawing remain host support. |
| `Box` | `Display` | Shape | `runtime/primitive.kry` | `.kry-backed` | Rectangle bounds policy is `.kry`; host keeps fill/border drawing. |
| `Line` | `Display` | Stroke | `runtime/primitive.kry` | `.kry-backed` | Endpoint and retained-bounds policy is `.kry`; host keeps stroke drawing. |
| `Bevel` | `Display` | Relief | `runtime/bevel.kry` | `.kry-backed` | Line geometry is `.kry`; still review whether it should fold into `Surface`/material props. |
| `Icon` | `Display` | Icon | `runtime/icon.kry` | Partly `.kry-backed` | Bounds/size policy is `.kry`; icon sheet/type lookup and drawing remain host support. |
| `Image` | `Display` | Image | `runtime/image.kry` | Partly `.kry-backed` | Fit and placeholder layout policy are `.kry`; placeholder typography uses resolved KSS font sizes directly; cache/loading/drawing remain host support. |
| `Card` | `Input` | Surface action | `runtime/card.kry`, `runtime/card_props.kry` | `.kry-backed` | Card composition and props live in `.kry`. |
| `Button` | `Input` | Action | `runtime/button.kry`, `runtime/button_props.kry` | `.kry-backed` | Single button surface; menu/split/info/icon variants are props/composition; retained and immediate typography defaults plus fallback/terminal paint policy are `.kry`/KSS-owned. |
| `Link` | `Input` | Link | `runtime/link.kry` | Partly `.kry-backed` | Bounds, interaction, activation, state, and color policy are `.kry`; URL dispatch remains host support. |
| `TextField` | `Input` | Input | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics, scroll, paint geometry, buffer-limit, cursor normalization, navigation, edit intent, double-click/pan/focus decisions, text-buffer mutation/range/bracket policy, and selection range/movement/collapse/select-all/paint-span policy are `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership, and paint still native. |
| `Dropdown` | `Input` | Selection | `runtime/dropdown.kry`, `runtime/dropdown_props.kry` | `.kry-backed` | Selection-only control; option/index normalization, popup placement, row/window, scrollbar, scrolling, keyboard intent, navigation, indicator geometry, and rich option data are generated from `.kry`. |
| `Slider` | `Input` | Value | `runtime/slider.kry` | `.kry-backed` | Value type, orientation, angle/unit, component/editor/hit layout, and text paint geometry are props/policy; label/value typography is KSS-owned. |
| `Toggle` | `Input` | On/off | `runtime/toggle.kry` | `.kry-backed` | Host handles input and drawing; paint/layout policy is `.kry`. |
| `Checkbox` | `Input` | Boolean | `runtime/checkbox.kry` | `.kry-backed` | Paint, row/text layout, and flag policy are `.kry`; box, mark, and label roles are KSS-owned. |
| `Radio` | `Input` | Choice | `runtime/radio.kry` | `.kry-backed` | Paint, layout, and marker text policy are `.kry`; host keeps group input. |
| `Progress` | `Input` | Progress | `runtime/progress.kry` | `.kry-backed` | One public progress concept. |
| `Spinbox` | `Input` | Number | `runtime/spinbox.kry` | `.kry-backed` | Layout/step policy is `.kry`; host keeps text/button input. |
| `ColorPicker` | `Input` | Color | `runtime/color_picker.kry` | `.kry-backed` | Channel layout and conversion are `.kry`. |
| `SegmentedControl` | `Input` | Segments | `runtime/segmented_control.kry` | `.kry-backed` | Layout, gap, font fallback, wrapping, and segment sizing policy are `.kry`; host keeps label measurement, input sampling, and button drawing. |
| `Group` | `Layout` | Container | `runtime/group.kry` | `.kry-backed` | Canonical non-layout grouping scope; bounds/content policy is `.kry`, host keeps retained tree scope ownership. |
| `Separator` | `Layout` | Divider | `runtime/separator.kry` | `.kry-backed` | Line, label, and bullet policy are `.kry`. |
| `Fieldset` | `Layout` | Frame | `runtime/fieldset.kry` | `.kry-backed` | Titled group and border policy are `.kry`. |
| `PanedView` | `Layout` | Split panes | `runtime/paned_view.kry` | Partly `.kry-backed` | Split clamp, layout, handle geometry, pointer split, drag lifecycle, and change policy are `.kry`; host keeps active split pointer storage and popup input owner binding. |
| `Collapsible` | `Layout` | Section | `runtime/collapsible.kry` | Partly `.kry-backed` | Header metrics, geometry, marker text, pointer/body toggle, close, keyboard open, and tree focus-routing policy are `.kry`/KSS-owned; host keeps input sampling, focus application, and drawing. |
| `ListBox` | `Collections` | List | `runtime/list_box.kry` | `.kry-backed` | Layout/navigation and row paint geometry policy is `.kry`; host keeps input/scroll sampling. |
| `TreeView` | `Collections` | Tree | `runtime/tree_view.kry` | Partly `.kry-backed` | Row, indent, scroll-window, marker text, text bounds, paint geometry, and row-selection decision policy are `.kry`; item typography defaults are KSS-owned; host keeps input sampling, selected-id storage, expansion state, and drawing. |
| `TableView` | `Collections` | Table | `runtime/table_view.kry` | Partly `.kry-backed` | Header/body/frozen-row/scroll/scrollbar/cell geometry, header/row pointer decisions, keyboard selection, activation, clear-selection, resize lifecycle/width, and clipboard intent policy are `.kry`; host keeps column ordering, input sampling, stored selection pointers, resize pointer ownership, clipboard IO, and drawing. |
| `TextArea` | `Collections` | Text area | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics, page-navigation rows, paint geometry, buffer-limit, cursor normalization, navigation, edit intent, double-click/pan/focus decisions, text-buffer mutation/range/bracket policy, and selection range/movement/collapse/select-all/paint-span policy are `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership, and paint still native. |
| `CanvasGrid` | `Collections` | Grid | `runtime/canvas_grid.kry` | `.kry-backed` | Grid spacing and line geometry are `.kry`; host draws. |
| `Menu` | `Navigation` | Menu | `runtime/menu.kry`, `runtime/menu_props.kry` | `.kry canonical` | Command menu surface; item/group/result data and bar, popup, and context behavior props are generated from `.kry`. |
| `NavigationBar` | `Navigation` | Tabs | `runtime/navigation_bar.kry` | `.kry-backed` | Item interaction/state, paint, sizing, and configuration modal layout/count/default policy are `.kry`. |
| `Toolbar` | `Navigation` | Tools | `runtime/toolbar.kry` | `.kry-backed` | Metrics/geometry/style-size and icon slider popup close policy are `.kry`; host dispatches child actions. |
| `TabBar` | `Navigation` | Tabs | `runtime/tab_bar.kry` | `.kry-backed` | Sizing, scroll, keyboard index, reorder marker/drag lifecycle, and double-click decision policy are `.kry`; host keeps input sampling and state storage. |
| `TitleBar` | `Navigation` | Title | `runtime/title_bar.kry` | Partly `.kry-backed` | Effective height/state, layout, paint geometry, and title font-fit policy are `.kry`; host keeps dropdown dispatch, text measurement, and leading-action input/rendering. |
| `Focus` | `Overlays` | Focus | `runtime/focus.kry` | Partly `.kry-backed` | Ring geometry and keyboard activation policy are `.kry`; host keeps focus state, registration, key sampling, popup capture lookup, and drawing. |
| `Modal` | `Overlays` | Dialog | `runtime/modal.kry` | Partly `.kry-backed` | Layout, frame geometry, message line-gap, outside-dismissal, prompt availability/focus fallback/input/result, and action sizing/row policy are `.kry`; host keeps capture application, release consumption, text editing, and drawing. |
| `Scene` | `Game2D/Core` | Scene root | `runtime/node2d_props.kry`, `runtime/scene_tree_props.kry` | `.kry props, native scene` | Public declaration props/defaults and kind values are `.kry`; scene ownership, lifecycle, physics world, and rendering remain native Game2D support. |
| `Node2D` | `Game2D/Core` | Transform | `runtime/node2d_props.kry`, `runtime/scene_tree_props.kry` | `.kry props, native scene` | Public transform declaration props/defaults and kind values are `.kry`; runtime tree mutation and world transform propagation remain native. |
| `Camera2D` | `Game2D/Core` | Camera | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; scene lifecycle and camera activation remain native Game2D support. |
| `Sprite2D` | `Game2D/Rendering` | Image | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; asset loading and drawing remain native Game2D support. |
| `AnimatedSprite2D` | `Game2D/Rendering` | Animation | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; frame advance and drawing remain native Game2D support. |
| `TileMap` | `Game2D/Rendering` | Tiles | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; tile iteration and drawing remain native Game2D support. |
| `CollisionShape2D` | `Game2D/Physics` | Collider | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; physics shape creation remains native Game2D support. |
| `Area2D` | `Game2D/Physics` | Trigger | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; overlap tracking remains native Game2D support. |
| `Body2D` | `Game2D/Physics` | Body | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; Box2D body ownership remains native Game2D support. |
| `AnimationPlayer` | `Game2D/Runtime` | Animation | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; animation playback/lifecycle remain native Game2D support. |
| `AudioSource` | `Game2D/Audio` | Sound | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; playback handle ownership remains native Game2D support. |
| `Light2D` | `Game2D/Rendering` | Point light | `runtime/node2d_props.kry` | Partly `.kry-backed` | Public props are generated from `.kry`; light rendering remains native Game2D support. |

## Parser Statement Surface

These names are the current standalone widget-call whitelist in
`cmd/kir/kir_parse.c`. This table is intentionally separate from the retained
node registry because many entries are commands, lexical-block companions, or
host roles rather than retained nodes.

| Parser name | Current decision | Notes |
|---|---|---|
| `AppBackground` | `.kry canonical` | App chrome background command backed by active style facts; preferred over spelling out `Background(GetThemeBackground())`. |
| `Background` | `.kry canonical` | Fill/display widget; geometry policy is in `.kry`. |
| `Text` | `.kry canonical` | Canonical text surface. |
| `Paragraph` | `.kry canonical` | Rich text surface. Plain labels stay `Text`; paragraph flow, icons, wrapping metrics, and rich text policy stay here. |
| `Abbreviation` | `.kry canonical` | Web-native abbreviation element. |
| `Address` | `.kry canonical` | Web-native contact/address content element. |
| `Area` | `.kry canonical` | Web-native image-map area element. |
| `Article` | `.kry canonical` | Web-native article content region. |
| `Aside` | `.kry canonical` | Web-native aside/related-content region. |
| `Base` | `.kry canonical` | Web-native document base URL element. |
| `Box` | `.kry canonical` | Canonical rectangle primitive with `Rectangle` bounds. |
| `Line` | `.kry canonical` | Endpoint and retained-bounds policy are in `.kry`; host keeps stroke drawing. |
| `Bevel` | Native canonical | Drawing effect unless material/surface props absorb it. |
| `Icon` | `.kry canonical` | Icon bounds/size policy is in `.kry`. |
| `Image` | `.kry canonical` | Canonical image widget; replaces old picture naming. |
| `Button` | `.kry canonical` | One button surface; variants belong in props/composition. |
| `Card` | `.kry canonical` | Surface action/card composition. |
| `Selectable` | `.kry canonical` | Selectable row/action surface. |
| `Audio` | `.kry canonical` | Web-native audio media element. |
| `BidirectionalIsolate` | `.kry canonical` | Web-native bidirectional-isolate phrasing element. |
| `BidirectionalOverride` | `.kry canonical` | Web-native bidirectional-override phrasing element. |
| `BlockQuote` | `.kry canonical` | Web-native block quotation element. |
| `Bold` | `.kry canonical` | Web-native bold phrasing element. |
| `Cite` | `.kry canonical` | Web-native citation phrasing element. |
| `Code` | `.kry canonical` | Web-native inline code element. |
| `CodeBlock` | `.kry canonical` | Web-native preformatted code block. |
| `Data` | `.kry canonical` | Web-native machine-readable data element. |
| `DataList` | `.kry canonical` | Web-native datalist suggestion container. |
| `Deleted` | `.kry canonical` | Web-native deleted-text phrasing element. |
| `DescriptionDetails` | `.kry canonical` | Web-native description details element. |
| `DescriptionList` | `.kry canonical` | Web-native description list element. |
| `DescriptionTerm` | `.kry canonical` | Web-native description term element. |
| `Details` | `.kry canonical` | Web-native disclosure details element. |
| `Dialog` | `.kry canonical` | Web-native dialog element. |
| `Embed` | `.kry canonical` | Web-native embedded external content element. |
| `Emphasis` | `.kry canonical` | Web-native emphasis phrasing element. |
| `Figcaption` | `.kry canonical` | Web-native figure caption element. |
| `Figure` | `.kry canonical` | Web-native figure element. |
| `Footer` | `.kry canonical` | Web-native footer landmark/content element. |
| `Form` | `.kry canonical` | Web-native form element. |
| `Header` | `.kry canonical` | Web-native header landmark/content element. |
| `HGroup` | `.kry canonical` | Web-native heading-group element. |
| `IFrame` | `.kry canonical` | Web-native iframe element. |
| `ImageMap` | `.kry canonical` | Web-native image map element. |
| `Inserted` | `.kry canonical` | Web-native inserted-text phrasing element. |
| `Italic` | `.kry canonical` | Web-native italic phrasing element. |
| `Keyboard` | `.kry canonical` | Web-native keyboard-input phrasing element. |
| `Label` | `.kry canonical` | Web-native form label element. |
| `Legend` | `.kry canonical` | Web-native fieldset legend element; authored legends participate in legend relations and are not separate Go app widgets. |
| `LineBreak` | `.kry canonical` | Web-native line-break element. |
| `ListItem` | `.kry canonical` | Web-native list item element. |
| `Main` | `.kry canonical` | Web-native main landmark element. |
| `Mark` | `.kry canonical` | Web-native marked/highlighted text element. |
| `Meta` | `.kry canonical` | Web-native document metadata element. |
| `Meter` | `.kry canonical` | Web-native scalar meter element. |
| `Navigation` | `.kry canonical` | Web-native navigation landmark element. |
| `NoScript` | `.kry canonical` | Web-native fallback content element for script-disabled browsers. |
| `EmbeddedObject` | `.kry canonical` | Web-native embedded object element. |
| `OrderedList` | `.kry canonical` | Web-native ordered list element. |
| `OptionGroup` | `.kry canonical` | Web-native grouped select options element. |
| `Option` | `.kry canonical` | Web-native select option element. |
| `Output` | `.kry canonical` | Web-native calculation output element. |
| `Param` | `.kry canonical` | Web-native object parameter element. |
| `Pre` | `.kry canonical` | Web-native preformatted text element. |
| `Quote` | `.kry canonical` | Web-native inline quote element. |
| `Ruby` | `.kry canonical` | Web-native ruby annotation container element. |
| `RubyParenthesis` | `.kry canonical` | Web-native ruby fallback parenthesis element. |
| `RubyText` | `.kry canonical` | Web-native ruby text annotation element. |
| `Sample` | `.kry canonical` | Web-native sample-output phrasing element. |
| `Script` | `.kry canonical` | Web-native script/data element; app logic still belongs in generated JS. |
| `Search` | `.kry canonical` | Web-native search landmark element. |
| `Select` | `.kry canonical` | Web-native select element; app selection control remains `Dropdown`. |
| `Slot` | `.kry canonical` | Web-native shadow DOM slot element. |
| `Small` | `.kry canonical` | Web-native side-comment/small text element. |
| `Source` | `.kry canonical` | Web-native media source element. |
| `Strong` | `.kry canonical` | Web-native strong-importance phrasing element. |
| `StyleElement` | `.kry canonical` | Web-native style element; KSS remains the primary styling authoring path. |
| `Subscript` | `.kry canonical` | Web-native subscript phrasing element. |
| `Summary` | `.kry canonical` | Web-native summary element for `Details`. |
| `Superscript` | `.kry canonical` | Web-native superscript phrasing element. |
| `Table` | `.kry canonical` | Web-native table element. |
| `TableBody` | `.kry canonical` | Web-native table body element. |
| `TableCaption` | `.kry canonical` | Web-native table caption element. |
| `TableColumn` | `.kry canonical` | Web-native table column element. |
| `TableColumnGroup` | `.kry canonical` | Web-native table column group element. |
| `TableFoot` | `.kry canonical` | Web-native table footer element. |
| `TableHead` | `.kry canonical` | Web-native table head element. |
| `TableRow` | `.kry canonical` | Web-native table row element. |
| `Template` | `.kry canonical` | Web-native inert template element. |
| `Title` | `.kry canonical` | Web-native document title element. |
| `Time` | `.kry canonical` | Web-native time element. |
| `Track` | `.kry canonical` | Web-native media text track element. |
| `UnorderedList` | `.kry canonical` | Web-native unordered list element. |
| `Variable` | `.kry canonical` | Web-native variable phrasing element. |
| `Video` | `.kry canonical` | Web-native video media element. |
| `WordBreakOpportunity` | `.kry canonical` | Web-native word-break opportunity element. |
| `Bullet` | `.kry canonical` | Small list/text marker primitive. |
| `Separator` | `.kry canonical` | Divider primitive. |
| `Link` | `.kry canonical` | Canonical link activation name. |
| `TextField` | `.kry canonical` | Metrics, KSS typography defaults, scroll, paint geometry, buffer-limit, navigation, focus/platform text-input sync, text-buffer mutation/range/bracket policy, selection state, and edit intent policy in `.kry`; raw string storage/memmove/scanning and IME host support remain. |
| `TextArea` | `.kry canonical` | Metrics, KSS typography defaults, page-navigation, paint geometry, buffer-limit, navigation, focus/platform text-input sync, text-buffer mutation/range/bracket policy, selection state, and edit intent policy in `.kry`; raw string storage/memmove/scanning and IME host support remain. |
| `Dropdown` | `.kry canonical` | Selection control only; `DropdownOption` is generated data for rich options, not a separate widget. |
| `SegmentedControl` | `.kry canonical` | Segmented choice control; layout/wrapping policy is in `.kry`, generated Go uses `kr.SegmentedControl`. |
| `Slider` | `.kry canonical` | Type/orientation/angle variants, component/editor/hit layout, and text paint geometry are props/policy; label/value typography is KSS-owned. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props. `MenuItem`, `MenuGroup`, and `MenuResult` are generated data/result records, not separate widgets. KSS uses `Menu`, `MenuItem`, and `MenuSeparator`; item typography participates in popup sizing; no `MenuBar` selector. |
| `Toggle` | `.kry canonical` | Boolean switch. |
| `Checkbox` | `.kry canonical` | Boolean checkbox. |
| `Radio` | `.kry canonical` | Choice control. |
| `Progress` | `.kry canonical` | One progress concept. |
| `Plot` | `.kry canonical` | Public props and mode names live in `runtime/plot_props.kry`; plot geometry/text policy lives in `.kry`. |
| `Drag` | `.kry canonical` | Numeric drag value control; value type/count, component layout, and text paint geometry are props/policy. |
| `Input` | `.kry canonical` | Numeric input control; value type/count, component/step-button layout, and temp-edit activation are props/policy. |
| `Spinbox` | `.kry canonical` | Numeric stepper; value typography is `SpinboxValue`, step controls use `Button`. |
| `DragDrop` | `.kry canonical` | Typed source/target roles are selected through props. |
| `ListBox` multi-selection | `.kry canonical` | Use `ListBoxProps.selected`, `selected_count`, and `anchor`; no separate public widget name. |
| `Screen` | `.kry canonical` | Top-level screen container. |
| `Page` | `.kry canonical` | Top-level generated page/document surface. |
| `Section` | `.kry canonical` | Semantic page section container. |
| `Heading` | `.kry canonical` | Semantic heading backed by KSS heading policy. |
| `ParagraphText` | `.kry canonical` | Semantic plain page paragraph text backed by KSS paragraph text policy. |
| `Column` | `.kry canonical` | Layout block. |
| `Row` | `.kry canonical` | Layout block. |
| `Stack` | `.kry canonical` | Layout block. |
| `Flow` | `.kry canonical` | Page/content flow layout. |
| `Grid` | `.kry canonical` | Grid layout; metrics and cursor placement are in `.kry`. |
| `Scroll` | `.kry canonical` | Parser statement form for generated/runtime lowering; lexical block form remains canonical for scroll content. |
| `End` | Lowered support | Parser block close marker, not a widget. |
| `Modal` | `.kry canonical` | Dialog/overlay layout surface. |
| `TitleBar` | `.kry canonical` | Title/action bar. |
| `TabBar` | `.kry canonical` | Tab navigation surface. |
| `NavigationBar` | `.kry canonical` | App navigation bar. |
| `Toolbar` | `.kry canonical` | Tool/action strip. |
| `Toast` | `.kry canonical` | Public props live in `runtime/toast_props.kry`; toast feedback command. |
| `Fieldset` | `.kry canonical` | Titled frame/group. |
| `PanedView` | `.kry canonical` | Split panes. |
| `Collapsible` | `.kry canonical` | Collapsible section. |
| `ListBox` | `.kry canonical` | List selection/navigation. |
| `TreeView` | `.kry canonical` | Tree rows/window, marker text, paint geometry, and row-selection decision policy in `.kry`; host keeps state/input. |
| `TableView` | `.kry canonical` | Table layout, scroll, scrollbar, cell geometry, header/row pointer decisions, keyboard selection, activation, clear-selection, resize lifecycle/width, and clipboard intent policy in `.kry`; host keeps state/input. |
| `ColorPicker` | `.kry canonical` | Color channel layout/conversion. |
| `CanvasGrid` | `.kry canonical` | Canvas grid line policy. |

## Block Statement Surface

These names are accepted as `.kry` lexical blocks by the parser's
`ui_block_prop_type` path. They include layout/content blocks and compositional
widget blocks; lowered `Begin*`/`End*` calls remain native support only.

| Block name | Current decision | Notes |
|---|---|---|
| `Disabled` | `.kry canonical` | Lexical disabled-content block; lowers to host disabled scope. |
| `Scroll` | `.kry canonical` | Lexical scroll-content block. |
| `TableCell` | `.kry canonical` | Lexical custom table-cell block. |
| `Canvas` | `.kry canonical` | Lexical canvas block. |
| `Popup` | `.kry canonical` | Lexical arbitrary overlay block. |
| `Text` | `.kry canonical` | Block form with `TextProps`. |
| `Row` | `.kry canonical` | Layout block with `RowProps`. |
| `Screen` | `.kry canonical` | Top-level layout block. |
| `Column` | `.kry canonical` | Layout block using column props. |
| `Stack` | `.kry canonical` | Layout block using stack/column props. |
| `Button` | `.kry canonical` | Composed content button block; lowers through native support. |
| `Card` | `.kry canonical` | Composed content card block; lowers through native support. |
| `TextField` | `.kry canonical` | Block form with `TextFieldProps`. |
| `TextArea` | `.kry canonical` | Block form with `TextAreaProps`. |
| `Image` | `.kry canonical` | Block form with `ImageProps`. |
| `Radio` | `.kry canonical` | Block form with `RadioProps`. |
| `Progress` | `.kry canonical` | Block form with `ProgressProps`. |
| `ColorPicker` | `.kry canonical` | Block form with `ColorPickerProps`. |
| `Separator` | `.kry canonical` | Block form with `SeparatorProps`. |
| `Spinbox` | `.kry canonical` | Block form with `SpinboxProps`. |
| `Dropdown` | `.kry canonical` | Block form with `DropdownProps`. |
| `SegmentedControl` | `.kry canonical` | Block form with `SegmentedControlProps`. |
| `Fieldset` | `.kry canonical` | Titled frame block. |
| `PanedView` | `.kry canonical` | Split pane block. |
| `Collapsible` | `.kry canonical` | Collapsible section block. |
| `ListBox` | `.kry canonical` | List block. |
| `TableView` | `.kry canonical` | Table block. |
| `NavigationBar` | `.kry canonical` | Navigation block. |
| `Toolbar` | `.kry canonical` | Toolbar block. |
| `TabBar` | `.kry canonical` | Tab bar block. |
| `Page` | `.kry canonical` | Web/page root block. |
| `Section` | `.kry canonical` | Semantic page section block. |
| `Heading` | `.kry canonical` | Semantic page heading block with its own KSS style kind. |
| `ParagraphText` | `.kry canonical` | Semantic page paragraph block with its own KSS style kind. |
| `Link` | `.kry canonical` | Link block. |
| `Flow` | `.kry canonical` | Page flow block. |
| `Grid` | `.kry canonical` | Grid layout block. |

## Native No-Compatibility Audit

No lowered widget block scopes are public compatibility exports from
`include/ui_tree.h`. Parser lowering and native tests use internal declarations
for these host scopes until the canonical `.kry` block surface owns the
remaining generated backends.

| Forbidden export | Canonical concept | Audit note |
|---|---|---|

## Go No-Compatibility Audit

No lowered widget block scopes are public package-level compatibility exports
from `go/kryon/api.go`. Generated Go routes lowered lexical scopes through its
private runtime value; public Go code uses canonical calls such as `kr.Button`,
`kr.Slider`, `kr.Menu`, and `.kry` blocks for `Scroll`, `Popup`, `Disabled`,
`TableCell`, and `Canvas`.

| Forbidden export | Canonical concept | Audit note |
|---|---|---|

## Web No-Compatibility Audit

No web runtime widget entries are accepted as compatibility names.
Lowered host support must remain private to generated code; generated web code
should use canonical `.kry` names and blocks.

## Core Drawing And Text

| Public name | Current decision | Notes |
|---|---|---|
| `Background` | `.kry canonical` | Viewport bounds policy is in `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `.kry canonical` | Public props live in `runtime/text_props.kry`; single canonical signature is `Text(TextProps)`. Selectable highlight geometry and double-click line-selection policy are in `.kry`. |
| `Paragraph` | `.kry canonical` | Metrics/default line-gap/layout spacing/height/alignment/selectable line-index/local-offset/double-click line-selection policy is in `.kry`; host keeps rich text parsing, line-break storage, icon shaping, and drawing. |
| `Box` | `.kry canonical` | Rectangle primitive with `Rectangle` bounds. |
| `Rect` | Removed | Old positional rectangle helper; use `Box`. |
| `Circle` | `.kry canonical` | Retained bounds policy is in `.kry`; host keeps circle drawing. Replaces raylib-style `DrawCircleV` in `.kry` surface. |
| `Ring` | `.kry canonical` | Retained bounds policy is in `.kry`; host keeps ring drawing. Replaces raylib-style `DrawRing` in `.kry` surface. |
| `Line` | `.kry canonical` | Endpoint and retained-bounds policy are in `.kry`; host keeps stroke drawing. |
| `Triangle` | `.kry canonical` | Retained bounds policy is in `.kry`; host keeps triangle drawing. |
| `Bevel` | Native canonical | Primitive drawing effect unless replaced by surface props. |
| `Icon` | `.kry canonical` | Bounds/size policy is in `.kry`; icon sheet/type surface is `IconType` with C `ICON_*` values and Go `kr.IconHome`-style constants. |
| `Image` | `.kry canonical` | Canonical image widget. |
| `Surface` | `.kry canonical` | Material/container paint helper; layer assembly policy is in `.kry`; material selection uses `MaterialKind`, not raw integers. |
| `Bullet` | `.kry canonical` | Bullet geometry and paint policy are in `.kry`; keep as a small primitive unless list item props absorb it. |

## Controls

| Public name | Current decision | Notes |
|---|---|---|
| `Card` | `.kry canonical` | Already has `.kry` module. |
| `Button` | `.kry canonical` | Single public button surface. Menu, split-action, icon-only, arrow, info/help, loading, disclosure, tone, emphasis, and fallback/terminal paint behavior live in `ButtonProps`/`.kry` policy or small `.kry` composition, not separate public widget names. |
| `Link` | `.kry canonical` | Canonical public name for URL/link activation; bounds, interaction, activation, color/hover/disabled policy are in `.kry`, typography uses resolved KSS font sizes directly, and URL dispatch remains host support. |
| `TextField` | `.kry canonical` | Metrics, horizontal scroll, paint geometry, buffer-limit, navigation, selection state, double-click/pan/focus decisions, platform text-input sync, text-buffer mutation/range/bracket policy, and edit intent policy are in `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership/painting, and rendering remain native host support. |
| `TextArea` | `.kry canonical` | Metrics, page-navigation rows, paint geometry, buffer-limit, navigation, selection state, double-click/pan/focus decisions, platform text-input sync, text-buffer mutation/range/bracket policy, and edit intent policy are in `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership/painting, and rendering remain native host support. |
| `Dropdown` | `.kry canonical` | Option/index normalization, popup placement, row/window, scrollbar, scrolling, keyboard intent, navigation, and indicator policy are in `.kry`; trigger and option typography use resolved KSS font sizes directly. |
| `Slider` | `.kry canonical` | Public props live in `runtime/slider_props.kry`; value type, orientation, angle/unit, component/editor/hit layout, and text paint geometry live in `SliderProps`/`.kry`; generated Go uses `kr.Slider`. |
| `Drag` | `.kry canonical` | Public props live in `runtime/drag_props.kry`; value type, range mode, typed keyboard input, component layout, and text paint geometry live in `DragProps`/`.kry`; generated Go uses `kr.Drag`. |
| `Input` | `.kry canonical` | Public props live in `runtime/input_props.kry`; value type, values, component/step-button layout, step policy, and temp-edit activation live in `.kry`; generated Go uses `kr.Input`; embedded editing uses `TextField` typography and step controls use `Button` typography. |
| `Spinbox` | `.kry canonical` | Public props live in `runtime/spinbox_props.kry`; layout and value stepping policy are in `.kry`; host handles button input and drawing. |
| `Toggle` | `.kry canonical` | Public props live in `runtime/toggle_props.kry`; paint/layout policy is in `.kry`, label typography is KSS-owned, host handles input and drawing. |
| `Checkbox` | `.kry canonical` | Public props live in `runtime/checkbox_props.kry`; paint, row/text layout, and flags toggle policy are in `.kry`; host handles input and drawing. |
| `Radio` | `.kry canonical` | Public props live in `runtime/radio_props.kry`; paint, layout, and marker text policy are in `.kry`; host handles focus/input and drawing. |
| `Selectable` | `.kry canonical` | Public props live in `runtime/selectable_props.kry`; paint/layout and toggle policy are in `.kry`; review whether list item props should absorb it later. |
| `Progress` | `.kry canonical` | Public props live in `runtime/progress_props.kry`; prefer one public progress name. |
| `ColorPicker` | `.kry canonical` | Public props live in `runtime/color_picker_props.kry`; channel layout, swatch paint geometry, and color conversion are in `.kry`; swatch activation is `Button` with swatch props. |
| `SegmentedControl` | `.kry canonical` | Layout policy is in `.kry`; segment typography and paint are KSS-owned; host handles label measurement, focus/input, and button drawing. |
| `LabelTextField` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `TextField`. |
| `CheckboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Checkbox`. |
| `SpinboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Spinbox`. |
| `ButtonRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Row` with `Button` children. |
| `SectionLabel` | Removed | Removed from public headers; internal row helper only. Public code should use `Text`/`Heading` props or `.kry` composition. |
| `InfoRows` | Internal support | Internal repeated label/value row helper; typography uses resolved KSS font sizes directly; public forms should use `.kry` layout with `Row`/`Text`. |

## Layout And Containers

| Public name | Current decision | Notes |
|---|---|---|
| `Column` | `.kry canonical` | Content and child placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Row` | `.kry canonical` | Content and child placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Grid` | `.kry canonical` | Metrics, columns, and cursor placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Stack` | `.kry canonical` | Content/child fill policy is in `.kry`; host keeps retained tree scope ownership. |
| `Screen` | `.kry canonical` | Top-level screen container; viewport fallback bounds policy is in `.kry`. |
| `Group` | `.kry canonical` | Non-layout grouping scope. Bounds/content policy is in `.kry`; host keeps retained tree scope ownership. |
| `Separator` | `.kry canonical` | Public props live in `runtime/separator_props.kry`; line, label, and bullet layout/paint policy are in `.kry`; label typography is KSS-owned; host handles text measurement and drawing. |
| `Fieldset` | `.kry canonical` | Public props live in `runtime/fieldset_props.kry`; titled border group. |
| `PanedView` | `.kry canonical` | Public props live in `runtime/paned_view_props.kry`; split clamp, layout, handle geometry, pointer split, drag lifecycle, and change policy are in `.kry`; host keeps active split pointer storage and popup input owner binding. |
| `Collapsible` | `.kry canonical` | Public props live in `runtime/collapsible_props.kry`; header metrics, geometry, marker text, pointer/body toggle, close, keyboard open, and tree focus-routing policy are in `.kry`/KSS; host keeps input sampling, focus application, and drawing. |
| `Scroll` | `.kry canonical` | Public props live in `runtime/scroll_props.kry`; lexical scroll-content block. Measurement, sizing, wheel offset, content-drag decision, thumb drag offset, scrollbar drag decision, ensure-visible policy, and clip/visual bounds geometry are in `.kry`; host keeps pointer ownership storage, clip-stack application, and lowered scope ownership. |
| `TableCell` | `.kry canonical` | Lexical custom table-cell block; lowers to host cell scope. |
| `ScrollContainer` | Internal support | Internal host helper only; public callers should use `Scroll` blocks. |
| `ScrollPage` | Internal support | Internal host helper only; not a public widget concept. |
| `ScreenScaffold` | Internal support | Internal app-shell helper only; compose pages from `.kry` layout. |

## Page And Web Surfaces

| Public name | Current decision | Notes |
|---|---|---|
| `Page` | `.kry canonical` | Top-level document surface for generated web/page output; lowers to layout scopes and page metadata host support. |
| `Section` | `.kry canonical` | Page section container; lowers to layout scopes. |
| `Heading` | `.kry canonical` | Semantic page heading backed by `Heading` KSS policy. |
| `ParagraphText` | `.kry canonical` | Semantic page paragraph backed by `ParagraphText` KSS policy. |
| `Flow` | `.kry canonical` | Page flow layout; lowers to row/layout policy. |

## Collections And Editors

| Public name | Current decision | Notes |
|---|---|---|
| `ListBox` | `.kry canonical` | Layout/navigation and row paint geometry policy is in `.kry`; item typography is KSS-owned. Multi-selection uses `selected`, `selected_count`, and `anchor` props, with keyboard input/navigation/selection policy in `.kry`. KSS styles multi-select mode with `ListBoxMulti` and `ListBoxMultiItem`, not a separate `MultiSelectList` widget. Host handles input sampling, scroll scope, and drawing. |
| `TreeView` | `.kry canonical` | Row/window, marker text, paint geometry, and row-selection decision policy is in `.kry`; item typography defaults are KSS-owned; host handles input sampling, selected-id storage, expansion state, and drawing. |
| `TableView` | `.kry canonical` | Header/body/frozen-row/scroll/scrollbar/cell geometry, header/row pointer decisions, keyboard selection, activation, clear-selection, resize lifecycle/width, and clipboard intent policy are in `.kry`; header, cell, and selection text typography is KSS-owned, including native fallback sizing; host handles column ordering, input sampling, stored selection pointers, resize pointer ownership, clipboard IO, and drawing. |
| `CanvasGrid` | `.kry canonical` | Grid spacing, line counts, and line rectangles are in `.kry`; host handles drawing. |
| `Canvas` | `.kry canonical` | Transform, hit-test, and result policy are in `.kry`; host keeps clip/camera renderer scope. |
| `DragDrop` | `.kry canonical` | Typed drag/drop interaction concept. Source and target roles belong in props or composition; source/target lifecycle decision policy is in `.kry`; host keeps payload storage, type comparison, and pointer ownership. |

## Navigation

| Public name | Current decision | Notes |
|---|---|---|
| `NavigationBar` | `.kry canonical` | Item interaction/state, paint, sizing, and configuration modal layout/count/default policy are in `.kry`; item and configuration-slot labels use `NavigationBarItem` KSS typography. |
| `Toolbar` | `.kry canonical` | Metrics, geometry, icon style-size, and icon slider popup close policy are in `.kry`; host handles input, drawing, and child `Button`/`Dropdown` calls. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props; metrics, selectable/keyboard navigation, bar open/index policy, and group pointer open/close decisions are in `.kry`. |
| `TabBar` | `.kry canonical` | Sizing, scroll, keyboard index, reorder marker/drag lifecycle, and double-click decision policy are in `.kry`; tab label typography is KSS-owned, including native fallback sizing; host handles input sampling, stored drag state, and drawing. |
| `TitleBar` | `.kry canonical` | Effective height/state, layout, and paint geometry policy are in `.kry`; title typography uses resolved KSS font sizes directly; leading action and dropdown behavior live in `TitleBarProps`. |
| `Router` | `.kry canonical` | Navigation runtime, not a visual widget. Routes, state, props, and result live in `runtime/router_props.kry`. |
| `Link` | `.kry canonical` | Canonical navigation/link widget. |

## Terminal Support

Terminal support is reusable runtime infrastructure, not a general UI widget
family. Keep app-specific terminal product UX outside Kryon; keep reusable pane
metrics and protocol support here.

| Public name | Current decision | Notes |
|---|---|---|
| `TerminalPane` | `.kry support + native host` | Font fallback, line-height floor, content padding/bounds, grid clamps, and scroll-indicator geometry live in `runtime/terminal_pane.kry`; terminal state, PTY/session IO, ANSI parsing, clipboard/selection, text measurement, input sampling, and drawing remain native support. |

## Overlays And Feedback

| Public name | Current decision | Notes |
|---|---|---|
| `Popup` | `.kry canonical` | Arbitrary anchored/floating content. Mode/input/Escape-close policy is in `.kry`; host handles pointer sampling, paint layers, clipping, and child content. |
| `Modal` | `.kry canonical` | Layout, frame geometry, outside-dismissal, prompt availability/focus fallback/input/result, and action sizing/row policy are in `.kry`; title, message, and action text typography is KSS-owned with resolved font sizes used directly; prompt fields use `TextField` typography; host handles capture application, release consumption, text editing, and drawing. |
| `Toast` | `.kry canonical` | Public toast feedback surface. Request/render clear decisions, duration/deadline, layout, text-placement, and truncation policy are in `.kry`; host keeps message storage, clock source, text measurement, and drawing. |
| `Focus` | Partly `.kry-backed` | Focus ring geometry and keyboard activation policy are in `.kry`; focus state, registration, key sampling, popup capture lookup, and drawing remain host support. |
| `Guide` | `.kry canonical` | Guided overlay flow. The clean public API is one `Guide(GuideProps)` surface with step data in props; `GuideStep` is data, not a widget. Label typography uses resolved KSS font sizes directly. Current C rendering is host support around `runtime/guide.kry` policy. |
| `GuideStep` | Props/data only | One anchored instruction inside `GuideProps`; not a standalone widget. |
| `GuidePager` | Internal support | Not a public widget. Footer layout/page transition policy is `.kry`; the C helper lives under `src/ui` and is not exported by public headers. |
| `StylePicker` | `.kry canonical` | Public props and option/selection/dropdown state policy live in `runtime/style_picker_props.kry`; style-pack storage, KSS parsing, and dropdown rendering remain host support. |

## Game2D Nodes

Game2D has its own node family. These are canonical for the Game2D domain and
stay separate from general UI widgets. Scene/Node2D declaration props/defaults
and concrete node props/enums live in `runtime/node2d_props.kry`; scene kind/flag values live in
`runtime/scene_tree_props.kry`; scene graph ownership and host resources remain
native Game2D support. Public code uses generated `NodeKind*` and `NodeFlag*`
names, not the old `NODE_*`/`NODE_FLAG_*` C constants.

| Public name | Current decision | Notes |
|---|---|---|
| `Scene` / `NodeKindRoot` | `.kry props, native scene` | Root game scene; declaration defaults live in `runtime/node2d_props.kry`; lifecycle remains native. |
| `Node2D` / `NodeKindNode2D` | `.kry props, native scene` | Base 2D transform node; declaration defaults live in `runtime/node2d_props.kry`; world transform propagation remains native. |
| `Camera2D` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; lifecycle remains native. |
| `Sprite2D` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; asset loading and drawing remain native. |
| `AnimatedSprite2D` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; frame playback remains native. |
| `TileMap` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; tile rendering remains native. |
| `CollisionShape2D` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; physics shape creation remains native. |
| `Area2D` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; monitoring remains native. |
| `Body2D` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; physics body ownership remains native. |
| `AnimationPlayer` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; playback/lifecycle remain native. |
| `AudioSource` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; playback handles remain native. |
| `Light2D` | `.kry props, native scene` | Props live in `runtime/node2d_props.kry`; light rendering remains native. |
| `NodeFlagAlive`, `NodeFlagReady`, `NodeFlagDirty` | `.kry support` | Generated scene-tree flags from `runtime/scene_tree_props.kry`; host code owns lifecycle mutation. |

## Escape Hatches

| Public name | Current decision | Notes |
|---|---|---|
| `Custom` | Internal support | Retained-tree escape hatch for host-only nodes; not preferred public design surface. |

## Retained Node Kinds

These are the current internal retained UI tree kind values. They are no longer
public header constants; public inspection uses `GetNodeKindName(kind)` and gets
clean names such as `Button`, `TextField`, and `Image`. Use this table for
naming feedback on the internal/runtime mapping while the app-facing surface
stays prefix-free.

| Current node kind | Public widget/concept | Decision |
|---|---|---|
| `WidgetKindScreen` | `Screen` | `.kry canonical`; viewport fallback bounds policy is `.kry-backed` |
| `WidgetKindBackground` | `Background` | `.kry-backed`; bounds and app fallback policy live in runtime primitive policy |
| `WidgetKindText` | `Text` | `.kry canonical` |
| `WidgetKindBox` | `Box` | `.kry-backed`; retained node kind now matches the public `Box` concept |
| `WidgetKindCircle` | `Circle` | `.kry-backed`; public code uses `Circle` |
| `WidgetKindRing` | `Ring` | `.kry-backed`; public code uses `Ring` |
| `WidgetKindLine` | `Line` | `.kry-backed`; measured bounds and retained endpoints come from runtime primitive policy |
| `WidgetKindTriangle` | `Triangle` | `.kry-backed`; public code uses `Triangle` |
| `WidgetKindButton` | `Button` | `.kry canonical` |
| `WidgetKindTextField` | `TextField` | `.kry canonical`; metrics, horizontal scroll, paint geometry, buffer-limit, navigation, focus/platform text-input sync, text-buffer mutation/range/bracket policy, selection state, and edit intent migrated; raw string storage and IME still host support |
| `WidgetKindTextArea` | `TextArea` | `.kry canonical`; metrics, page rows, paint geometry, buffer-limit, navigation, focus/platform text-input sync, text-buffer mutation/range/bracket policy, selection state, and edit intent migrated; raw string storage and IME still host support |
| `WidgetKindDropdown` | `Dropdown` | `.kry canonical` |
| `WidgetKindSlider` | `Slider` | `.kry canonical` |
| `WidgetKindToggle` | `Toggle` | `.kry canonical` |
| `WidgetKindCheckbox` | `Checkbox` | `.kry canonical` |
| `WidgetKindParagraph` | `Paragraph` | `.kry canonical`; rich text metrics/default line-gap/layout spacing/height/alignment/selectable line-index/local-offset/double-click line-selection policy is `.kry-backed` |
| `WidgetKindNavigationBar` | `NavigationBar` | `.kry canonical` |
| `WidgetKindTabBar` | `TabBar` | `.kry canonical` |
| `WidgetKindTitleBar` | `TitleBar` | `.kry canonical` |
| `WidgetKindGroup` | `Group` | `.kry canonical`; bounds/content policy is `.kry-backed` |
| `WidgetKindColumn` | `Column` | `.kry canonical`; placement policy is `.kry-backed` |
| `WidgetKindRow` | `Row` | `.kry canonical`; placement policy is `.kry-backed` |
| `WidgetKindStack` | `Stack` | `.kry canonical`; placement policy is `.kry-backed` |
| `WidgetKindGrid` | `Grid` | `.kry canonical`; metrics and cursor placement policy are `.kry-backed` |
| `WidgetKindImage` | `Image` | `.kry canonical`; fit and missing-placeholder layout policy are `.kry-backed` |
| `WidgetKindCustom` | `Custom` | Internal support escape hatch |
| `WidgetKindDrag` | `Drag` | `.kry canonical` |
| `WidgetKindRouter` | `Router` | `.kry canonical` |
| `WidgetKindCard` | `Card` | `.kry canonical` |

### Canonical Widgets Lowered Through `WidgetKindCustom`

These names are still clean public concepts even though the retained tree uses
`WidgetKindCustom` as the temporary host bucket. Do not expose the host bucket
as a public widget name. Use this list as the feedback surface for deciding
which concepts deserve dedicated retained node kinds later.

| Public widget/concept | Current retained lowering | Decision |
|---|---|---|
| `Separator` | `WidgetKindCustom` | `.kry canonical`; line/label/bullet policy lives in `runtime/separator.kry` |
| `DragDrop` | `WidgetKindCustom` | `.kry canonical`; source/target lifecycle policy lives in `runtime/drag_drop.kry` |
| `Radio` | `WidgetKindCustom` | `.kry canonical`; paint/layout/marker policy lives in `runtime/radio.kry` |
| `Progress` | `WidgetKindCustom` | `.kry canonical`; track/fill/label layout policy lives in `runtime/progress.kry` |
| `Plot` | `WidgetKindCustom` | `.kry canonical`; range/mark/text paint policy lives in `runtime/plot.kry` |
| `Focus` | `WidgetKindCustom` | `.kry-backed`; focus ring geometry lives in `runtime/focus.kry` |
| `Spinbox` | `WidgetKindCustom` | `.kry canonical`; step and layout policy lives in `runtime/spinbox.kry` |
| `Fieldset` | `WidgetKindCustom` | `.kry canonical`; title/border paint policy lives in `runtime/fieldset.kry` |
| `ListBox` | `WidgetKindCustom` | `.kry canonical`; row/window and multi-select policy lives in `runtime/list_box.kry` and `runtime/list_box_multi.kry` |
| `TreeView` | `WidgetKindCustom` | `.kry canonical`; row/window/marker/selection paint policy lives in `runtime/tree_view.kry` |
| `TableView` | `WidgetKindCustom` | `.kry canonical`; table geometry, roles, pointer decisions, selection, activation, resize, clipboard intent, and scroll policy live in `runtime/table_view.kry` |
| `PanedView` | `WidgetKindCustom` | `.kry canonical`; split/handle/drag policy lives in `runtime/paned_view.kry` |
| `Collapsible` | `WidgetKindCustom` | `.kry canonical`; header/close/layout and input decision policy lives in `runtime/collapsible.kry` |
| `ColorPicker` | `WidgetKindCustom` | `.kry canonical`; channel/swatch policy lives in `runtime/color_picker.kry` |
| `Modal` | `WidgetKindCustom` | `.kry canonical`; layout/frame/action policy lives in `runtime/modal.kry` |
| `Toolbar` | `WidgetKindCustom` | `.kry canonical`; toolbar, bottom-row layout, and icon slider popup policy lives in `runtime/toolbar.kry` |
| `Menu` | `WidgetKindCustom` | `.kry canonical`; bar/popup/context metrics and navigation policy live in `runtime/menu.kry` |
| `Selectable` | `WidgetKindCustom` | `.kry canonical`; paint/layout and toggle policy live in `runtime/selectable.kry` |
| `StylePicker` | `WidgetKindCustom` | `.kry canonical`; public props and option state live in `runtime/style_picker_props.kry` |
| `Guide` | `WidgetKindCustom` | `.kry canonical`; guide and pager policy live in `runtime/guide.kry` and `runtime/guide_pager.kry` |

The public retained-tree and generated-code surface is guarded by
`canonical-surface-test` and `public-api-names-check`. Public names must be the
canonical names listed above; historical prefix names, spelling aliases, split
numeric widget names, and lowered host hooks stay out of docs, headers, Go
package exports, web runtime exports, parser call names, and snapshots.

## Cleanup Queue

The public surface is now guarded. Remaining work is not choosing the names
again; it is finishing the migration of internal policy and host plumbing
behind the canonical names.

1. Finish C geometry-to-`.kry` migration:
   `Button`, `Dropdown`, `Scroll`, `TabBar`, `PanedView`, and several primitive
   widgets already have `.kry` policy, but raw widget constants still remain in
   native files. The latest focused audits moved context-popup activation,
   popup input records, and Escape-close policy into `runtime/popup_policy.kry` and label text-field row layout into
  `runtime/rows.kry`; SegmentedControl row-advance/wrap policy now routes through
  `runtime/segmented_control.kry`; InfoRows background/text/separator geometry and
  button-row wrap height/advance now also
  route through `runtime/rows.kry`; dropdown panel/option/scrollbar role
  policy, selected-option appearance, trigger/menu keyboard intent, and menu transient pointer state now route through `runtime/dropdown.kry`;
  menu selectable-item, submenu activation, pointer item effects,
  row and bar keyboard input decisions, wraparound navigation, and bar open/index policy now route through
  `runtime/menu.kry`; group pointer open/close decisions also now route
  through `runtime/menu.kry`; ListBox row selection policy now routes through
  `runtime/list_box.kry`; ListBox multi-select keyboard input now routes
  through `runtime/list_box_multi.kry`; centered-column and page side-padding policy now route
  through `runtime/layout.kry`; reorder lifecycle gates now route through
  `runtime/reorder.kry`; swipe drag/release lifecycle effects now route through
  `runtime/swipe.kry`; drag/drop source/target lifecycle decisions now route
  through `runtime/drag_drop.kry`; scroll-page content-width normalization and
  scroll drag/scrollbar-drag/ensure-visible policy now route through
  `runtime/scroll.kry`;
  table keyboard selection, clear-selection, header sort cycling,
  row click/context effects, resize lifecycle/width, clipboard intent, and scroll-into-view policy now route through
  `runtime/table_view.kry`; `Input` numeric kind default format, integer
  rounding, step dispatch, temp-edit activation, shared pointer interaction for old immediate helpers, and
  pointer-drag threshold/start/direction policy now route through
  `runtime/input.kry`; retained
  and immediate `Drag`/`Slider`/`Spinbox` default numeric formats now also use
  that runtime policy; `Spinbox` child focus IDs now route through
  `runtime/spinbox.kry`; `Drag` typed keyboard input, text inset, label-gap metrics, component
  drag tokens, and retained pointer lifecycle now route through
  `runtime/drag.kry`; `Slider` component tokens,
  focus IDs, editor center placement, old immediate pointer lifecycle, and
  retained ratio pointer lifecycle now route through
  `runtime/slider.kry`; toolbar action focus IDs now route through
  `runtime/toolbar.kry`; `ColorPicker` channel focus IDs now route through
  `runtime/color_picker.kry`; immediate list/tree/table row wheel-step policy
  now routes through `runtime/scroll.kry`; numeric temp-edit double-click activation
  and selectable text double-click slop metrics, including retained text widget click slop,
  now route through `runtime/input.kry` and `runtime/text.kry`;
  `TextField`/`TextArea` double-click slop, field pan
  drag threshold, and `TextArea` gutter metrics now route through
  `runtime/text_input.kry`; retained wrapped text measurement now uses
  `runtime/paragraph.kry` default line-gap policy; `Progress` label
  vertical placement now routes through `runtime/progress.kry`; tab close-label
  placement now routes through `runtime/tab_bar.kry`; retained wrapped text
  painting now uses `runtime/paragraph.kry` default line-gap policy; retained
  `DragRange` label placement now routes through `runtime/drag.kry`; generic
  pointer drag threshold policy now routes through `runtime/input.kry`;
  `TextArea` scrollbar width now routes through `runtime/text_input.kry`;
  `Link` underline placement now routes through `runtime/link.kry`; text
  selection highlight padding and minimum width now route through
  `runtime/text.kry`; shared control-text baseline sample and clip guard now
  route through `runtime/text.kry`; text baseline vertical placement,
  centered text placement, and measured-height/line-height fallback policy now
  route through `runtime/text.kry`;
  shared centered row text placement now routes
  through `runtime/text.kry`; slider/toggle focus stroke width now routes
  through `runtime/focus.kry`; icon button size and padding policy now routes
  through `runtime/icon.kry`; desktop layout breakpoint policy now routes
  through `runtime/layout.kry`; default focus outline bounds now route through
  `runtime/focus.kry`; default control shine bounds now route through
  `runtime/style.kry`; default ripple radius policy now routes through
  `runtime/style.kry`; retained `Button(info)` bounds and diameter policy now
  routes through `runtime/button.kry`; immediate `Paragraph` default line-gap
  policy now routes through `runtime/paragraph.kry`; immediate `Separator`
  label vertical placement now routes through `runtime/separator.kry`;
  row muted-label alpha now routes through `runtime/rows.kry`;
  profile header muted-text alpha now routes through `runtime/profile_header.kry`;
  text-area gutter inactive-label alpha now routes through
  `runtime/text_input.kry`;
  navigation-bar icon tint alpha now routes through
  `runtime/navigation_bar.kry`;
  profile image picker selected-stroke width now routes through
  `runtime/profile_header.kry`; toolbar divider line geometry now routes
  through `runtime/toolbar.kry`; radio state-layer alpha now routes through
  `runtime/radio.kry`; title-bar action radius now routes through
  `runtime/title_bar.kry`; progress draw radius normalization now routes
  through `runtime/progress.kry`; default text selection alpha now routes
  through `runtime/text.kry`; default disabled alpha and focus outline alpha
  now route through `runtime/style.kry`; termi button fallback outline alpha
  now routes through `runtime/button.kry`. Continue by reducing raw
  native constants in shared immediate-mode helpers. Icon slider popup and
  bottom icon row style icon sizing now route through `runtime/toolbar.kry`;
  icon action style size/radius policy now routes through `runtime/button.kry`;
  default state-layer alpha, legacy box radius, elevation shadow policy, and
  ripple paint policy now route through `runtime/style.kry`; guide
  keyboard input mapping now routes through `runtime/guide.kry`; TabBar
  keyboard intent mapping now routes through `runtime/tab_bar.kry`; guide pager
  keyboard input mapping now routes through `runtime/guide_pager.kry`; modal
  prompt keyboard input/result mapping now routes through `runtime/modal.kry`.
2. Keep prefix cleanup verified:
   Guard tests intentionally mention old names so they can reject regressions.
   Re-run the prefix scans after each widget migration so compatibility shims do
   not creep back in.
3. Finish text editing policy migration:
   `TextField` and `TextArea` already own metrics, paint geometry,
   buffer-limit, navigation, edit-intent, selection range, preedit
   accept/cancel/drain gating, composition session visibility/cancel policy,
   composition phase classification, apply-result flag policy, preedit
   selection-length policy, composition view range/offset policy, and
   composition paint span/underline policy, selection paint span policy,
   selection owner match policy, context-menu availability policy, and
   context-menu plus keyboard edit-command decision policy, text-area shortcut
   focus-claim policy, delete/replacement/native-edit/commit gate policy,
   navigation/enter/collapse gate policy, keyboard/escape/selection-range/
   composition-display gate policy, older `EditText` shortcut/commit gate
   policy, retained-tree text-input double-click policy, and text reveal/scroll/context-registration gate policy in `.kry`;
   raw string storage/memmove/scanning still native; remaining native work is
   IME/composition, selection
   ownership/painting, and the final decision about how much of that can become
   reusable `.kry` policy.
4. Finish rich text migration:
  `Paragraph` has `.kry` metrics/default line-gap/layout spacing/height/line-stride/alignment/selectable line-index/local-offset/double-click line-selection
  policy, retained selectable text block height/line advance, and generated `ParagraphSpec` data, but parsing, line-break ownership,
  icon shaping, and rendering are still host work.
5. Audit host-owned input/state lifecycles:
   retained menu open/focus/input state, drag/drop payload storage, reorder and
   swipe pointer ownership storage, paned-view active split storage, tree/table stored selection
   mutation, table resize pointer ownership/clipboard IO, modal input capture, and toast message
   storage/clock source are still native support around `.kry` policy.
6. Finish lowered block backend cleanup:
   `Scroll`, `Popup`, `Disabled`, `TableCell`, `Canvas`, and composed content
   blocks are canonical `.kry` syntax. Lowered `Scroll` scope geometry, wheel,
   content-drag, thumb-drag, scrollbar-drag, and ensure-visible policy now route through
   `runtime/scroll.kry`; the remaining
   host scopes still require backend support until generated backends own the
   whole block path.
7. Separate pure host services from widget policy:
   image cache/loading/drawing, icon sheet/type lookup, URL dispatch, text
   measurement, focus registration, paint layers, clipping, and platform
   services should stay native only when they are true host services and not
   widget policy.
8. Decide the Game2D boundary:
   Game2D declaration props/enums are generated from
   `runtime/node2d_props.kry`, but scene lifecycle, physics/audio handles,
   asset playback, and rendering remain native scene support. Keep this as an
   explicit non-widget domain unless `.kry` starts owning scene lifecycle.
9. Keep lowered host scopes out of public `.kry` documentation:
   immediate-mode `Begin*`/`End*` wrappers are native support, not widget names.
   Tutorial image helpers remain internal and route through canonical `Image`
   policy.
10. Keep `docs/IMGUI_WIDGET_COVERAGE.md` as the coverage audit. Use this file
    as the naming and migration review surface.
