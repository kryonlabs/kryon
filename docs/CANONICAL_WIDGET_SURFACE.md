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
| `Removed` | Deleted from public headers/codegen/runtime surfaces. |

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
| `runtime/drawing_props.kry` | Shared drawing props | `.kry canonical` |
| `runtime/dropdown.kry` | Dropdown composition | `.kry canonical` |
| `runtime/drag_drop.kry` | DragDrop decision policy | `.kry canonical` |
| `runtime/drag.kry` | Drag value/keyboard policy | `.kry canonical` |
| `runtime/grid.kry` | Grid composition | `.kry canonical` |
| `runtime/grid_props.kry` | Grid props | `.kry canonical` |
| `runtime/group.kry` | Group bounds/content policy | `.kry canonical` |
| `runtime/guide.kry` | Guide overlay layout and step policy | `.kry canonical` |
| `runtime/guide_pager.kry` | Internal pager footer layout and page transition policy | Native support |
| `runtime/icon.kry` | Icon bounds/size policy | `.kry canonical` |
| `runtime/image.kry` | Image fit and placeholder layout policy | `.kry canonical` |
| `runtime/input.kry` | Input value/step policy | `.kry canonical` |
| `runtime/input_props.kry` | Input props | `.kry canonical` |
| `runtime/instance.kry` | Generated widget instance identity helpers | Native support |
| `runtime/fieldset.kry` | Fieldset layout/paint policy | `.kry canonical` |
| `runtime/focus.kry` | Focus ring and debug overlay geometry policy | Native support |
| `runtime/layout.kry` | Column/Row/Stack content and child placement policy | `.kry canonical` |
| `runtime/link.kry` | Link state/color policy | `.kry canonical` |
| `runtime/list_box.kry` | ListBox layout/navigation policy | `.kry canonical` |
| `runtime/material.kry` | Material layer assembly | `.kry canonical` |
| `runtime/menu.kry` | Menu metrics and geometry policy | `.kry canonical` |
| `runtime/list_box_multi.kry` | ListBox multi-selection row/navigation/selection policy | `.kry canonical` |
| `runtime/navigation_bar.kry` | Navigation bar composition | `.kry canonical` |
| `runtime/paint.kry` | Paint/drawing helpers | Native support |
| `runtime/paned_view.kry` | PanedView split/handle geometry policy | `.kry canonical` |
| `runtime/paragraph.kry` | Paragraph metrics/default policy | `.kry canonical` |
| `runtime/plot.kry` | Plot geometry and text policy | `.kry canonical` |
| `runtime/popup_policy.kry` | Popup mode/input policy | `.kry canonical` |
| `runtime/primitive.kry` | Background/Box/Line/Circle/Ring/Triangle primitive geometry policy | `.kry canonical` |
| `runtime/progress.kry` | Progress layout policy | `.kry canonical` |
| `runtime/radio.kry` | Radio paint/layout policy | `.kry canonical` |
| `runtime/segmented_control.kry` | SegmentedControl layout policy | `.kry canonical` |
| `runtime/selectable.kry` | Selectable paint/layout policy | `.kry canonical` |
| `runtime/separator.kry` | Separator/Bullet layout and paint policy | `.kry canonical` |
| `runtime/slider.kry` | Slider composition plus value/keyboard policy | `.kry canonical` |
| `runtime/spinbox.kry` | Spinbox layout/value policy | `.kry canonical` |
| `runtime/scroll.kry` | Scroll measurement/sizing policy | `.kry canonical` |
| `runtime/style.kry` | Style helpers | `.kry canonical` |
| `runtime/style_sheet.kry` | Style sheet evaluation helpers | `.kry canonical` |
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
| Text and drawing | `Text` style resolution, `Paragraph` metrics/default policy, `Background`/`Box`/`Line`/`Circle`/`Ring`/`Triangle` geometry policy, `Bevel` line geometry, `Icon` bounds/size policy, `Image` canonical props/name and placeholder layout, clean drawing primitive names (`Box`, `Circle`, `Ring`, `Triangle`) | icon sheet/drawing host support, paragraph reflow/rendering |
| Actions | `Button`, `Card`, `Link`, `Button` menu/split/arrow/info options | helper button variants belong in `ButtonProps` or composition; invisible hit testing is host support |
| Inputs | `Checkbox`, `Dropdown`, `Progress`, `Radio`, `SegmentedControl`, `Selectable`, `Slider`, `Spinbox`, `TextField`/`TextArea` metrics, `Toggle`, `Button` swatch props, `ColorPicker` layout/color policy | text composition/editing host support |
| Layout | `Column`/`Row`/`Stack` content and child placement policy, `Group` bounds/content policy, `Screen` viewport fallback bounds policy, `Grid`, `Fieldset` layout policy, `PanedView` split geometry, `Collapsible` header geometry, `Separator`, `Scroll` measurement/sizing policy, shared `Surface`/`Style`/`Material` policy | scroll/list/table begin-end wrappers |
| Collections | `Canvas` transform/hit-test policy, `CanvasGrid`, drag/drop decision policy, `ListBox` layout/navigation/multi-selection policy, `Plot` geometry policy, `TreeView` row/window geometry policy, `TableView` layout/scroll geometry policy | drag/drop payload storage |
| Navigation | `NavigationBar` paint policy, `TabBar` sizing/scroll policy, `Toolbar` metrics/geometry policy, `TitleBar` layout policy, `Menu` geometry policy | retained menu open/focus/input state, router/link helpers |
| Overlays | `Popup` mode/input policy, `Focus` ring geometry policy, `Guide` overlay layout/step policy, `Modal` layout/action policy, `Toast` duration/layout policy, transition fade alpha/easing policy | theme pickers |
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
| `Box` | `UI/Display` | Shape | `runtime/primitive.kry` | Partly `.kry-backed` | Registry/public rectangle primitive with `Rectangle` bounds. |
| `Line` | `UI/Display` | Stroke | `runtime/primitive.kry` | Partly `.kry-backed` | Endpoint and retained-bounds policy is `.kry`; host keeps stroke drawing. |
| `Bevel` | `UI/Display` | Relief | `runtime/bevel.kry` | `.kry-backed` | Line geometry is `.kry`; still review whether it should fold into `Surface`/material props. |
| `Icon` | `UI/Display` | Icon | `runtime/icon.kry` | Partly `.kry-backed` | Bounds/size policy is `.kry`; icon sheet/type lookup and drawing remain host support. |
| `Image` | `UI/Display` | Image | `runtime/image.kry` | Partly `.kry-backed` | Fit and placeholder layout policy are `.kry`; cache/loading/drawing remain host support. |
| `Card` | `UI/Input` | Surface action | `runtime/card.kry`, `runtime/card_props.kry` | `.kry-backed` | Card composition and props live in `.kry`. |
| `Button` | `UI/Input` | Action | `runtime/button.kry`, `runtime/button_props.kry` | `.kry-backed` | Single button surface; menu/split/info/icon variants are props/composition. |
| `Link` | `UI/Input` | Link | `runtime/link.kry` | Partly `.kry-backed` | State/color policy is `.kry`; URL dispatch remains host support. |
| `TextField` | `UI/Input` | Input | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics/scroll are `.kry`; editing, IME, selection, and paint still native. |
| `Dropdown` | `UI/Input` | Selection | `runtime/dropdown.kry` | `.kry-backed` | Selection-only control. |
| `Slider` | `UI/Input` | Value | `runtime/slider.kry` | `.kry-backed` | Value type, orientation, and angle/unit are props. |
| `Toggle` | `UI/Input` | On/off | `runtime/toggle.kry` | `.kry-backed` | Host handles input and drawing; paint/layout policy is `.kry`. |
| `Checkbox` | `UI/Input` | Boolean | `runtime/checkbox.kry` | `.kry-backed` | Paint/layout/flag policy is `.kry`. |
| `Radio` | `UI/Input` | Choice | `runtime/radio.kry` | `.kry-backed` | Paint, layout, and marker text policy are `.kry`; host keeps group input. |
| `Progress` | `UI/Input` | Progress | `runtime/progress.kry` | `.kry-backed` | One public progress concept. |
| `Spinbox` | `UI/Input` | Number | `runtime/spinbox.kry` | `.kry-backed` | Layout/step policy is `.kry`; host keeps text/button input. |
| `ColorPicker` | `UI/Input` | Color | `runtime/color_picker.kry` | `.kry-backed` | Channel layout and conversion are `.kry`. |
| `SegmentedControl` | `UI/Input` | Segments | `runtime/segmented_control.kry` | `.kry-backed` | Layout, wrapping, and segment sizing policy are `.kry`; host keeps label measurement, input sampling, and button drawing. |
| `Group` | `UI/Layout` | Container | `runtime/group.kry` | `.kry-backed` | Canonical non-layout grouping scope; bounds/content policy is `.kry`, host keeps retained tree scope ownership. |
| `Separator` | `UI/Layout` | Divider | `runtime/separator.kry` | `.kry-backed` | Line, label, and bullet policy are `.kry`. |
| `Fieldset` | `UI/Layout` | Frame | `runtime/fieldset.kry` | `.kry-backed` | Titled group and border policy are `.kry`. |
| `PanedView` | `UI/Layout` | Split panes | `runtime/paned_view.kry` | Partly `.kry-backed` | Split clamp and handle geometry are `.kry`; host keeps drag/input ownership. |
| `Collapsible` | `UI/Layout` | Section | `runtime/collapsible.kry` | Partly `.kry-backed` | Header metrics, geometry, and marker text are `.kry`; host keeps input, focus, tree navigation, and drawing. |
| `ListBox` | `UI/Collections` | List | `runtime/list_box.kry` | `.kry-backed` | Layout/navigation policy is `.kry`; host keeps input/scroll sampling. |
| `TreeView` | `UI/Collections` | Tree | `runtime/tree_view.kry` | Partly `.kry-backed` | Row, indent, scroll-window, and text bounds policy are `.kry`; host keeps input, scrollbars, selection mutation, expansion state, and drawing. |
| `TableView` | `UI/Collections` | Table | `runtime/table_view.kry` | Partly `.kry-backed` | Header/body/frozen-row/scroll/cell geometry policy is `.kry`; host keeps column ordering, input, selection mutation, resizing, clipboard, and drawing. |
| `TextArea` | `UI/Collections` | Text area | `runtime/text_input.kry` | Partly `.kry-backed` | Page rows/metrics are `.kry`; editing, IME, selection, and paint still native. |
| `CanvasGrid` | `UI/Collections` | Grid | `runtime/canvas_grid.kry` | `.kry-backed` | Grid spacing and line geometry are `.kry`; host draws. |
| `Menu` | `UI/Navigation` | Menu | `runtime/menu.kry` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props. |
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
| `Box` | `.kry canonical` | Canonical rectangle primitive with `Rectangle` bounds. |
| `Line` | `.kry canonical` | Endpoint and retained-bounds policy are in `.kry`; host keeps stroke drawing. |
| `Bevel` | Native canonical | Drawing effect unless material/surface props absorb it. |
| `Icon` | `.kry canonical` | Icon bounds/size policy is in `.kry`. |
| `Image` | `.kry canonical` | Canonical image widget; replaces old picture naming. |
| `Button` | `.kry canonical` | One button surface; variants belong in props/composition. |
| `Card` | `.kry canonical` | Surface action/card composition. |
| `Selectable` | `.kry canonical` | Selectable row/action surface. |
| `Bullet` | `.kry canonical` | Small list/text marker primitive. |
| `Separator` | `.kry canonical` | Divider primitive. |
| `Link` | `.kry canonical` | Canonical link activation name. |
| `TextField` | `.kry canonical` | Metrics/scroll policy in `.kry`; editing host support remains. |
| `TextArea` | `.kry canonical` | Metrics/page policy in `.kry`; editing host support remains. |
| `Dropdown` | `.kry canonical` | Selection control only. |
| `SegmentedControl` | `.kry canonical` | Segmented choice control; layout/wrapping policy is in `.kry`, generated Go uses `kr.SegmentedControl`. |
| `Slider` | `.kry canonical` | Type/orientation/angle variants are props. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props. KSS uses `Menu`, `MenuItem`, and `MenuSeparator`; no `MenuBar` selector. |
| `Toggle` | `.kry canonical` | Boolean switch. |
| `Checkbox` | `.kry canonical` | Boolean checkbox. |
| `Radio` | `.kry canonical` | Choice control. |
| `Progress` | `.kry canonical` | One progress concept. |
| `Plot` | `.kry canonical` | Plot geometry/text policy lives in `.kry`. |
| `Drag` | `.kry canonical` | Numeric drag value control; value type/count are props. |
| `Input` | `.kry canonical` | Numeric input control; value type/count are props. |
| `Spinbox` | `.kry canonical` | Numeric stepper. |
| `DragDrop` | `.kry canonical` | Typed source/target roles are selected through props. |
| `ListBox` multi-selection | `.kry canonical` | Use `ListBoxProps.selected`, `selected_count`, and `anchor`; no separate public widget name. |
| `Screen` | `.kry canonical` | Top-level screen container. |
| `Column` | `.kry canonical` | Layout block. |
| `Row` | `.kry canonical` | Layout block. |
| `Stack` | `.kry canonical` | Layout block. |
| `End` | Native support | Lowered/parser block close marker, not a widget. |
| `Scroll` | `.kry canonical` | Lexical scroll-content block. |
| `Canvas` | `.kry canonical` | Lexical canvas block. |
| `Modal` | `.kry canonical` | Dialog/overlay layout surface. |
| `TitleBar` | `.kry canonical` | Title/action bar. |
| `TabBar` | `.kry canonical` | Tab navigation surface. |
| `NavigationBar` | `.kry canonical` | App navigation bar. |
| `Toolbar` | `.kry canonical` | Tool/action strip. |
| `Toast` | `.kry canonical` | Toast feedback command; message and duration live in `ToastProps`. |
| `Fieldset` | `.kry canonical` | Titled frame/group. |
| `PanedView` | `.kry canonical` | Split panes. |
| `Collapsible` | `.kry canonical` | Collapsible section. |
| `ListBox` | `.kry canonical` | List selection/navigation. |
| `TreeView` | `.kry canonical` | Tree rows/window policy in `.kry`; host keeps state/input. |
| `TableView` | `.kry canonical` | Table geometry policy in `.kry`; host keeps state/input. |
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
| `Heading` | `.kry canonical` | Semantic page heading block. |
| `ParagraphText` | `.kry canonical` | Semantic page paragraph block. |
| `Link` | `.kry canonical` | Link block. |
| `Flow` | `.kry canonical` | Page flow block. |
| `Grid` | `.kry canonical` | Grid layout block. |

