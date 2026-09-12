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
| `Native canonical` | This is low-level drawing/layout/runtime surface that may remain native. |
| `Native support` | Runtime support only; not a canonical public widget. |
| `Composite candidate` | Keep as `.kry` composition if it is reusable, otherwise fold into callers. |
| `Rename review` | Public name needs a naming decision. |
| `Remove after migration` | Compatibility or duplicate surface to delete once callers move. |

## Runtime `.kry` Modules

These modules already exist in `runtime/` and are part of the current widget
surface review:

| Module | Current role | Decision |
|---|---|---|
| `runtime/bevel.kry` | Bevel line geometry policy | `.kry canonical` |
| `runtime/button.kry` | Button composition | `.kry canonical` |
| `runtime/button_props.kry` | Button props | `.kry canonical` |
| `runtime/canvas.kry` | Canvas transform and hit-test policy | `.kry canonical` |
| `runtime/canvas_grid.kry` | CanvasGrid line policy | `.kry canonical` |
| `runtime/card.kry` | Card composition | `.kry canonical` |
| `runtime/card_props.kry` | Card props | `.kry canonical` |
| `runtime/checkbox.kry` | Checkbox paint/layout policy | `.kry canonical` |
| `runtime/collapsible.kry` | Collapsible metrics/header geometry policy | `.kry canonical` |
| `runtime/color_picker.kry` | ColorPicker channel layout and color policy | `.kry canonical` |
| `runtime/control_props.kry` | Shared control props | `.kry canonical` |
| `runtime/dropdown.kry` | Dropdown composition | `.kry canonical` |
| `runtime/drag_drop.kry` | DragDrop decision policy | `.kry canonical` |
| `runtime/drag.kry` | Drag value/keyboard policy | `.kry canonical` |
| `runtime/grid.kry` | Grid composition | `.kry canonical` |
| `runtime/grid_props.kry` | Grid props | `.kry canonical` |
| `runtime/group.kry` | Group bounds/content policy | `.kry canonical` |
| `runtime/guide.kry` | Guide overlay layout and step policy | `.kry canonical` |
| `runtime/guide_pager.kry` | Internal pager footer layout and page transition policy | Native support |
| `runtime/icon.kry` | Icon bounds/size policy | `.kry canonical` |
| `runtime/input.kry` | Input value/step policy | `.kry canonical` |
| `runtime/input_props.kry` | Input props | `.kry canonical` |
| `runtime/fieldset.kry` | Fieldset layout/paint policy | `.kry canonical` |
| `runtime/focus.kry` | Focus ring and debug overlay geometry policy | Native support |
| `runtime/layout.kry` | Column/Row/Stack content and child placement policy | `.kry canonical` |
| `runtime/link.kry` | Link state/color policy | `.kry canonical` |
| `runtime/list_box.kry` | ListBox layout/navigation policy | `.kry canonical` |
| `runtime/material.kry` | Material layer assembly | `.kry canonical` |
| `runtime/menu.kry` | Menu metrics and geometry policy | `.kry canonical` |
| `runtime/multi_select_list.kry` | MultiSelectList row/navigation/selection policy | `.kry canonical` |
| `runtime/navigation_bar.kry` | Navigation bar composition | `.kry canonical` |
| `runtime/paint.kry` | Paint/drawing helpers | Native support |
| `runtime/paned_view.kry` | PanedView split/handle geometry policy | `.kry canonical` |
| `runtime/paragraph.kry` | Paragraph metrics/default policy | `.kry canonical` |
| `runtime/plot.kry` | Plot geometry and text policy | `.kry canonical` |
| `runtime/popup_policy.kry` | Popup mode/input policy | `.kry canonical` |
| `runtime/primitive.kry` | Background/Rect/Line primitive geometry policy | `.kry canonical` |
| `runtime/progress.kry` | Progress layout policy | `.kry canonical` |
| `runtime/radio.kry` | Radio paint/layout policy | `.kry canonical` |
| `runtime/segmented_control.kry` | SegmentedControl layout policy | `.kry canonical` |
| `runtime/selectable.kry` | Selectable paint/layout policy | `.kry canonical` |
| `runtime/separator.kry` | Separator/Bullet layout and paint policy | `.kry canonical` |
| `runtime/slider.kry` | Slider composition plus value/keyboard policy | `.kry canonical` |
| `runtime/spinbox.kry` | Spinbox layout/value policy | `.kry canonical` |
| `runtime/scroll.kry` | Scroll measurement/sizing policy | `.kry canonical` |
| `runtime/style.kry` | Style helpers | `.kry canonical` |
| `runtime/surface.kry` | Surface/container helpers | `.kry canonical` |
| `runtime/tab_bar.kry` | TabBar sizing/scroll policy | `.kry canonical` |
| `runtime/text.kry` | Text composition | `.kry canonical` |
| `runtime/text_input.kry` | TextField/TextArea metrics and scroll policy | `.kry canonical` |
| `runtime/theme.kry` | Theme data/helpers | `.kry canonical` |
| `runtime/title_bar.kry` | TitleBar layout policy | `.kry canonical` |
| `runtime/toggle.kry` | Toggle composition | `.kry canonical` |
| `runtime/toolbar.kry` | Toolbar metrics and geometry policy | `.kry canonical` |
| `runtime/toast.kry` | Toast duration and layout policy | `.kry canonical` |
| `runtime/transition_fade.kry` | TransitionFade alpha/easing policy | `.kry canonical` |
| `runtime/modal.kry` | Modal layout and action policy | `.kry canonical` |
| `runtime/tree_view.kry` | TreeView row/window geometry policy | `.kry canonical` |
| `runtime/table_view.kry` | TableView layout and scroll geometry policy | `.kry canonical` |

