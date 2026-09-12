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
| `runtime/button.kry` | Button composition | `.kry canonical` |
| `runtime/button_props.kry` | Button props | `.kry canonical` |
| `runtime/canvas_grid.kry` | CanvasGrid line policy | `.kry canonical` |
| `runtime/card.kry` | Card composition | `.kry canonical` |
| `runtime/card_props.kry` | Card props | `.kry canonical` |
| `runtime/checkbox.kry` | Checkbox paint/layout policy | `.kry canonical` |
| `runtime/color_picker.kry` | ColorPicker channel layout and color policy | `.kry canonical` |
| `runtime/control_props.kry` | Shared control props | `.kry canonical` |
| `runtime/dropdown.kry` | Dropdown composition | `.kry canonical` |
| `runtime/drag.kry` | Drag value/keyboard policy | `.kry canonical` |
| `runtime/grid.kry` | Grid composition | `.kry canonical` |
| `runtime/grid_props.kry` | Grid props | `.kry canonical` |
| `runtime/input.kry` | Input value/step policy | `.kry canonical` |
| `runtime/input_props.kry` | Input props | `.kry canonical` |
| `runtime/fieldset.kry` | Fieldset layout/paint policy | `.kry canonical` |
| `runtime/list_box.kry` | ListBox layout/navigation policy | `.kry canonical` |
| `runtime/material.kry` | Material layer assembly | `.kry canonical` |
| `runtime/menu.kry` | Menu metrics and geometry policy | `.kry canonical` |
| `runtime/multi_select_list.kry` | MultiSelectList row/navigation/selection policy | `.kry canonical` |
| `runtime/navigation_bar.kry` | Navigation bar composition | `.kry canonical` |
| `runtime/paint.kry` | Paint/drawing helpers | Native support |
| `runtime/plot.kry` | Plot geometry and text policy | `.kry canonical` |
| `runtime/popup_policy.kry` | Popup mode/input policy | `.kry canonical` |
| `runtime/progress.kry` | Progress layout policy | `.kry canonical` |
| `runtime/radio.kry` | Radio paint/layout policy | `.kry canonical` |
| `runtime/segmented_control.kry` | SegmentedControl layout policy | `.kry canonical` |
| `runtime/selectable.kry` | Selectable paint/layout policy | `.kry canonical` |
| `runtime/separator.kry` | Separator/Bullet layout and paint policy | `.kry canonical` |
| `runtime/slider.kry` | Slider composition plus value/keyboard policy | `.kry canonical` |
| `runtime/spinbox.kry` | Spinbox layout/value policy | `.kry canonical` |
| `runtime/style.kry` | Style helpers | `.kry canonical` |
| `runtime/surface.kry` | Surface/container helpers | `.kry canonical` |
| `runtime/tab_bar.kry` | TabBar sizing/scroll policy | `.kry canonical` |
| `runtime/text.kry` | Text composition | `.kry canonical` |
| `runtime/text_input.kry` | TextField/TextArea metrics and scroll policy | `.kry canonical` |
| `runtime/theme.kry` | Theme data/helpers | `.kry canonical` |
| `runtime/toggle.kry` | Toggle composition | `.kry canonical` |
| `runtime/toolbar.kry` | Toolbar metrics and geometry policy | `.kry canonical` |

## Current Implementation Audit

This is the current migration truth, not the desired final state. A name in the
registry is only considered `.kry`-backed when its reusable behavior, props, or
layout policy lives in `runtime/*.kry` and native code only adapts host input,
text measurement, painting, storage, or platform services.