## Native Public Compatibility Exports

These names are still exported by `include/ui_tree.h`, but they are not clean
widget concepts. They are compatibility or lowered host support that should
disappear from the public surface once the canonical `.kry` surface owns the
behavior.

| Export | Replacement concept | Removal note |
|---|---|---|
| `BeginButton` | `Button` block | Lowered host entry for composed button content. |
| `BeginCard` | `Card` block | Lowered host entry for composed card content. |
| `BeginDisabled` | `Disabled` block | Lowered host entry for disabled lexical content. |
| `EndDisabled` | `Disabled` block | Lowered host exit for disabled lexical content. |
| `BeginScroll` | `Scroll` block | Lowered host entry for scroll content. |
| `EndScroll` | `Scroll` block | Lowered host exit for scroll content. |
| `BeginPopup` | `Popup` block | Lowered host entry for popup content. |
| `EndPopup` | `Popup` block | Lowered host exit for popup content. |
| `BeginTableCell` | `TableCell` block | Lowered host entry for custom table-cell content. |
| `EndTableCell` | `TableCell` block | Lowered host exit for custom table-cell content. |

## Go Public Compatibility Exports

These names are still exported by `go/kryon/api.go`, but they mirror lowered
host support or role-specific compatibility rather than clean widget concepts.
Generated Go should continue to prefer canonical calls such as `kr.Button`,
`kr.Scroll` blocks, `kr.Popup` blocks, `kr.Menu`, and `kr.DragDrop` roles as
those surfaces become available.