## Current Implementation Audit

This is the current migration truth, not the desired final state. A name in the
registry is only considered `.kry`-backed when its reusable behavior, props, or
layout policy lives in `runtime/*.kry` and native code only adapts host input,
text measurement, painting, storage, or platform services.

| Group | `.kry`-backed today | Still native-only or compatibility |
|---|---|---|
| Text and drawing | `Text` style resolution, `Paragraph` metrics/default policy, `Background`/`Rect`/`Line` geometry policy, `Bevel` line geometry, `Icon` bounds/size policy, `Image` canonical props/name and placeholder layout, clean drawing primitive names (`Box`, `Circle`, `Ring`, `Triangle`) | icon sheet/drawing host support, paragraph reflow/rendering |
| Actions | `Button`, `Card`, `Link`, `Button` menu/split/arrow/info options, `InvisibleButton` disabled policy | helper button variants belong in `ButtonProps` or composition |
| Inputs | `Checkbox`, `Dropdown`, `Progress`, `Radio`, `SegmentedControl`, `Selectable`, `Slider`, `Spinbox`, `TextField`/`TextArea` metrics, `Toggle`, `Button` swatch props, `ColorPicker` layout/color policy | text composition/editing host support |
| Layout | `Column`/`Row`/`Stack` content and child placement policy, `Group` bounds/content policy, `Screen` viewport fallback bounds policy, `Grid`, `Fieldset` layout policy, `PanedView` split geometry, `Collapsible` header geometry, `Separator`, `Scroll` measurement/sizing policy, shared `Surface`/`Style`/`Material` policy | scroll/list/table begin-end wrappers |
| Collections | `Canvas` transform/hit-test policy, `CanvasGrid`, drag/drop decision policy, `ListBox` layout/navigation policy, `MultiSelectList` row/navigation/selection policy, `Plot` geometry policy, `TreeView` row/window geometry policy, `TableView` layout/scroll geometry policy | drag/drop payload storage |
| Navigation | `NavigationBar` paint policy, `TabBar` sizing/scroll policy, `Toolbar` metrics/geometry policy, `TitleBar` layout policy, menu geometry policy | `MenuBar`, `PopupMenu`, `ContextMenu` retained state/input, router/link helpers |
| Overlays | `Popup` mode/input policy, `Focus` ring geometry policy, `Guide` overlay layout/step policy, `Modal` layout/action policy, `Toast` duration/layout policy, `TransitionFade` alpha/easing policy | theme pickers |
| Game2D | Native scene nodes | Game2D nodes are separate from UI widgets; keep them in the Game2D runtime unless `.kry` scene declarations are introduced. |

The immediate migration target is to finish moving high-use controls first:
`TextField` and `TextArea` editing/composition policy. Once those are backed by `.kry`, collapse the
native suffix variants into props on the canonical widgets.

## Registry Surface Audit

This table is the authoritative shared list of public node names from
`src/ui/ui_node_registry.c`. Keep one row per registry name so rename feedback
has a single place to land.