| Group | `.kry`-backed today | Still native-only or compatibility |
|---|---|---|
| Text and drawing | `Text` style resolution, `Image` canonical props/name, clean drawing primitive names (`Box`, `Circle`, `Ring`, `Triangle`) | `Background`, `Paragraph`, `Rect`, `Line`, `Bevel`, `Icon` |
| Actions | `Button`, `Card`, `Link`, `Button` menu/split/arrow/info options, `InvisibleButton` disabled policy | helper button variants belong in `ButtonProps` or composition |
| Inputs | `Checkbox`, `Dropdown`, `Progress`, `Radio`, `SegmentedControl`, `Selectable`, `Slider`, `Spinbox`, `TextField`/`TextArea` metrics, `Toggle`, `Button` swatch props, `ColorPicker` layout/color policy | text composition/editing host support |
| Layout | `Grid`, `Fieldset` layout policy, `Separator`, shared `Surface`/`Style`/`Material` policy | `Column`, `Row`, `Stack`, `Screen`, `Group`, `PanedView`, `Collapsible`, scroll/list/table begin-end wrappers |
| Collections | `CanvasGrid`, `ListBox` layout/navigation policy, `MultiSelectList` row/navigation/selection policy, `Plot` geometry policy | `TreeView`, `TableView`, `Canvas`, drag/drop wrappers |
| Navigation | `NavigationBar` paint policy, `TabBar` sizing/scroll policy, `Toolbar` metrics/geometry policy, menu geometry policy | `MenuBar`, `PopupMenu`, `ContextMenu` retained state/input, title bars, router/link helpers |
| Overlays | `Popup` mode/input policy | dialogs, `Modal`, toast, theme pickers, focus/guide/tutorial overlays, transition helpers |
| Game2D | Native scene nodes | Game2D nodes are separate from UI widgets; keep them in the Game2D runtime unless `.kry` scene declarations are introduced. |

The immediate migration target is to finish moving high-use controls first:
`TextField` and `TextArea` editing/composition policy. Once those are backed by `.kry`, collapse the
native suffix variants into props on the canonical widgets.

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
| `Background` | Native canonical | Primitive scene/background fill. |
| `Text` | `.kry canonical` | Single canonical signature should be `Text(TextProps)`. |
| `Paragraph` | `.kry canonical` | May become `Text` variant if props cover paragraph layout. |
| `Rect` | Native canonical | Primitive drawing node. |
| `Box` | Native canonical | Rectangle primitive with `Rectangle` bounds; keep only if it remains clearer than `Rect` props. |
| `Circle` | Native canonical | Primitive drawing node; replaces raylib-style `DrawCircleV` in `.kry` surface. |
| `Ring` | Native canonical | Primitive drawing node; replaces raylib-style `DrawRing` in `.kry` surface. |
| `Line` | Native canonical | Primitive drawing node. |
| `Triangle` | Native canonical | Primitive drawing node. |
| `Bevel` | Native canonical | Primitive drawing effect unless replaced by surface props. |
| `Icon` | `.kry canonical` | User-facing icon widget. |
| `Image` | `.kry canonical` | Canonical image widget. |

## Controls

| Public name | Current decision | Notes |
|---|---|---|
| `Card` | `.kry canonical` | Already has `.kry` module. |
| `Button` | `.kry canonical` | Single public button surface. Menu, split-action, icon-only, arrow, info/help, loading, disclosure, tone, and emphasis behavior should live in `ButtonProps` or small `.kry` composition, not separate public widget names. |
| `InvisibleButton` | Native support | Hit-test primitive; disabled policy is in `.kry`; not a design widget. |
| `Link` | `.kry canonical` | Canonical public name for URL/link activation. |
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
| `Radio` | `.kry canonical` | Paint/layout policy is in `.kry`; host handles focus/input and drawing. |
| `Selectable` | `.kry canonical` | Paint/layout policy is in `.kry`; review whether list item props should absorb it later. |
| `Progress` | `.kry canonical` | Prefer one public progress name. |
| `ColorPicker` | `.kry canonical` | Channel layout and color conversion are in `.kry`; swatch activation is `Button` with swatch props. |
| `SegmentedControl` | `.kry canonical` | Layout policy is in `.kry`; host handles label measurement, focus/input, and button drawing. |

## Layout And Containers