| Export | Replacement concept | Removal note |
|---|---|---|
| `BeginButton` | `Button` block | Lowered host entry for composed button content. |
| `BeginCard` | `Card` block | Lowered host entry for composed card content. |
| `BeginDisabled` | `Disabled` block | Lowered host entry for disabled lexical content. |
| `EndDisabled` | `Disabled` block | Lowered host exit for disabled lexical content. |
| `BeginScroll` | `Scroll` block | Lowered host entry for scroll content. |
| `EndScroll` | `Scroll` block | Lowered host exit for scroll content. |
| `BeginPopup` | `Popup` block | Lowered host entry for popup content. |
| `EndPopup` | `Popup` block | Lowered host exit for popup content. |
| `BeginTableCell` | `TableCell` block | Lowered host entry for custom table-cell content. |
| `EndTableCell` | `TableCell` block | Lowered host exit for custom table-cell content. |
| `BeginCanvas` | `Canvas` block | Lowered host entry for canvas content. |
| `EndCanvas` | `Canvas` block | Lowered host exit for canvas content. |

## Web Runtime Compatibility Entries

These names are still recognized or exported by `web/kryon-runtime.js`, but
they are lowered host support rather than clean widget concepts. Generated web
code should prefer canonical `.kry` names and blocks.

No web runtime widget entries are accepted as public compatibility names.