| Public name | Registry group | Detail | Runtime `.kry` source | State | Migration note |
|---|---|---|---|---|---|
| `Background` | `UI/Display` | Fill | `runtime/primitive.kry` | Partly `.kry-backed` | Viewport bounds policy is `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `UI/Display` | Label | `runtime/text.kry` | `.kry-backed` | Keep one `Text(TextProps)` surface. |
| `Paragraph` | `UI/Display` | Rich text | `runtime/paragraph.kry` | Partly `.kry-backed` | Metrics/default policy is `.kry`; text parsing, reflow, icon shaping, and drawing remain host support. |
| `Rect` | `UI/Display` | Shape | `runtime/primitive.kry` | Partly `.kry-backed` | Bounds policy is `.kry`; still review whether public name should be `Box` or surface props. |
| `Line` | `UI/Display` | Stroke | `runtime/primitive.kry` | Partly `.kry-backed` | Endpoint and retained-bounds policy is `.kry`; host keeps stroke drawing. |
| `Bevel` | `UI/Display` | Relief | `runtime/bevel.kry` | `.kry-backed` | Line geometry is `.kry`; still review whether it should fold into `Surface`/material props. |
| `Icon` | `UI/Display` | Icon | `runtime/icon.kry` | Partly `.kry-backed` | Bounds/size policy is `.kry`; icon sheet/type lookup and drawing remain host support. |
| `Image` | `UI/Display` | Image | `runtime/image.kry` | Partly `.kry-backed` | Canonical replacement for old `Picture`; fit and placeholder layout policy are `.kry`, cache/loading/drawing remain host support. |
| `Card` | `UI/Input` | Surface action | `runtime/card.kry`, `runtime/card_props.kry` | `.kry-backed` | Card composition and props live in `.kry`. |
| `Button` | `UI/Input` | Action | `runtime/button.kry`, `runtime/button_props.kry` | `.kry-backed` | Single button surface; menu/split/info/icon variants are props/composition. |
| `Link` | `UI/Input` | Link | `runtime/link.kry` | Partly `.kry-backed` | Canonical replacement for old `Href`; state/color policy is `.kry`, URL dispatch remains host support. |
| `TextField` | `UI/Input` | Input | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics/scroll are `.kry`; editing, IME, selection, and paint still native. |
| `Dropdown` | `UI/Input` | Selection | `runtime/dropdown.kry` | `.kry-backed` | Selection-only control; old combo surface stays removed. |
| `Slider` | `UI/Input` | Value | `runtime/slider.kry` | `.kry-backed` | Value type, orientation, and angle/unit are props. |
| `Toggle` | `UI/Input` | On/off | `runtime/toggle.kry` | `.kry-backed` | Host handles input and drawing; paint/layout policy is `.kry`. |
| `Checkbox` | `UI/Input` | Boolean | `runtime/checkbox.kry` | `.kry-backed` | Paint/layout/flag policy is `.kry`. |
| `Radio` | `UI/Input` | Choice | `runtime/radio.kry` | `.kry-backed` | Paint, layout, and marker text policy are `.kry`; host keeps group input. |
| `Progress` | `UI/Input` | Progress | `runtime/progress.kry` | `.kry-backed` | One public progress concept. |
| `Spinbox` | `UI/Input` | Number | `runtime/spinbox.kry` | `.kry-backed` | Layout/step policy is `.kry`; host keeps text/button input. |
| `ColorPicker` | `UI/Input` | Color | `runtime/color_picker.kry` | `.kry-backed` | Channel layout and conversion are `.kry`. |
| `Group` | `UI/Layout` | Container | `runtime/group.kry` | `.kry-backed` | Canonical non-layout grouping scope; bounds/content policy is `.kry`, host keeps retained tree scope ownership. |
| `Separator` | `UI/Layout` | Divider | `runtime/separator.kry` | `.kry-backed` | Line, label, and bullet policy are `.kry`. |
| `Fieldset` | `UI/Layout` | Frame | `runtime/fieldset.kry` | `.kry-backed` | Canonical titled group; old `LabelFrame` stays removed. |
| `PanedView` | `UI/Layout` | Split panes | `runtime/paned_view.kry` | Partly `.kry-backed` | Split clamp and handle geometry are `.kry`; host keeps drag/input ownership. |
| `Collapsible` | `UI/Layout` | Section | `runtime/collapsible.kry` | Partly `.kry-backed` | Header metrics, geometry, and marker text are `.kry`; host keeps input, focus, tree navigation, and drawing. |
| `ListBox` | `UI/Collections` | List | `runtime/list_box.kry` | `.kry-backed` | Layout/navigation policy is `.kry`; host keeps input/scroll sampling. |
| `TreeView` | `UI/Collections` | Tree | `runtime/tree_view.kry` | Partly `.kry-backed` | Row, indent, scroll-window, and text bounds policy are `.kry`; host keeps input, scrollbars, selection mutation, expansion state, and drawing. |
| `TableView` | `UI/Collections` | Table | `runtime/table_view.kry` | Partly `.kry-backed` | Header/body/frozen-row/scroll/cell geometry policy is `.kry`; host keeps column ordering, input, selection mutation, resizing, clipboard, and drawing. |
| `TextArea` | `UI/Collections` | Text area | `runtime/text_input.kry` | Partly `.kry-backed` | Page rows/metrics are `.kry`; editing, IME, selection, and paint still native. |
| `CanvasGrid` | `UI/Collections` | Grid | `runtime/canvas_grid.kry` | `.kry-backed` | Grid spacing and line geometry are `.kry`; host draws. |
| `MenuBar` | `UI/Navigation` | Menu | `runtime/menu.kry` | Partly `.kry-backed` | Geometry is `.kry`; retained open/focus/input state remains native. |
| `PopupMenu` | `UI/Navigation` | Menu | `runtime/menu.kry`, `runtime/popup_policy.kry` | Partly `.kry-backed` | Menu geometry and popup mode are `.kry`; nested input registry remains native. |
| `NavigationBar` | `UI/Navigation` | Tabs | `runtime/navigation_bar.kry` | `.kry-backed` | Paint and sizing policy are `.kry`. |
| `Toolbar` | `UI/Navigation` | Tools | `runtime/toolbar.kry` | `.kry-backed` | Metrics/geometry are `.kry`; host dispatches child actions. |
| `TabBar` | `UI/Navigation` | Tabs | `runtime/tab_bar.kry` | `.kry-backed` | Sizing/scroll policy is `.kry`; host keeps input sampling. |
| `TitleBar` | `UI/Navigation` | Title | `runtime/title_bar.kry` | Partly `.kry-backed` | Layout and title font-fit policy are `.kry`; host keeps dropdown dispatch, text measurement, and leading-action input/rendering. |
| `Focus` | `UI/Overlays` | Focus | `runtime/focus.kry` | Partly `.kry-backed` | Ring geometry policy is `.kry`; host keeps focus state, registration, and drawing. |
| `FocusDebugOverlay` | `UI/Overlays` | Debug | `runtime/focus.kry` | Partly `.kry-backed` | Debug overlay bounds and label placement are `.kry`; host keeps accessibility snapshot sampling and drawing. |
| `TransitionFade` | `UI/Overlays` | Transition | `runtime/transition_fade.kry` | Partly `.kry-backed` | Alpha/easing policy is `.kry`; host keeps state mutation and fade rectangle drawing. |
| `Modal` | `UI/Overlays` | Dialog | `runtime/modal.kry` | Partly `.kry-backed` | Layout/action sizing policy is `.kry`; host keeps modal input layer, text editing, and drawing. |
| `Scene` | `Game2D/Core` | Scene root | missing | Game2D native scene | Separate Game2D surface; introduce `.kry` scene declarations later. |
| `Node2D` | `Game2D/Core` | Transform | missing | Game2D native scene | Separate Game2D surface. |
| `Camera2D` | `Game2D/Core` | Camera | missing | Game2D native scene | Separate Game2D surface. |
| `Sprite2D` | `Game2D/Rendering` | Image | missing | Game2D native scene | Separate Game2D surface. |
| `AnimatedSprite2D` | `Game2D/Rendering` | Animation | missing | Game2D native scene | Separate Game2D surface. |
| `TileMap` | `Game2D/Rendering` | Tiles | missing | Game2D native scene | Separate Game2D surface. |
| `CollisionShape2D` | `Game2D/Physics` | Collider | missing | Game2D native scene | Separate Game2D surface. |
| `Area2D` | `Game2D/Physics` | Trigger | missing | Game2D native scene | Separate Game2D surface. |
| `Body2D` | `Game2D/Physics` | Body | missing | Game2D native scene | Separate Game2D surface. |
| `Timer` | `Game2D/Runtime` | Timer | missing | Game2D native scene | Separate Game2D surface. |
| `AudioSource` | `Game2D/Audio` | Sound | missing | Game2D native scene | Separate Game2D surface. |
| `Light2D` | `Game2D/Rendering` | Point light | missing | Game2D native scene | Separate Game2D surface. |

`Combo` is not a canonical public concept. The final surface has three clear
names instead:

| Concept | Canonical name | Notes |
|---|---|---|
| Choose one option from a list | `Dropdown` | No arbitrary child content. |
| Arbitrary anchored/floating content | `Popup` | Includes tooltip, modal, and context variants through props. |
| Command lists and menu bars | `Menu` / `MenuBar` / `PopupMenu` / `ContextMenu` | Command semantics, accelerators, and submenu behavior. |

`BeginCombo`, `EndCombo`, `CloseCombo`, `ComboProps`, and `ComboFlags` have
been removed from the public surface. Existing generated fixtures use
`Dropdown` for option selection and `Popup` for arbitrary child content.

## Core Drawing And Text

| Public name | Current decision | Notes |
|---|---|---|
| `Background` | `.kry canonical` | Viewport bounds policy is in `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `.kry canonical` | Single canonical signature should be `Text(TextProps)`. |
| `Paragraph` | `.kry canonical` | Metrics/default policy is in `.kry`; may become `Text` variant if props cover paragraph layout. |
| `Rect` | Native canonical | Bounds policy is in `.kry`; still review against `Box` or surface props. |
| `Box` | Native canonical | Rectangle primitive with `Rectangle` bounds; keep only if it remains clearer than `Rect` props. |
| `Circle` | Native canonical | Primitive drawing node; replaces raylib-style `DrawCircleV` in `.kry` surface. |
| `Ring` | Native canonical | Primitive drawing node; replaces raylib-style `DrawRing` in `.kry` surface. |
| `Line` | Native canonical | Endpoint and retained-bounds policy are in `.kry`; host keeps stroke drawing. |
| `Triangle` | Native canonical | Primitive drawing node. |
| `Bevel` | Native canonical | Primitive drawing effect unless replaced by surface props. |
| `Icon` | `.kry canonical` | Bounds/size policy is in `.kry`; icon sheet/type surface is `IconType` with C `ICON_*` values and Go `kr.IconHome`-style constants. |
| `Image` | `.kry canonical` | Canonical image widget. |