| Public name | Current decision | Notes |
|---|---|---|
| `Column` | `.kry canonical` | Core layout container. |
| `Row` | `.kry canonical` | Core layout container. |
| `Grid` | `.kry canonical` | Already has `.kry` module. |
| `Stack` | `.kry canonical` | Core layered layout container. |
| `Screen` | `.kry canonical` | Top-level screen container. |
| `Group` | Native support | Low-level grouping; review public need. |
| `Separator` | `.kry canonical` | Line, label, and bullet layout/paint policy are in `.kry`; host handles text measurement and drawing. |
| `Fieldset` | `.kry canonical` | Canonical titled border group; replaces old `LabelFrame` spelling. |
| `LabelFrame` | Removed | Old spelling for `Fieldset`; no longer accepted as a public widget name. |
| `PanedView` | `.kry canonical` | Split-pane layout component. |
| `Collapsible` | `.kry canonical` | Disclosure container. |
| `BeginScroll` | Native support | Compatibility wrapper; canonical API should be declarative. |
| `EndScroll` | Native support | Compatibility wrapper; canonical API should be declarative. |
| `BeginTableCell` | Native support | Compatibility wrapper; table cell should be declarative. |
| `EndTableCell` | Native support | Compatibility wrapper; table cell should be declarative. |

## Collections And Editors

| Public name | Current decision | Notes |
|---|---|---|
| `ListBox` | `.kry canonical` | Layout/navigation policy is in `.kry`; host handles input sampling, scroll scope, and drawing. |
| `MultiSelectList` | `.kry canonical` | Row, keyboard navigation, and selection policy are in `.kry`; could become `ListBox` selection props. |
| `TreeView` | `.kry canonical` | Hierarchical collection. |
| `TableView` | `.kry canonical` | Tabular collection. |
| `CanvasGrid` | `.kry canonical` | Grid spacing, line counts, and line rectangles are in `.kry`; host handles drawing. |
| `Canvas` | `.kry canonical` | General drawing/editing surface. |
| `DragDropSource` | Native support | Behavior primitive. |
| `DragDropTarget` | Native support | Behavior primitive. |

## Navigation

| Public name | Current decision | Notes |
|---|---|---|
| `NavigationBar` | `.kry canonical` | Already has `.kry` module. |
| `Toolbar` | `.kry canonical` | Metrics and geometry policy are in `.kry`; host handles input, drawing, and child `Button`/`Dropdown` calls. |
| `MenuBar` | `.kry canonical` | Menu geometry is in `.kry`; retained open/focus/input state still native host support. |
| `PopupMenu` | `.kry canonical` | Menu geometry is in `.kry`; retained focus/input state still native host support. |
| `ContextMenu` | `.kry canonical` | Menu geometry is in `.kry`; retained trigger/open/input state still native host support. |
| `TabBar` | `.kry canonical` | Sizing/scroll policy is in `.kry`; host handles input sampling, drag state, and drawing. |
| `BeginTabBar` | Native support | Immediate-mode compatibility. |
| `BeginTabItem` | Native support | Immediate-mode compatibility. |
| `EndTabItem` | Native support | Immediate-mode compatibility. |
| `EndTabBar` | Native support | Immediate-mode compatibility. |
| `TitleBar` | `.kry canonical` | Window/page title surface; leading action and dropdown behavior live in `TitleBarProps`. |
| `Router` | Native support | Navigation runtime, not a visual widget. |
| `Link` | `.kry canonical` | Canonical navigation/link widget. |

## Overlays And Feedback

| Public name | Current decision | Notes |
|---|---|---|
| `Popup` | `.kry canonical` | Arbitrary anchored/floating content. Mode/input policy is in `.kry`; host handles pointer sampling, paint layers, clipping, and child content. |
| `BeginPopup` | Native support | Lowered host entry for `.kry` `Popup` blocks; not a separate public widget name. |
| `EndPopup` | Native support | Lowered host entry for `.kry` `Popup` blocks; not a separate public widget name. |
| `ClosePopup` | Native support | Explicit close operation for the active `.kry` `Popup` block. |
| `Modal` | `.kry canonical` | Canonical modal surface. |
| `Toast` | `.kry canonical` | Non-modal feedback. |
| `Focus` | Native support | Focus behavior primitive. |
| `FocusDebugOverlay` | Native support | Debug-only overlay. |
| `TransitionFade` | Native support | Transition primitive. |

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

## Cleanup Queue

1. Finish porting high-use native controls into `.kry`: `TextField` and
   `TextArea` editing/composition policy.
2. Remove other compatibility names after migrations: tutorial helpers
   and immediate-mode `Begin*`/`End*` wrappers from public `.kry`
   documentation.
3. Keep `docs/IMGUI_WIDGET_COVERAGE.md` as the coverage audit. Use this file
   as the naming and migration review surface.