## Core Drawing And Text

| Public name | Current decision | Notes |
|---|---|---|
| `Background` | `.kry canonical` | Viewport bounds policy is in `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `.kry canonical` | Single canonical signature should be `Text(TextProps)`. |
| `Paragraph` | `.kry canonical` | Metrics/default policy is in `.kry`; host keeps rich text parsing, reflow, icon shaping, and drawing. |
| `Box` | `.kry canonical` | Rectangle primitive with `Rectangle` bounds. |
| `Rect` | Removed | Old positional rectangle helper; use `Box`. |
| `Circle` | `.kry canonical` | Retained bounds policy is in `.kry`; host keeps circle drawing. Replaces raylib-style `DrawCircleV` in `.kry` surface. |
| `Ring` | `.kry canonical` | Retained bounds policy is in `.kry`; host keeps ring drawing. Replaces raylib-style `DrawRing` in `.kry` surface. |
| `Line` | `.kry canonical` | Endpoint and retained-bounds policy are in `.kry`; host keeps stroke drawing. |
| `Triangle` | `.kry canonical` | Retained bounds policy is in `.kry`; host keeps triangle drawing. |
| `Bevel` | Native canonical | Primitive drawing effect unless replaced by surface props. |
| `Icon` | `.kry canonical` | Bounds/size policy is in `.kry`; icon sheet/type surface is `IconType` with C `ICON_*` values and Go `kr.IconHome`-style constants. |
| `Image` | `.kry canonical` | Canonical image widget. |
| `Surface` | `.kry canonical` | Material/container paint helper; layer assembly policy is in `.kry`. |
| `Bullet` | `.kry canonical` | Bullet geometry and paint policy are in `.kry`; keep as a small primitive unless list item props absorb it. |

## Controls

| Public name | Current decision | Notes |
|---|---|---|
| `Card` | `.kry canonical` | Already has `.kry` module. |
| `BeginCard` | Native support | Lowered host entry for composed `.kry` `Card` content; not a separate widget concept. |
| `Button` | `.kry canonical` | Single public button surface. Menu, split-action, icon-only, arrow, info/help, loading, disclosure, tone, and emphasis behavior should live in `ButtonProps` or small `.kry` composition, not separate public widget names. |
| `BeginButton` | Native support | Lowered host entry for composed `.kry` `Button` content; not a separate widget concept. |
| `BeginDisabled` | Native support | Host scope for disabled child content. |
| `EndDisabled` | Native support | Host scope exit for disabled child content. |
| `Link` | `.kry canonical` | Canonical public name for URL/link activation; color/hover/disabled policy is in `.kry`, URL dispatch remains host support. |
| `TextField` | `.kry canonical` | Metrics and horizontal scroll policy are in `.kry`; editing, IME, selection, and rendering remain native host support. |
| `TextArea` | `.kry canonical` | Metrics and page-row policy are in `.kry`; editing, IME, selection, and rendering remain native host support. |
| `Dropdown` | `.kry canonical` | Already has `.kry` module. |
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
| `LabelTextField` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `TextField`. |
| `CheckboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Checkbox`. |
| `SpinboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Spinbox`. |
| `ButtonRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Row` with `Button` children. |
| `SectionLabel` | Removed | Removed from public headers; internal row helper only. Public code should use `Text`/`Heading` props or `.kry` composition. |
| `InfoRows` | Native support | Internal repeated label/value row helper; public forms should use `.kry` layout with `Row`/`Text`. |
| `Form` | Native support | Cursor/layout helper for legacy native row APIs; not a public widget concept. Prefer `.kry` layout blocks. |

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
| `Fieldset` | `.kry canonical` | Titled border group. |
| `PanedView` | `.kry canonical` | Split clamp and handle geometry are in `.kry`; host keeps drag/input ownership. |
| `Collapsible` | `.kry canonical` | Header metrics, geometry, and marker text are in `.kry`; host keeps input, focus, tree navigation, and drawing. |
| `Scroll` | `.kry canonical` | Lexical scroll-content block. Measurement and sizing policy are in `.kry`; host keeps wheel/drag/clipping and lowered scope ownership. |
| `TableCell` | `.kry canonical` | Lexical custom table-cell block; lowers to host cell scope. |
| `ScrollContainer` | Native support | Internal host helper only; public callers should use `Scroll` blocks. |
| `ScrollPage` | Native support | Internal host helper only; not a public widget concept. |
| `ScreenScaffold` | Native support | Internal app-shell helper only; compose pages from `.kry` layout. |
| `BeginScroll` | Native support | Lowered host entry for `.kry` `Scroll` blocks; not a separate public widget name. |
| `EndScroll` | Native support | Lowered host exit for `.kry` `Scroll` blocks; not a separate public widget name. |
| `BeginTableCell` | Native support | Lowered host entry for `.kry` `TableCell` blocks; not a separate public widget name. |
| `EndTableCell` | Native support | Lowered host exit for `.kry` `TableCell` blocks; not a separate public widget name. |

## Page And Web Surfaces

| Public name | Current decision | Notes |
|---|---|---|
| `Page` | `.kry canonical` | Top-level document surface for generated web/page output; lowers to layout scopes and page metadata host support. |
| `Section` | `.kry canonical` | Page section container; lowers to layout scopes. |
| `Heading` | `.kry canonical` | Semantic page heading backed by text policy. |
| `ParagraphText` | `.kry canonical` | Semantic page paragraph backed by text policy. |
| `Flow` | `.kry canonical` | Page flow layout; lowers to row/layout policy. |

## Collections And Editors

| Public name | Current decision | Notes |
|---|---|---|
| `ListBox` | `.kry canonical` | Layout/navigation policy is in `.kry`; multi-selection uses `selected`, `selected_count`, and `anchor` props. KSS styles multi-select mode with `ListBoxMulti` and `ListBoxMultiItem`, not a separate `MultiSelectList` widget. Host handles input sampling, scroll scope, and drawing. |
| `TreeView` | `.kry canonical` | Row/window geometry policy is in `.kry`; host handles input, scrollbars, selection mutation, expansion state, and drawing. |
| `TableView` | `.kry canonical` | Header/body/frozen-row/scroll/cell geometry is in `.kry`; host handles column ordering, input, selection mutation, resizing, clipboard, and drawing. |
| `CanvasGrid` | `.kry canonical` | Grid spacing, line counts, and line rectangles are in `.kry`; host handles drawing. |
| `Canvas` | `.kry canonical` | Transform, hit-test, and result policy are in `.kry`; host keeps clip/camera renderer scope. |
| `BeginCanvas` | Native support | Lowered host entry for `.kry` `Canvas` blocks; not a separate public widget name. |
| `EndCanvas` | Native support | Lowered host exit for `.kry` `Canvas` blocks; not a separate public widget name. |
| `DragDrop` | `.kry canonical` | Typed drag/drop interaction concept. Source and target roles belong in props or composition; decision policy is in `.kry`, host keeps payload storage, type comparison, and pointer ownership. |

## Navigation

| Public name | Current decision | Notes |
|---|---|---|
| `NavigationBar` | `.kry canonical` | Already has `.kry` module. |
| `Toolbar` | `.kry canonical` | Metrics and geometry policy are in `.kry`; host handles input, drawing, and child `Button`/`Dropdown` calls. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props. |
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
| `Toast` | `.kry canonical` | Public toast feedback surface. Duration and layout policy are in `.kry`; host keeps message storage, timing source, truncation, and drawing. |
| `Focus` | Native support | Focus ring geometry is in `.kry`; focus state remains host support. |
| `Guide` | `.kry canonical` | Guided overlay flow. The clean public API is one `Guide(GuideProps)` surface with step data in props; `GuideStep` is data, not a widget. Current C rendering is host support around `runtime/guide.kry` policy. |
| `GuideStep` | Props/data only | One anchored instruction inside `GuideProps`; not a standalone widget. |
| `GuidePager` | Internal support | Not a public widget. Footer layout/page transition policy is `.kry`; the C helper lives under `src/ui` and is not exported by public headers. |

## Game2D Nodes

Game2D has its own node family. These are canonical for the Game2D domain, but
they are still native scene nodes today and should stay separate from general
UI widgets.

| Public name | Current decision | Notes |
|---|---|---|
| `Scene` | Game2D native scene | Root game scene; add `.kry` declaration support later. |
| `Node2D` | Game2D native scene | Base 2D node; add `.kry` declaration support later. |
| `Camera2D` | Game2D native scene | Camera node; add `.kry` declaration support later. |
| `Sprite2D` | Game2D native scene | Sprite node; add `.kry` declaration support later. |
| `AnimatedSprite2D` | Game2D native scene | Animated sprite node; add `.kry` declaration support later. |
| `TileMap` | Game2D native scene | Tile map node; add `.kry` declaration support later. |
| `CollisionShape2D` | Game2D native scene | Collision shape node; add `.kry` declaration support later. |
| `Area2D` | Game2D native scene | Trigger/area node; add `.kry` declaration support later. |
| `Body2D` | Game2D native scene | Physics/body node; add `.kry` declaration support later. |
| `Timer` | Game2D native scene | Timer node; add `.kry` declaration support later. |
| `AudioSource` | Game2D native scene | Audio playback node; add `.kry` declaration support later. |
| `Light2D` | Game2D native scene | Light node; add `.kry` declaration support later. |

## Escape Hatches

| Public name | Current decision | Notes |
|---|---|---|
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
| `WIDGET_RECT` | `Box` | `.kry-backed`; public code uses `Box` |
| `WIDGET_CIRCLE` | `Circle` | `.kry-backed`; public code uses `Circle` |
| `WIDGET_RING` | `Ring` | `.kry-backed`; public code uses `Ring` |
| `WIDGET_LINE` | `Line` | Partly `.kry-backed` |
| `WIDGET_TRIANGLE` | `Triangle` | `.kry-backed`; public code uses `Triangle` |
| `WIDGET_BUTTON` | `Button` | `.kry canonical` |
| `WIDGET_TEXT_FIELD` | `TextField` | `.kry canonical`; editing policy still migrating |
| `WIDGET_TEXT_AREA` | `TextArea` | `.kry canonical`; editing policy still migrating |
| `WIDGET_DROPDOWN` | `Dropdown` | `.kry canonical` |
| `WIDGET_SLIDER` | `Slider` | `.kry canonical` |
| `WIDGET_TOGGLE` | `Toggle` | `.kry canonical` |
| `WIDGET_CHECKBOX` | `Checkbox` | `.kry canonical` |
| `WIDGET_PARAGRAPH` | `Paragraph` | `.kry canonical`; rich text metrics/default policy is `.kry-backed` |
| `WIDGET_READONLY_TEXT_BOX` | Removed | Old retained node/helper deleted; use `TextArea` with read-only props. |
| `WIDGET_NAVIGATION_BAR` | `NavigationBar` | `.kry canonical` |
| `WIDGET_TAB_BAR` | `TabBar` | `.kry canonical` |
| `WIDGET_PARAGRAPH_MODAL` | Removed | Old retained measuring helper deleted; compose `Modal` with `Paragraph`/`Text`. |
| `WIDGET_TITLE_BAR` | `TitleBar` | `.kry canonical` |
| `WIDGET_GROUP` | `Group` | `.kry canonical`; bounds/content policy is `.kry-backed` |
| `WIDGET_COLUMN` | `Column` | `.kry canonical`; placement policy is `.kry-backed` |
| `WIDGET_ROW` | `Row` | `.kry canonical`; placement policy is `.kry-backed` |
| `WIDGET_STACK` | `Stack` | `.kry canonical`; placement policy is `.kry-backed` |
| `WIDGET_GRID` | `Grid` | `.kry canonical`; metrics and cursor placement policy are `.kry-backed` |
| `WIDGET_IMAGE` | `Image` | `.kry canonical` |
| `WIDGET_CUSTOM` | `Custom` | Native support escape hatch |
| `WIDGET_DRAG` | `Drag` | `.kry canonical` |
| `WIDGET_TEXT_INPUT_PAINT` | Removed | Internal text input paint snapshots lower through `WIDGET_CUSTOM` with a private runtime flag. |
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
| text input paint snapshot | Internal `TextInputPaint` host snapshot. |
| `UIAccessibilitySink` | `AccessibilitySink` |
| `UIInspect*`, `BeginUIInspect*`, `PushUIInspect*`, `IsUIInspectActive` | `Inspect*`, `BeginInspect*`, `PushInspect*`, `IsInspectActive` |
| `UIDPIState`, `UI_DPI_BASE_*`, `InitUIDPI`, `GetUIDPI*` | `DPIState`, `DPI_BASE_*`, `InitDPI`, `GetDPI*` |
| `UIFrameState`, `InitUI`, `BeginUIFrame`, `EndUIFrame` | `FrameState`, `InitInterface`, `BeginInterfaceFrame`, `EndInterfaceFrame` |
| `SetUI*`, `GetUI*`, `IsUI*`, `ClearUI*`, `PushUI*`, `PopUI*` focus/input helpers | `Set*`, `Get*`, `Is*`, `Clear*`, `Push*`, `Pop*` focus/input helpers |
| `UIFont*`, `UI_FONT_*`, `RegisterUISmallFont` | `TextFont*`, `TEXT_FONT_*`, `RegisterSmallTextFont` |
| `UIClipboard*`, `UI_CLIPBOARD_*`, `UIPrimarySelection*` | `Clipboard*`, `CLIPBOARD_*`, `PrimarySelection*` |
| old profile image helpers | `ProfileImage*`, `SyncProfileIcon`, `SYNC_PROFILE_ICON_*` |
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
2. Keep lowered host scopes out of public `.kry` documentation:
   immediate-mode `Begin*`/`End*` wrappers are native support, not widget names.
   Tutorial image helpers remain internal and route through canonical `Image`
   policy.
3. Keep `docs/IMGUI_WIDGET_COVERAGE.md` as the coverage audit. Use this file
   as the naming and migration review surface.