## Controls

| Public name | Current decision | Notes |
|---|---|---|
| `Card` | `.kry canonical` | Already has `.kry` module. |
| `Button` | `.kry canonical` | Single public button surface. Menu, split-action, icon-only, arrow, info/help, loading, disclosure, tone, and emphasis behavior should live in `ButtonProps` or small `.kry` composition, not separate public widget names. |
| `InvisibleButton` | Native support | Hit-test primitive; disabled policy is in `.kry`; not a design widget. |
| `Link` | `.kry canonical` | Canonical public name for URL/link activation; color/hover/disabled policy is in `.kry`, URL dispatch remains host support. |
| `TextField` | `.kry canonical` | Metrics and horizontal scroll policy are in `.kry`; editing, IME, selection, and rendering remain native host support. |
| `TextArea` | `.kry canonical` | Metrics and page-row policy are in `.kry`; editing, IME, selection, and rendering remain native host support. |
| `Dropdown` | `.kry canonical` | Already has `.kry` module. |
| `Combo` | Removed | Old arbitrary-popup name; use `Dropdown`, `Popup`, or `Menu`. |
| `BeginCombo` | Removed | Immediate-mode compatibility name deleted from the public surface. |
| `EndCombo` | Removed | Immediate-mode compatibility name deleted from the public surface. |
| `CloseCombo` | Removed | Immediate-mode compatibility name deleted from the public surface. |
| `Slider` | `.kry canonical` | Value type, orientation, and angle/unit live in `SliderProps`; generated Go uses `kr.Slider`. |
| `Drag` | `.kry canonical` | Value type and range mode live in `DragProps`; generated Go uses `kr.Drag`. |
| `Input` | `.kry canonical` | Value type, values, and step policy live in `InputProps`; generated Go uses `kr.Input`. |
| `Spinbox` | `.kry canonical` | Layout and value stepping policy are in `.kry`; host handles button input and drawing. |
| `Toggle` | `.kry canonical` | Public surface is `Toggle(ToggleProps)`; paint/layout policy is in `.kry`, host handles input and drawing. |
| `Checkbox` | `.kry canonical` | Paint, layout, and flags toggle policy are in `.kry`; host handles input and drawing. |
| `Radio` | `.kry canonical` | Paint, layout, and marker text policy are in `.kry`; host handles focus/input and drawing. |
| `Selectable` | `.kry canonical` | Paint/layout policy is in `.kry`; review whether list item props should absorb it later. |
| `Progress` | `.kry canonical` | Prefer one public progress name. |
| `ColorPicker` | `.kry canonical` | Channel layout and color conversion are in `.kry`; swatch activation is `Button` with swatch props. |
| `SegmentedControl` | `.kry canonical` | Layout policy is in `.kry`; host handles label measurement, focus/input, and button drawing. |

## Layout And Containers

| Public name | Current decision | Notes |
|---|---|---|
| `Column` | `.kry canonical` | Content and child placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Row` | `.kry canonical` | Content and child placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Grid` | `.kry canonical` | Metrics, columns, and cursor placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Stack` | `.kry canonical` | Content/child fill policy is in `.kry`; host keeps retained tree scope ownership. |
| `Screen` | `.kry canonical` | Top-level screen container; viewport fallback bounds policy is in `.kry`. |
| `Group` | `.kry canonical` | Non-layout grouping scope. Bounds/content policy is in `.kry`; host keeps retained tree scope ownership. |
| `Separator` | `.kry canonical` | Line, label, and bullet layout/paint policy are in `.kry`; host handles text measurement and drawing. |
| `Fieldset` | `.kry canonical` | Canonical titled border group; replaces old `LabelFrame` spelling. |
| `LabelFrame` | Removed | Old spelling for `Fieldset`; no longer accepted as a public widget name. |
| `PanedView` | `.kry canonical` | Split clamp and handle geometry are in `.kry`; host keeps drag/input ownership. |
| `Collapsible` | `.kry canonical` | Header metrics, geometry, and marker text are in `.kry`; host keeps input, focus, tree navigation, and drawing. |
| `Scroll` | `.kry canonical` | Lexical scroll-content block. Measurement and sizing policy are in `.kry`; host keeps wheel/drag/clipping and lowered scope ownership. |
| `TableCell` | `.kry canonical` | Lexical custom table-cell block; lowers to host cell scope. |
| `BeginScroll` | Native support | Lowered host entry for `.kry` `Scroll` blocks; not a separate public widget name. |
| `EndScroll` | Native support | Lowered host exit for `.kry` `Scroll` blocks; not a separate public widget name. |
| `BeginTableCell` | Native support | Lowered host entry for `.kry` `TableCell` blocks; not a separate public widget name. |
| `EndTableCell` | Native support | Lowered host exit for `.kry` `TableCell` blocks; not a separate public widget name. |

## Collections And Editors

| Public name | Current decision | Notes |
|---|---|---|
| `ListBox` | `.kry canonical` | Layout/navigation policy is in `.kry`; host handles input sampling, scroll scope, and drawing. |
| `MultiSelectList` | `.kry canonical` | Row, keyboard navigation, and selection policy are in `.kry`; could become `ListBox` selection props. |
| `TreeView` | `.kry canonical` | Row/window geometry policy is in `.kry`; host handles input, scrollbars, selection mutation, expansion state, and drawing. |
| `TableView` | `.kry canonical` | Header/body/frozen-row/scroll/cell geometry is in `.kry`; host handles column ordering, input, selection mutation, resizing, clipboard, and drawing. |
| `CanvasGrid` | `.kry canonical` | Grid spacing, line counts, and line rectangles are in `.kry`; host handles drawing. |
| `Canvas` | `.kry canonical` | Transform, hit-test, and result policy are in `.kry`; host keeps clip/camera renderer scope. |
| `BeginCanvas` | Native support | Lowered host entry for `.kry` `Canvas` blocks; not a separate public widget name. |
| `EndCanvas` | Native support | Lowered host exit for `.kry` `Canvas` blocks; not a separate public widget name. |
| `DragDropSource` | Native support | Decision policy is in `.kry`; host keeps payload storage, type comparison, and pointer ownership. |
| `DragDropTarget` | Native support | Decision/copy-size policy is in `.kry`; host keeps payload storage, type comparison, output copy, and pointer ownership. |

## Navigation

| Public name | Current decision | Notes |
|---|---|---|
| `NavigationBar` | `.kry canonical` | Already has `.kry` module. |
| `Toolbar` | `.kry canonical` | Metrics and geometry policy are in `.kry`; host handles input, drawing, and child `Button`/`Dropdown` calls. |
| `MenuBar` | `.kry canonical` | Menu geometry is in `.kry`; retained open/focus/input state still native host support. |
| `PopupMenu` | `.kry canonical` | Menu geometry is in `.kry`; retained focus/input state still native host support. |
| `ContextMenu` | `.kry canonical` | Menu geometry is in `.kry`; retained trigger/open/input state still native host support. |
| `TabBar` | `.kry canonical` | Sizing/scroll policy is in `.kry`; host handles input sampling, drag state, and drawing. |
| `TitleBar` | `.kry canonical` | Layout policy is in `.kry`; leading action and dropdown behavior live in `TitleBarProps`. |
| `Router` | Native support | Navigation runtime, not a visual widget. |
| `Link` | `.kry canonical` | Canonical navigation/link widget. |

## Overlays And Feedback

| Public name | Current decision | Notes |
|---|---|---|
| `Popup` | `.kry canonical` | Arbitrary anchored/floating content. Mode/input policy is in `.kry`; host handles pointer sampling, paint layers, clipping, and child content. |
| `BeginPopup` | Native support | Lowered host entry for `.kry` `Popup` blocks; not a separate public widget name. |
| `EndPopup` | Native support | Lowered host entry for `.kry` `Popup` blocks; not a separate public widget name. |
| `ClosePopup` | Native support | Explicit close operation for the active `.kry` `Popup` block. |
| `Modal` | `.kry canonical` | Layout/action sizing policy is in `.kry`; host handles capture, input, text editing, and drawing. |
| `Toast` | `.kry canonical` | Duration and layout policy are in `.kry`; host keeps message storage, timing source, truncation, and drawing. |
| `Focus` | Native support | Focus ring geometry is in `.kry`; focus state remains host support. |
| `FocusDebugOverlay` | Native support | Debug overlay bounds and label placement are in `.kry`; accessibility snapshot sampling remains host support. |
| `Guide` | `.kry canonical` | Public guided overlay flow. Steps, tip placement, scrim cutout, nav button layout, and step transition policy are in `.kry`; host keeps target lookup, text measurement, input sampling, and drawing. |
| `GuideStep` | `.kry canonical` | One anchored instruction inside `Guide`; replaces `UIGuideStep` as the public spelling. |
| `GuidePager` | Internal/lowered support | Not a clean public widget. Footer layout/page transition policy is `.kry`; the C helper lives under `src/ui` and is not exported by `kryon.h`/`ui.h`. |
| `TransitionFade` | `.kry canonical` | Alpha/easing policy is in `.kry`; host keeps state mutation and fade rectangle drawing. |

## Composite And App-Level Candidates

These names are in the registry, but each should prove it is reusable Kryon
surface before staying public:

| Public name | Current decision | Notes |
|---|---|---|

## Game2D Nodes

Game2D has its own node family. These are canonical for the Game2D domain, but
they should stay separate from general UI widgets.

| Public name | Current decision | Notes |
|---|---|---|
| `Scene` | `.kry canonical` | Root game scene. |
| `Node2D` | `.kry canonical` | Base 2D node. |
| `Camera2D` | `.kry canonical` | Camera node. |
| `Sprite2D` | `.kry canonical` | Sprite node. |
| `AnimatedSprite2D` | `.kry canonical` | Animated sprite node. |
| `TileMap` | `.kry canonical` | Tile map node. |
| `CollisionShape2D` | `.kry canonical` | Collision shape node. |
| `Area2D` | `.kry canonical` | Trigger/area node. |
| `Body2D` | `.kry canonical` | Physics/body node. |
| `Timer` | `.kry canonical` | Timer node. |
| `AudioSource` | `.kry canonical` | Audio playback node. |
| `Light2D` | `.kry canonical` | Light node. |
| `Custom` | Native support | Escape hatch, not preferred public design surface. |

## Retained Node Kinds

These are the current `WidgetKind` values in the retained UI tree. They are
runtime/node names, not necessarily final public widget constructor names. Use
this table for naming feedback before we lock the clean surface.

| Current node kind | Public widget/concept | Decision |
|---|---|---|
| `WIDGET_SCREEN` | `Screen` | `.kry canonical`; viewport fallback bounds policy is `.kry-backed` |
| `WIDGET_BACKGROUND` | `Background` | Partly `.kry-backed` |
| `WIDGET_TEXT` | `Text` | `.kry canonical` |
| `WIDGET_RECT` | `Rect` / `Box` | Rename review; geometry is `.kry-backed` |
| `WIDGET_CIRCLE` | `Circle` | Native canonical |
| `WIDGET_RING` | `Ring` | Native canonical |
| `WIDGET_LINE` | `Line` | Partly `.kry-backed` |
| `WIDGET_TRIANGLE` | `Triangle` | Native canonical |
| `WIDGET_BUTTON` | `Button` | `.kry canonical` |
| `WIDGET_TEXT_FIELD` | `TextField` | `.kry canonical`; editing policy still migrating |
| `WIDGET_TEXT_AREA` | `TextArea` | `.kry canonical`; editing policy still migrating |
| `WIDGET_DROPDOWN` | `Dropdown` | `.kry canonical` |
| `WIDGET_SLIDER` | `Slider` | `.kry canonical` |
| `WIDGET_TOGGLE` | `Toggle` | `.kry canonical` |
| `WIDGET_CHECKBOX` | `Checkbox` | `.kry canonical` |
| `WIDGET_PARAGRAPH` | `Paragraph` | Rename review; may become `Text` props |
| `WIDGET_READONLY_TEXT_BOX` | `ReadonlyTextBox` | Rename review; likely `TextArea` props |
| `WIDGET_NAVIGATION_BAR` | `NavigationBar` | `.kry canonical` |
| `WIDGET_TAB_BAR` | `TabBar` | `.kry canonical` |
| `WIDGET_PARAGRAPH_MODAL` | `ParagraphModal` | Composite candidate |
| `WIDGET_TITLE_BAR` | `TitleBar` | `.kry canonical` |
| `WIDGET_GROUP` | `Group` | `.kry canonical`; bounds/content policy is `.kry-backed` |
| `WIDGET_COLUMN` | `Column` | `.kry canonical`; placement policy is `.kry-backed` |
| `WIDGET_ROW` | `Row` | `.kry canonical`; placement policy is `.kry-backed` |
| `WIDGET_STACK` | `Stack` | `.kry canonical`; placement policy is `.kry-backed` |
| `WIDGET_GRID` | `Grid` | `.kry canonical`; metrics and cursor placement policy are `.kry-backed` |
| `WIDGET_IMAGE` | `Image` | `.kry canonical` |
| `WIDGET_CUSTOM` | `Custom` | Native support escape hatch |
| `WIDGET_DRAG` | `Drag` | `.kry canonical` |
| `WIDGET_TEXT_INPUT_PAINT` | `TextInputPaint` | Native support; internal paint node |
| `WIDGET_ROUTER` | `Router` | Native support |
| `WIDGET_CARD` | `Card` | `.kry canonical` |

Recent retained-tree public C cleanup:

| Old public name | Current name |
|---|---|
| `UIEventKind`, `UI_EVENT_*` | `EventKind`, `EVENT_*` |
| `UIEvent` | `Event` |
| `UIInvalidation`, `UI_INVALIDATE_*` | `Invalidation`, `INVALIDATE_*` |
| `UIWidgetKind`, `UI_WIDGET_*_NODE` | `WidgetKind`, `WIDGET_*` |
| `UIWidgetNode` | `WidgetNode` |
| `UIWidgetData` | `WidgetData` |
| `UIWidgetTextInputPaint` | `WidgetTextInputPaint` |
| `UIAccessibilitySink` | `AccessibilitySink` |
| `UIInspect*`, `BeginUIInspect*`, `PushUIInspect*`, `IsUIInspectActive` | `Inspect*`, `BeginInspect*`, `PushInspect*`, `IsInspectActive` |
| `UIDPIState`, `UI_DPI_BASE_*`, `InitUIDPI`, `GetUIDPI*` | `DPIState`, `DPI_BASE_*`, `InitDPI`, `GetDPI*` |
| `UIFrameState`, `InitUI`, `BeginUIFrame`, `EndUIFrame` | `FrameState`, `InitInterface`, `BeginInterfaceFrame`, `EndInterfaceFrame` |
| `SetUI*`, `GetUI*`, `IsUI*`, `ClearUI*`, `PushUI*`, `PopUI*` focus/input helpers | `Set*`, `Get*`, `Is*`, `Clear*`, `Push*`, `Pop*` focus/input helpers |
| `UIFont*`, `UI_FONT_*`, `RegisterUISmallFont` | `TextFont*`, `TEXT_FONT_*`, `RegisterSmallTextFont` |
| `UIClipboard*`, `UI_CLIPBOARD_*`, `UIPrimarySelection*` | `Clipboard*`, `CLIPBOARD_*`, `PrimarySelection*` |
| `ProfilePicture*`, `UISyncProfileIcon`, `UI_SYNC_PROFILE_ICON_*` | `ProfileImage*`, `SyncProfileIcon`, `SYNC_PROFILE_ICON_*` |
| `SetUIViewSize`, `GetUIViewWidth`, `GetUIViewHeight` | `SetViewSize`, `GetViewWidth`, `GetViewHeight` |
| `GetUICenteredColumn`, `GetUIPageSidePadding` | `GetCenteredColumn`, `GetPageSidePadding` |
| `SetUIScale`, `GetUIScale`, `ClampUIPx` | `SetScale`, `GetScale`, `ClampPx` |
| `LightenUIColor`, `DarkenUIColor` | `LightenColor`, `DarkenColor` |
| `BeginUIClip`, `EndUIClip`, `ResetUIClip`, `GetUIClip*` | `BeginClip`, `EndClip`, `ResetClip`, `GetClip*` |
| `UIFloatDrag*`, `UIIntDrag*`, `UIFloatSlider*`, `UIIntSlider*`, typed fixture values | `DragScalar*`, `DragWhole*`, `SliderScalar*`, `SliderWhole*`, neutral fixture values |
| `BeginTabBar`, `BeginTabItem`, `EndTabItem`, `EndTabBar` | `TabBar` plus caller-owned selected state and ordinary conditionals |
| direct `.kry` `BeginScroll`/`EndScroll` and `BeginTableCell`/`EndTableCell` calls | `Scroll` and `TableCell` lexical blocks |
| direct `.kry` `BeginCanvas`/`EndCanvas` calls | `Canvas` lexical block |

`include/*.h` and `docs/PUBLIC_API_SNAPSHOT.txt` are guarded by
`canonical-surface-test`: public `UI*`/`UI_*` prefixes are not accepted there.

## Cleanup Queue

1. Finish porting high-use native controls into `.kry`: `TextField` and
   `TextArea` editing/composition policy.
2. Remove other compatibility names after migrations: immediate-mode
   `Begin*`/`End*` wrappers from public `.kry` documentation. Tutorial image
   helpers remain internal and should route through canonical `Image` policy.
3. Keep `docs/IMGUI_WIDGET_COVERAGE.md` as the coverage audit. Use this file
   as the naming and migration review surface.
