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
| `runtime/canvas_props.kry` | Canvas props and result | `.kry canonical` |
| `runtime/canvas_grid.kry` | CanvasGrid line policy | `.kry canonical` |
| `runtime/card.kry` | Card composition | `.kry canonical` |
| `runtime/card_props.kry` | Card props | `.kry canonical` |
| `runtime/checkbox.kry` | Checkbox paint, row/text layout, and flag policy | `.kry canonical` |
| `runtime/checkbox_props.kry` | Checkbox props | `.kry canonical` |
| `runtime/collapsible.kry` | Collapsible metrics/header geometry policy | `.kry canonical` |
| `runtime/collapsible_props.kry` | Collapsible props | `.kry canonical` |
| `runtime/color_picker.kry` | ColorPicker channel, swatch, and color policy | `.kry canonical` |
| `runtime/color_picker_props.kry` | ColorPicker props | `.kry canonical` |
| `runtime/control_props.kry` | Shared control props | `.kry canonical` |
| `runtime/drawing_props.kry` | Shared drawing props and paragraph spec data | `.kry canonical` |
| `runtime/dropdown.kry` | Dropdown composition, popup, row, scrollbar, navigation, and indicator geometry policy | `.kry canonical` |
| `runtime/dropdown_props.kry` | Dropdown rich item data and props | `.kry canonical` |
| `runtime/drag_drop.kry` | DragDrop decision policy | `.kry canonical` |
| `runtime/drag_drop_props.kry` | DragDrop props and role enum | `.kry canonical` |
| `runtime/drag.kry` | Drag component layout, text paint geometry, and value/keyboard policy | `.kry canonical` |
| `runtime/drag_props.kry` | Drag props | `.kry canonical` |
| `runtime/grid.kry` | Grid composition | `.kry canonical` |
| `runtime/grid_props.kry` | Grid props | `.kry canonical` |
| `runtime/group.kry` | Group bounds/content policy | `.kry canonical` |
| `runtime/guide.kry` | Guide overlay layout, arrow geometry, and step policy | `.kry canonical` |
| `runtime/guide_pager.kry` | Internal pager footer layout and page transition policy | Native support |
| `runtime/icon.kry` | Icon bounds/size policy | `.kry canonical` |
| `runtime/image.kry` | Image fit and placeholder layout policy | `.kry canonical` |
| `runtime/image_props.kry` | Image props | `.kry canonical` |
| `runtime/input.kry` | Input component layout and value/step policy | `.kry canonical` |
| `runtime/input_props.kry` | Input props and shared numeric value kind | `.kry canonical` |
| `runtime/instance.kry` | Generated widget instance identity helpers | Native support |
| `runtime/fieldset.kry` | Fieldset layout/paint policy | `.kry canonical` |
| `runtime/fieldset_props.kry` | Fieldset props | `.kry canonical` |
| `runtime/focus.kry` | Focus ring and debug overlay geometry policy | Native support |
| `runtime/layout.kry` | Column/Row/Stack content and child placement policy | `.kry canonical` |
| `runtime/layout_props.kry` | Column/Row/Flow layout props | `.kry canonical` |
| `runtime/link.kry` | Link state/color policy | `.kry canonical` |
| `runtime/link_props.kry` | Link props | `.kry canonical` |
| `runtime/list_box.kry` | ListBox layout/navigation and row paint geometry policy | `.kry canonical` |
| `runtime/list_box_props.kry` | ListBox props | `.kry canonical` |
| `runtime/material.kry` | Material layer assembly | `.kry canonical` |
| `runtime/menu.kry` | Menu metrics and geometry policy | `.kry canonical` |
| `runtime/menu_props.kry` | Menu item/group/result data and props | `.kry canonical` |
| `runtime/list_box_multi.kry` | ListBox multi-selection row/navigation/selection policy | `.kry canonical` |
| `runtime/navigation_bar.kry` | Navigation bar composition | `.kry canonical` |
| `runtime/navigation_bar_props.kry` | NavigationBar props and result | `.kry canonical` |
| `runtime/node2d_props.kry` | Game2D node props and enums | `.kry canonical` |
| `runtime/paint.kry` | Paint/drawing helpers | Native support |
| `runtime/paned_view.kry` | PanedView split/handle geometry policy | `.kry canonical` |
| `runtime/paned_view_props.kry` | PanedView props | `.kry canonical` |
| `runtime/page_props.kry` | Page/Section/Heading/Paragraph props | `.kry canonical` |
| `runtime/paragraph.kry` | Paragraph metrics/default policy | `.kry canonical` |
| `runtime/plot.kry` | Plot geometry and text policy | `.kry canonical` |
| `runtime/plot_props.kry` | Plot props | `.kry canonical` |
| `runtime/popup_policy.kry` | Popup mode/input policy | `.kry canonical` |
| `runtime/popup_props.kry` | Popup props | `.kry canonical` |
| `runtime/primitive.kry` | Background/Box/Line/Circle/Ring/Triangle primitive geometry policy | `.kry canonical` |
| `runtime/progress.kry` | Progress layout policy | `.kry canonical` |
| `runtime/progress_props.kry` | Progress props | `.kry canonical` |
| `runtime/radio.kry` | Radio paint/layout policy | `.kry canonical` |
| `runtime/radio_props.kry` | Radio props | `.kry canonical` |
| `runtime/reorder.kry` | Reorder metrics, handle/placeholder paint geometry, and target-index policy | Native support |
| `runtime/reorder_props.kry` | Reorder support state, data, and result records | Native support |
| `runtime/router_props.kry` | Router routes, state, props, and result | `.kry canonical` |
| `runtime/segmented_control.kry` | SegmentedControl layout policy | `.kry canonical` |
| `runtime/segmented_control_props.kry` | SegmentedControl props and result | `.kry canonical` |
| `runtime/selectable.kry` | Selectable paint/layout policy | `.kry canonical` |
| `runtime/selectable_props.kry` | Selectable props | `.kry canonical` |
| `runtime/separator.kry` | Separator/Bullet layout and paint policy | `.kry canonical` |
| `runtime/separator_props.kry` | Separator props | `.kry canonical` |
| `runtime/slider.kry` | Slider composition, component/editor/hit layout, text paint geometry, and value/keyboard policy | `.kry canonical` |
| `runtime/slider_props.kry` | Slider props | `.kry canonical` |
| `runtime/spinbox.kry` | Spinbox layout/value policy | `.kry canonical` |
| `runtime/spinbox_props.kry` | Spinbox props | `.kry canonical` |
| `runtime/scroll.kry` | Scroll measurement/sizing policy | `.kry canonical` |
| `runtime/scroll_props.kry` | Scroll props | `.kry canonical` |
| `runtime/style.kry` | Style helpers | `.kry canonical` |
| `runtime/style_picker_props.kry` | StylePicker props | `.kry canonical` |
| `runtime/style_sheet.kry` | Style sheet evaluation helpers | `.kry canonical` |
| `runtime/surface.kry` | Surface/container helpers | `.kry canonical` |
| `runtime/swipe.kry` | Swipe direction/default/progress policy | Native support |
| `runtime/swipe_props.kry` | Swipe support state, data, and result records | Native support |
| `runtime/tab_bar.kry` | TabBar sizing, scroll, and reorder marker policy | `.kry canonical` |
| `runtime/tab_bar_props.kry` | TabBar props | `.kry canonical` |
| `runtime/text.kry` | Text composition | `.kry canonical` |
| `runtime/text_props.kry` | Text props | `.kry canonical` |
| `runtime/text_input.kry` | TextField/TextArea metrics, scroll, paint geometry, buffer-limit, navigation, selection state, and edit-intent policy | `.kry canonical` |
| `runtime/text_input_props.kry` | TextField/TextArea props and text input style enums | `.kry canonical` |
| `runtime/theme.kry` | Theme data/helpers | `.kry canonical` |
| `runtime/title_bar.kry` | TitleBar layout and paint geometry policy | `.kry canonical` |
| `runtime/title_bar_props.kry` | TitleBar props | `.kry canonical` |
| `runtime/toggle.kry` | Toggle composition | `.kry canonical` |
| `runtime/toggle_props.kry` | Toggle props | `.kry canonical` |
| `runtime/toolbar.kry` | Toolbar, bottom icon row, and icon slider popup metrics/geometry policy | `.kry canonical` |
| `runtime/toolbar_props.kry` | Toolbar and bottom icon row props/results | `.kry canonical` |
| `runtime/toast.kry` | Toast duration and layout policy | `.kry canonical` |
| `runtime/toast_props.kry` | Toast props | `.kry canonical` |
| `runtime/transition_fade.kry` | Transition fade alpha/easing policy | Native support |
| `runtime/modal.kry` | Modal layout, frame geometry, and action policy | `.kry canonical` |
| `runtime/modal_props.kry` | Modal props and action props | `.kry canonical` |
| `runtime/tree_view.kry` | TreeView row/window and paint geometry policy | `.kry canonical` |
| `runtime/tree_view_props.kry` | TreeView props | `.kry canonical` |
| `runtime/table_view.kry` | TableView layout, scroll, scrollbar, and cell geometry policy | `.kry canonical` |
| `runtime/table_view_props.kry` | TableView row and props | `.kry canonical` |

## Current Implementation Audit

This is the current migration truth, not the desired final state. A name in the
registry is only considered `.kry`-backed when its reusable behavior, props, or
layout policy lives in `runtime/*.kry` and native code only adapts host input,
text measurement, painting, storage, or platform services.

| Group | `.kry`-backed today | Still native-only or compatibility |
|---|---|---|
| Text and drawing | `Text` style resolution, `Paragraph` metrics/default policy, `ParagraphSpec` generated data, `Background`/`Box`/`Line`/`Circle`/`Ring`/`Triangle` geometry policy, `Bevel` line geometry, `Icon` bounds/size policy, `Image` canonical props/name and placeholder layout, clean drawing primitive names (`Box`, `Circle`, `Ring`, `Triangle`) | icon sheet/drawing host support, paragraph reflow/rendering |
| Actions | `Button`, `Card`, `Link`, `Button` menu/split/arrow/info options | helper button variants belong in `ButtonProps` or composition; invisible hit testing is host support |
| Inputs | `Checkbox` paint/row/text/flag policy, `Dropdown` popup/row/scrollbar/navigation/indicator policy, `DropdownOption`, `Drag` component layout/text paint/value policy, `Input` component/step-button layout and value policy, `Progress`, `Radio`, `SegmentedControl`, `Selectable`, `Slider` component/editor/hit layout, text paint geometry, and value/keyboard policy, `Spinbox`, `TextField`/`TextArea` metrics/paint geometry/buffer-limit/navigation/edit intent/selection state policy, `Toggle`, `Button` swatch props, `ColorPicker` layout/swatch/color policy | text composition/buffer mutation host support |
| Layout | `Column`/`Row`/`Stack` content and child placement policy, `Group` bounds/content policy, `Screen` viewport fallback bounds policy, `Grid`, `Fieldset` layout policy, `PanedView` split geometry, `Collapsible` header geometry, `Separator`, `Scroll` measurement/sizing policy, shared `Surface`/`Style`/`Material` policy, `Reorder` metrics/handle geometry/placeholder paint geometry/target-index policy, `ReorderState`/`ReorderItem`/`ReorderList`/`ReorderListResult` generated support records | scroll/list/table begin-end wrappers; reorder pointer ownership and gesture lifecycle remain host support |
| Collections | `Canvas` transform/hit-test policy, `CanvasGrid`, drag/drop decision policy, `ListBox` layout/navigation/row paint geometry/multi-selection policy, `Plot` geometry policy, `TreeView` row/window/paint geometry policy, `TableView` layout/scroll/scrollbar/cell geometry policy | drag/drop payload storage |
| Navigation | `NavigationBar` paint/config layout policy, `TabBar` sizing/scroll/reorder marker policy, `Toolbar`, bottom icon row, and icon slider popup metrics/geometry policy, `TitleBar` layout/paint geometry policy, `Menu` geometry policy, `MenuItem`/`MenuGroup`/`MenuResult` data | retained menu open/focus/input state, router/link helpers |
| Overlays | `Popup` mode/input policy, `Focus` ring geometry policy, `Guide` overlay layout/arrow/step policy, swipe direction/default/progress policy, `SwipeGesture`/`SwipeSpec`/`SwipeResult` generated pager support records, `Modal` layout/frame/action policy, `Toast` duration/layout policy, transition fade alpha/easing policy, `StylePicker` public props | theme picker rendering/input host support; swipe pointer ownership and gesture lifecycle remain host support |
| Game2D | `Camera2D`, `Sprite2D`, `AnimatedSprite2D`, `TileMap`, `CollisionShape2D`, `Area2D`, `Body2D`, `AnimationPlayer`, `AudioSource`, and `Light2D` public props/enums | Scene ownership, lifecycle, physics/audio handles, rendering, and `Scene`/`Node2D` runtime behavior remain native Game2D support. |

The remaining migration target is the native support around text editing and
content wrappers: `TextField` and `TextArea` own metrics/paint geometry/buffer-limit/navigation/edit-intent
and selection range/movement/collapse/select-all policy in `.kry`, but buffer mutation, IME, selection
painting, and rich text reflow/rendering remain host work.

## Registry Surface Audit

This table is the authoritative shared list of public node names from
`src/ui/ui_node_registry.c`. Keep one row per registry name so rename feedback
has a single place to land.

| Public name | Registry group | Detail | Runtime `.kry` source | State | Migration note |
|---|---|---|---|---|---|
| `Background` | `UI/Display` | Fill | `runtime/primitive.kry` | `.kry-backed` | Viewport bounds and app fallback policy are `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `UI/Display` | Label | `runtime/text.kry` | `.kry-backed` | Keep one `Text(TextProps)` surface; retained tree typography uses resolved KSS font sizes directly. |
| `Paragraph` | `UI/Display` | Rich text | `runtime/paragraph.kry`, `runtime/drawing_props.kry` | Partly `.kry-backed` | Metrics/default policy and `ParagraphSpec` data are `.kry`; text parsing, reflow, icon shaping, and drawing remain host support. |
| `Box` | `UI/Display` | Shape | `runtime/primitive.kry` | `.kry-backed` | Rectangle bounds policy is `.kry`; host keeps fill/border drawing. |
| `Line` | `UI/Display` | Stroke | `runtime/primitive.kry` | `.kry-backed` | Endpoint and retained-bounds policy is `.kry`; host keeps stroke drawing. |
| `Bevel` | `UI/Display` | Relief | `runtime/bevel.kry` | `.kry-backed` | Line geometry is `.kry`; still review whether it should fold into `Surface`/material props. |
| `Icon` | `UI/Display` | Icon | `runtime/icon.kry` | Partly `.kry-backed` | Bounds/size policy is `.kry`; icon sheet/type lookup and drawing remain host support. |
| `Image` | `UI/Display` | Image | `runtime/image.kry` | Partly `.kry-backed` | Fit and placeholder layout policy are `.kry`; placeholder typography uses resolved KSS font sizes directly; cache/loading/drawing remain host support. |
| `Card` | `UI/Input` | Surface action | `runtime/card.kry`, `runtime/card_props.kry` | `.kry-backed` | Card composition and props live in `.kry`. |
| `Button` | `UI/Input` | Action | `runtime/button.kry`, `runtime/button_props.kry` | `.kry-backed` | Single button surface; menu/split/info/icon variants are props/composition; retained and immediate typography defaults are KSS-owned. |
| `Link` | `UI/Input` | Link | `runtime/link.kry` | Partly `.kry-backed` | State/color policy is `.kry`; URL dispatch remains host support. |
| `TextField` | `UI/Input` | Input | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics, scroll, paint geometry, buffer-limit, navigation, edit intent, and selection range/movement/collapse/select-all policy are `.kry`; buffer mutation, IME, selection ownership, and paint still native. |
| `Dropdown` | `UI/Input` | Selection | `runtime/dropdown.kry`, `runtime/dropdown_props.kry` | `.kry-backed` | Selection-only control; popup placement, row/window, scrollbar, scrolling, navigation, indicator geometry, and rich option data are generated from `.kry`. |
| `Slider` | `UI/Input` | Value | `runtime/slider.kry` | `.kry-backed` | Value type, orientation, angle/unit, component/editor/hit layout, and text paint geometry are props/policy; label/value typography is KSS-owned. |
| `Toggle` | `UI/Input` | On/off | `runtime/toggle.kry` | `.kry-backed` | Host handles input and drawing; paint/layout policy is `.kry`. |
| `Checkbox` | `UI/Input` | Boolean | `runtime/checkbox.kry` | `.kry-backed` | Paint, row/text layout, and flag policy are `.kry`; box, mark, and label roles are KSS-owned. |
| `Radio` | `UI/Input` | Choice | `runtime/radio.kry` | `.kry-backed` | Paint, layout, and marker text policy are `.kry`; host keeps group input. |
| `Progress` | `UI/Input` | Progress | `runtime/progress.kry` | `.kry-backed` | One public progress concept. |
| `Spinbox` | `UI/Input` | Number | `runtime/spinbox.kry` | `.kry-backed` | Layout/step policy is `.kry`; host keeps text/button input. |
| `ColorPicker` | `UI/Input` | Color | `runtime/color_picker.kry` | `.kry-backed` | Channel layout and conversion are `.kry`. |
| `SegmentedControl` | `UI/Input` | Segments | `runtime/segmented_control.kry` | `.kry-backed` | Layout, wrapping, and segment sizing policy are `.kry`; host keeps label measurement, input sampling, and button drawing. |
| `Group` | `UI/Layout` | Container | `runtime/group.kry` | `.kry-backed` | Canonical non-layout grouping scope; bounds/content policy is `.kry`, host keeps retained tree scope ownership. |
| `Separator` | `UI/Layout` | Divider | `runtime/separator.kry` | `.kry-backed` | Line, label, and bullet policy are `.kry`. |
| `Fieldset` | `UI/Layout` | Frame | `runtime/fieldset.kry` | `.kry-backed` | Titled group and border policy are `.kry`. |
| `PanedView` | `UI/Layout` | Split panes | `runtime/paned_view.kry` | Partly `.kry-backed` | Split clamp and handle geometry are `.kry`; host keeps drag/input ownership. |
| `Collapsible` | `UI/Layout` | Section | `runtime/collapsible.kry` | Partly `.kry-backed` | Header metrics, geometry, marker text, and typography defaults are `.kry`/KSS-owned; host keeps input, focus, tree navigation, and drawing. |
| `ListBox` | `UI/Collections` | List | `runtime/list_box.kry` | `.kry-backed` | Layout/navigation and row paint geometry policy is `.kry`; host keeps input/scroll sampling. |
| `TreeView` | `UI/Collections` | Tree | `runtime/tree_view.kry` | Partly `.kry-backed` | Row, indent, scroll-window, text bounds, and paint geometry policy are `.kry`; item typography defaults are KSS-owned; host keeps input, selection mutation, expansion state, and drawing. |
| `TableView` | `UI/Collections` | Table | `runtime/table_view.kry` | Partly `.kry-backed` | Header/body/frozen-row/scroll/scrollbar/cell geometry policy is `.kry`; host keeps column ordering, input, selection mutation, resizing, clipboard, and drawing. |
| `TextArea` | `UI/Collections` | Text area | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics, page-navigation rows, paint geometry, buffer-limit, navigation, edit intent, and selection range/movement/collapse/select-all policy are `.kry`; buffer mutation, IME, selection ownership, and paint still native. |
| `CanvasGrid` | `UI/Collections` | Grid | `runtime/canvas_grid.kry` | `.kry-backed` | Grid spacing and line geometry are `.kry`; host draws. |
| `Menu` | `UI/Navigation` | Menu | `runtime/menu.kry`, `runtime/menu_props.kry` | `.kry canonical` | Command menu surface; item/group/result data and bar, popup, and context behavior props are generated from `.kry`. |
| `NavigationBar` | `UI/Navigation` | Tabs | `runtime/navigation_bar.kry` | `.kry-backed` | Paint, sizing, and configuration modal layout policy are `.kry`. |
| `Toolbar` | `UI/Navigation` | Tools | `runtime/toolbar.kry` | `.kry-backed` | Metrics/geometry are `.kry`; host dispatches child actions. |
| `TabBar` | `UI/Navigation` | Tabs | `runtime/tab_bar.kry` | `.kry-backed` | Sizing/scroll/reorder marker policy is `.kry`; host keeps input sampling. |
| `TitleBar` | `UI/Navigation` | Title | `runtime/title_bar.kry` | Partly `.kry-backed` | Layout, paint geometry, and title font-fit policy are `.kry`; host keeps dropdown dispatch, text measurement, and leading-action input/rendering. |
| `Focus` | `UI/Overlays` | Focus | `runtime/focus.kry` | Partly `.kry-backed` | Ring geometry policy is `.kry`; host keeps focus state, registration, and drawing. |
| `Modal` | `UI/Overlays` | Dialog | `runtime/modal.kry` | Partly `.kry-backed` | Layout, frame geometry, and action sizing policy are `.kry`; host keeps modal input layer, text editing, and drawing. |
| `Scene` | `Game2D/Core` | Scene root | missing | Game2D native scene | Separate Game2D surface; introduce `.kry` scene declarations later. |
| `Node2D` | `Game2D/Core` | Transform | missing | Game2D native scene | Separate Game2D surface. |
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
| `Abbr` | `.kry canonical` | Web-native abbreviation alias; renders as semantic DOM in the web runtime. |
| `Abbreviation` | `.kry canonical` | Long-form alias for `Abbr` in web-native content. |
| `Address` | `.kry canonical` | Web-native contact/address content element. |
| `Article` | `.kry canonical` | Web-native article content region. |
| `Aside` | `.kry canonical` | Web-native aside/related-content region. |
| `Box` | `.kry canonical` | Canonical rectangle primitive with `Rectangle` bounds. |
| `Line` | `.kry canonical` | Endpoint and retained-bounds policy are in `.kry`; host keeps stroke drawing. |
| `Bevel` | Native canonical | Drawing effect unless material/surface props absorb it. |
| `Icon` | `.kry canonical` | Icon bounds/size policy is in `.kry`. |
| `Image` | `.kry canonical` | Canonical image widget; replaces old picture naming. |
| `Button` | `.kry canonical` | One button surface; variants belong in props/composition. |
| `Card` | `.kry canonical` | Surface action/card composition. |
| `Selectable` | `.kry canonical` | Selectable row/action surface. |
| `Audio` | `.kry canonical` | Web-native audio media element. |
| `BlockQuote` | `.kry canonical` | Web-native block quotation element. |
| `Bold` | `.kry canonical` | Web-native bold phrasing element. |
| `Cite` | `.kry canonical` | Web-native citation phrasing element. |
| `Code` | `.kry canonical` | Web-native inline code element. |
| `CodeBlock` | `.kry canonical` | Web-native preformatted code block. |
| `Col` | `.kry canonical` | Web-native table column element. |
| `ColGroup` | `.kry canonical` | Web-native table column group element. |
| `Data` | `.kry canonical` | Web-native machine-readable data element. |
| `Del` | `.kry canonical` | Web-native deleted text element. |
| `Deleted` | `.kry canonical` | Long-form alias for `Del`. |
| `DescriptionDetails` | `.kry canonical` | Web-native description details element. |
| `DescriptionList` | `.kry canonical` | Web-native description list element. |
| `DescriptionTerm` | `.kry canonical` | Web-native description term element. |
| `Details` | `.kry canonical` | Web-native disclosure details element. |
| `Dialog` | `.kry canonical` | Web-native dialog element. |
| `Em` | `.kry canonical` | Web-native emphasized text element. |
| `Embed` | `.kry canonical` | Web-native embedded external content element. |
| `Emphasis` | `.kry canonical` | Long-form alias for `Em`. |
| `Figcaption` | `.kry canonical` | Web-native figure caption element. |
| `Figure` | `.kry canonical` | Web-native figure element. |
| `Footer` | `.kry canonical` | Web-native footer landmark/content element. |
| `Form` | `.kry canonical` | Web-native form element. |
| `Header` | `.kry canonical` | Web-native header landmark/content element. |
| `IFrame` | `.kry canonical` | Web-native iframe element. |
| `Iframe` | `.kry canonical` | Alternate casing for `IFrame`. |
| `Ins` | `.kry canonical` | Web-native inserted text element. |
| `Inserted` | `.kry canonical` | Long-form alias for `Ins`. |
| `Italic` | `.kry canonical` | Web-native italic phrasing element. |
| `Kbd` | `.kry canonical` | Web-native keyboard input phrasing element. |
| `Keyboard` | `.kry canonical` | Long-form alias for `Kbd`. |
| `Label` | `.kry canonical` | Web-native form label element. |
| `List` | `.kry canonical` | Web-native unordered list alias. |
| `ListItem` | `.kry canonical` | Web-native list item element. |
| `Main` | `.kry canonical` | Web-native main landmark element. |
| `Mark` | `.kry canonical` | Web-native marked/highlighted text element. |
| `Meter` | `.kry canonical` | Web-native scalar meter element. |
| `Nav` | `.kry canonical` | Web-native navigation landmark element. |
| `Navigation` | `.kry canonical` | Long-form alias for `Nav`. |
| `OrderedList` | `.kry canonical` | Web-native ordered list element. |
| `Option` | `.kry canonical` | Web-native select option element. |
| `Output` | `.kry canonical` | Web-native calculation output element. |
| `Pre` | `.kry canonical` | Web-native preformatted text element. |
| `Quote` | `.kry canonical` | Web-native inline quote element. |
| `Samp` | `.kry canonical` | Web-native sample output phrasing element. |
| `Sample` | `.kry canonical` | Long-form alias for `Samp`. |
| `Select` | `.kry canonical` | Web-native select element; app selection control remains `Dropdown`. |
| `Small` | `.kry canonical` | Web-native side-comment/small text element. |
| `Source` | `.kry canonical` | Web-native media source element. |
| `Strong` | `.kry canonical` | Web-native strong-importance phrasing element. |
| `Sub` | `.kry canonical` | Web-native subscript element. |
| `Subscript` | `.kry canonical` | Long-form alias for `Sub`. |
| `Summary` | `.kry canonical` | Web-native summary element for `Details`. |
| `Sup` | `.kry canonical` | Web-native superscript element. |
| `Superscript` | `.kry canonical` | Long-form alias for `Sup`. |
| `Table` | `.kry canonical` | Web-native table element. |
| `TableBody` | `.kry canonical` | Web-native table body element. |
| `TableCaption` | `.kry canonical` | Web-native table caption element. |
| `TableColumn` | `.kry canonical` | Web-native table column alias for `Col`. |
| `TableColumnGroup` | `.kry canonical` | Web-native table column group alias for `ColGroup`. |
| `TableFoot` | `.kry canonical` | Web-native table footer element. |
| `TableHead` | `.kry canonical` | Web-native table head element. |
| `TableRow` | `.kry canonical` | Web-native table row element. |
| `Tbody` | `.kry canonical` | Web-native short alias for `TableBody`. |
| `Tfoot` | `.kry canonical` | Web-native short alias for `TableFoot`. |
| `Thead` | `.kry canonical` | Web-native short alias for `TableHead`. |
| `Time` | `.kry canonical` | Web-native time element. |
| `Tr` | `.kry canonical` | Web-native short alias for `TableRow`. |
| `Track` | `.kry canonical` | Web-native media text track element. |
| `UnorderedList` | `.kry canonical` | Web-native unordered list element. |
| `Var` | `.kry canonical` | Web-native variable phrasing element. |
| `Variable` | `.kry canonical` | Long-form alias for `Var`. |
| `Video` | `.kry canonical` | Web-native video media element. |
| `Bullet` | `.kry canonical` | Small list/text marker primitive. |
| `Separator` | `.kry canonical` | Divider primitive. |
| `Link` | `.kry canonical` | Canonical link activation name. |
| `TextField` | `.kry canonical` | Metrics, KSS typography defaults, scroll, paint geometry, buffer-limit, navigation, selection state, and edit intent policy in `.kry`; buffer mutation and IME host support remain. |
| `TextArea` | `.kry canonical` | Metrics, KSS typography defaults, page-navigation, paint geometry, buffer-limit, navigation, selection state, and edit intent policy in `.kry`; buffer mutation and IME host support remain. |
| `Dropdown` | `.kry canonical` | Selection control only; `DropdownOption` is generated data for rich options, not a separate widget. |
| `SegmentedControl` | `.kry canonical` | Segmented choice control; layout/wrapping policy is in `.kry`, generated Go uses `kr.SegmentedControl`. |
| `Slider` | `.kry canonical` | Type/orientation/angle variants, component/editor/hit layout, and text paint geometry are props/policy; label/value typography is KSS-owned. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props. `MenuItem`, `MenuGroup`, and `MenuResult` are generated data/result records, not separate widgets. KSS uses `Menu`, `MenuItem`, and `MenuSeparator`; item typography participates in popup sizing; no `MenuBar` selector. |
| `Toggle` | `.kry canonical` | Boolean switch. |
| `Checkbox` | `.kry canonical` | Boolean checkbox. |
| `Radio` | `.kry canonical` | Choice control. |
| `Progress` | `.kry canonical` | One progress concept. |
| `Plot` | `.kry canonical` | Public props live in `runtime/plot_props.kry`; plot geometry/text policy lives in `.kry`. |
| `Drag` | `.kry canonical` | Numeric drag value control; value type/count, component layout, and text paint geometry are props/policy. |
| `Input` | `.kry canonical` | Numeric input control; value type/count and component/step-button layout are props/policy. |
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
| `End` | Native support | Lowered/parser block close marker, not a widget. |
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
| `TreeView` | `.kry canonical` | Tree rows/window and paint geometry policy in `.kry`; host keeps state/input. |
| `TableView` | `.kry canonical` | Table layout, scroll, scrollbar, and cell geometry policy in `.kry`; host keeps state/input. |
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

## Native Public Compatibility Exports

No lowered widget block scopes are exported by `include/ui_tree.h`. Parser
lowering and native tests use internal declarations for these host scopes until
the canonical `.kry` block surface owns the remaining generated backends.

| Export | Replacement concept | Removal note |
|---|---|---|

## Go Public Compatibility Exports

No lowered widget block scopes are exported as package-level functions by
`go/kryon/api.go`. Generated Go routes lowered lexical scopes through its
private runtime value; public Go code uses canonical calls such as `kr.Button`,
`kr.Slider`, `kr.Menu`, and `.kry` blocks for `Scroll`, `Popup`, `Disabled`,
`TableCell`, and `Canvas`.

| Export | Replacement concept | Removal note |
|---|---|---|

## Web Runtime Compatibility Entries

These names are still recognized or exported by `web/kryon-runtime.js`, but
they are lowered host support rather than clean widget concepts. Generated web
code should prefer canonical `.kry` names and blocks.

No web runtime widget entries are accepted as public compatibility names.

## Core Drawing And Text

| Public name | Current decision | Notes |
|---|---|---|
| `Background` | `.kry canonical` | Viewport bounds policy is in `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `.kry canonical` | Public props live in `runtime/text_props.kry`; single canonical signature is `Text(TextProps)`. |
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
| `Button` | `.kry canonical` | Single public button surface. Menu, split-action, icon-only, arrow, info/help, loading, disclosure, tone, and emphasis behavior should live in `ButtonProps` or small `.kry` composition, not separate public widget names. |
| `Link` | `.kry canonical` | Canonical public name for URL/link activation; color/hover/disabled policy is in `.kry`, typography uses resolved KSS font sizes directly, and URL dispatch remains host support. |
| `TextField` | `.kry canonical` | Metrics, horizontal scroll, paint geometry, buffer-limit, navigation, selection state, and edit intent policy are in `.kry`; buffer mutation, IME, selection ownership/painting, and rendering remain native host support. |
| `TextArea` | `.kry canonical` | Metrics, page-navigation rows, paint geometry, buffer-limit, navigation, selection state, and edit intent policy are in `.kry`; buffer mutation, IME, selection ownership/painting, and rendering remain native host support. |
| `Dropdown` | `.kry canonical` | Popup placement, row/window, scrollbar, scrolling, navigation, and indicator policy are in `.kry`; trigger and option typography use resolved KSS font sizes directly. |
| `Slider` | `.kry canonical` | Public props live in `runtime/slider_props.kry`; value type, orientation, angle/unit, component/editor/hit layout, and text paint geometry live in `SliderProps`/`.kry`; generated Go uses `kr.Slider`. |
| `Drag` | `.kry canonical` | Public props live in `runtime/drag_props.kry`; value type, range mode, component layout, and text paint geometry live in `DragProps`/`.kry`; generated Go uses `kr.Drag`. |
| `Input` | `.kry canonical` | Public props live in `runtime/input_props.kry`; value type, values, component/step-button layout, and step policy live in `.kry`; generated Go uses `kr.Input`; embedded editing uses `TextField` typography and step controls use `Button` typography. |
| `Spinbox` | `.kry canonical` | Public props live in `runtime/spinbox_props.kry`; layout and value stepping policy are in `.kry`; host handles button input and drawing. |
| `Toggle` | `.kry canonical` | Public props live in `runtime/toggle_props.kry`; paint/layout policy is in `.kry`, label typography is KSS-owned, host handles input and drawing. |
| `Checkbox` | `.kry canonical` | Public props live in `runtime/checkbox_props.kry`; paint, row/text layout, and flags toggle policy are in `.kry`; host handles input and drawing. |
| `Radio` | `.kry canonical` | Public props live in `runtime/radio_props.kry`; paint, layout, and marker text policy are in `.kry`; host handles focus/input and drawing. |
| `Selectable` | `.kry canonical` | Public props live in `runtime/selectable_props.kry`; paint/layout policy is in `.kry`; review whether list item props should absorb it later. |
| `Progress` | `.kry canonical` | Public props live in `runtime/progress_props.kry`; prefer one public progress name. |
| `ColorPicker` | `.kry canonical` | Public props live in `runtime/color_picker_props.kry`; channel layout, swatch paint geometry, and color conversion are in `.kry`; swatch activation is `Button` with swatch props. |
| `SegmentedControl` | `.kry canonical` | Layout policy is in `.kry`; segment typography and paint are KSS-owned; host handles label measurement, focus/input, and button drawing. |
| `LabelTextField` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `TextField`. |
| `CheckboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Checkbox`. |
| `SpinboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Spinbox`. |
| `ButtonRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Row` with `Button` children. |
| `SectionLabel` | Removed | Removed from public headers; internal row helper only. Public code should use `Text`/`Heading` props or `.kry` composition. |
| `InfoRows` | Native support | Internal repeated label/value row helper; typography uses resolved KSS font sizes directly; public forms should use `.kry` layout with `Row`/`Text`. |
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
| `Separator` | `.kry canonical` | Public props live in `runtime/separator_props.kry`; line, label, and bullet layout/paint policy are in `.kry`; label typography is KSS-owned; host handles text measurement and drawing. |
| `Fieldset` | `.kry canonical` | Public props live in `runtime/fieldset_props.kry`; titled border group. |
| `PanedView` | `.kry canonical` | Public props live in `runtime/paned_view_props.kry`; split clamp and handle geometry are in `.kry`; host keeps drag/input ownership. |
| `Collapsible` | `.kry canonical` | Public props live in `runtime/collapsible_props.kry`; header metrics, geometry, marker text, and typography defaults are in `.kry`/KSS; host keeps input, focus, tree navigation, and drawing. |
| `Scroll` | `.kry canonical` | Public props live in `runtime/scroll_props.kry`; lexical scroll-content block. Measurement and sizing policy are in `.kry`; host keeps wheel/drag/clipping and lowered scope ownership. |
| `TableCell` | `.kry canonical` | Lexical custom table-cell block; lowers to host cell scope. |
| `ScrollContainer` | Native support | Internal host helper only; public callers should use `Scroll` blocks. |
| `ScrollPage` | Native support | Internal host helper only; not a public widget concept. |
| `ScreenScaffold` | Native support | Internal app-shell helper only; compose pages from `.kry` layout. |

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
| `ListBox` | `.kry canonical` | Layout/navigation and row paint geometry policy is in `.kry`; item typography is KSS-owned. Multi-selection uses `selected`, `selected_count`, and `anchor` props. KSS styles multi-select mode with `ListBoxMulti` and `ListBoxMultiItem`, not a separate `MultiSelectList` widget. Host handles input sampling, scroll scope, and drawing. |
| `TreeView` | `.kry canonical` | Row/window and paint geometry policy is in `.kry`; item typography defaults are KSS-owned; host handles input, selection mutation, expansion state, and drawing. |
| `TableView` | `.kry canonical` | Header/body/frozen-row/scroll/scrollbar/cell geometry is in `.kry`; header, cell, and selection text typography is KSS-owned, including native fallback sizing; host handles column ordering, input, selection mutation, resizing, clipboard, and drawing. |
| `CanvasGrid` | `.kry canonical` | Grid spacing, line counts, and line rectangles are in `.kry`; host handles drawing. |
| `Canvas` | `.kry canonical` | Transform, hit-test, and result policy are in `.kry`; host keeps clip/camera renderer scope. |
| `DragDrop` | `.kry canonical` | Typed drag/drop interaction concept. Source and target roles belong in props or composition; decision policy is in `.kry`, host keeps payload storage, type comparison, and pointer ownership. |

## Navigation

| Public name | Current decision | Notes |
|---|---|---|
| `NavigationBar` | `.kry canonical` | Paint, sizing, and configuration modal layout policy are in `.kry`; item and configuration-slot labels use `NavigationBarItem` KSS typography. |
| `Toolbar` | `.kry canonical` | Metrics and geometry policy are in `.kry`; host handles input, drawing, and child `Button`/`Dropdown` calls. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props. |
| `TabBar` | `.kry canonical` | Sizing, scroll, and reorder marker policy are in `.kry`; tab label typography is KSS-owned, including native fallback sizing; host handles input sampling, drag state, and drawing. |
| `TitleBar` | `.kry canonical` | Layout and paint geometry policy are in `.kry`; title typography uses resolved KSS font sizes directly; leading action and dropdown behavior live in `TitleBarProps`. |
| `Router` | Native support | Navigation runtime, not a visual widget. |
| `Link` | `.kry canonical` | Canonical navigation/link widget. |

## Overlays And Feedback

| Public name | Current decision | Notes |
|---|---|---|
| `Popup` | `.kry canonical` | Arbitrary anchored/floating content. Mode/input policy is in `.kry`; host handles pointer sampling, paint layers, clipping, and child content. |
| `Modal` | `.kry canonical` | Layout, frame geometry, and action sizing policy are in `.kry`; title, message, and action text typography is KSS-owned with resolved font sizes used directly; prompt fields use `TextField` typography; host handles capture, input, text editing, and drawing. |
| `Toast` | `.kry canonical` | Public toast feedback surface. Duration and layout policy are in `.kry`; host keeps message storage, timing source, truncation, and drawing. |
| `Focus` | Native support | Focus ring geometry is in `.kry`; focus state remains host support. |
| `Guide` | `.kry canonical` | Guided overlay flow. The clean public API is one `Guide(GuideProps)` surface with step data in props; `GuideStep` is data, not a widget. Label typography uses resolved KSS font sizes directly. Current C rendering is host support around `runtime/guide.kry` policy. |
| `GuideStep` | Props/data only | One anchored instruction inside `GuideProps`; not a standalone widget. |
| `GuidePager` | Internal support | Not a public widget. Footer layout/page transition policy is `.kry`; the C helper lives under `src/ui` and is not exported by public headers. |
| `StylePicker` | `.kry canonical` | Public props live in `runtime/style_picker_props.kry`; style-pack storage, KSS parsing, and dropdown rendering remain host support. |

## Game2D Nodes

Game2D has its own node family. These are canonical for the Game2D domain and
stay separate from general UI widgets. Concrete node props/enums live in
`runtime/node2d_props.kry`; scene graph ownership and host resources remain
native Game2D support.

| Public name | Current decision | Notes |
|---|---|---|
| `Scene` | Game2D native scene | Root game scene; add `.kry` declaration support later. |
| `Node2D` | Game2D native scene | Base 2D node; add `.kry` declaration support later. |
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

## Escape Hatches

| Public name | Current decision | Notes |
|---|---|---|
| `Custom` | Native support | Escape hatch, not preferred public design surface. |

## Retained Node Kinds

These are the current internal retained UI tree kind values. They are no longer
public header constants; public inspection uses `GetNodeKindName(kind)` and gets
clean names such as `Button`, `TextField`, and `Image`. Use this table for
naming feedback on the internal/runtime mapping while the app-facing surface
stays prefix-free.

| Current node kind | Public widget/concept | Decision |
|---|---|---|
| `WIDGET_SCREEN` | `Screen` | `.kry canonical`; viewport fallback bounds policy is `.kry-backed` |
| `WIDGET_BACKGROUND` | `Background` | `.kry-backed`; bounds and app fallback policy live in runtime primitive policy |
| `WIDGET_TEXT` | `Text` | `.kry canonical` |
| `WIDGET_BOX` | `Box` | `.kry-backed`; retained node kind now matches the public `Box` concept |
| `WIDGET_CIRCLE` | `Circle` | `.kry-backed`; public code uses `Circle` |
| `WIDGET_RING` | `Ring` | `.kry-backed`; public code uses `Ring` |
| `WIDGET_LINE` | `Line` | `.kry-backed`; measured bounds and retained endpoints come from runtime primitive policy |
| `WIDGET_TRIANGLE` | `Triangle` | `.kry-backed`; public code uses `Triangle` |
| `WIDGET_BUTTON` | `Button` | `.kry canonical` |
| `WIDGET_TEXT_FIELD` | `TextField` | `.kry canonical`; metrics, horizontal scroll, paint geometry, buffer-limit, navigation, selection state, and edit intent migrated; buffer mutation and IME still host support |
| `WIDGET_TEXT_AREA` | `TextArea` | `.kry canonical`; metrics, page rows, paint geometry, buffer-limit, navigation, selection state, and edit intent migrated; buffer mutation and IME still host support |
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
| `UIWidgetKind`, `UI_WIDGET_*_NODE`, public `WIDGET_*` constants | `int` as an opaque integer plus `GetNodeKindName(kind)` |
| `UIWidgetNode` | Opaque `TreeNode` inspection handle plus `GetNode*` accessors |
| `UIWidgetData` | Internal `WidgetData`; public code uses clean node inspection helpers |
| public C `ButtonSpec` | Internal retained/render payload; public code uses `Button(ButtonProps)` |
| text input paint snapshot | Internal `TextInputPaint` host snapshot. |
| `UIAccessibilitySink` | `AccessibilitySink` |
| `UIInspect*`, `BeginUIInspect*`, `PushUIInspect*`, `IsUIInspectActive` | `Inspect*`, `BeginInspect*`, `PushInspect*`, `IsInspectActive` |
| `UIDPIState`, `UI_DPI_BASE_*`, `InitUIDPI`, `GetUIDPI*` | `DPIState`, `DPI_BASE_*`, `InitDPI`, `GetDPI*` |
| `UIFrameState`, `InitUI`, `BeginUIFrame`, `EndUIFrame` | `FrameState`, `InitInterface`, `BeginInterfaceFrame`, `EndInterfaceFrame` |
| `SetUI*`, `GetUI*`, `IsUI*`, `ClearUI*`, `PushUI*`, `PopUI*` focus/input helpers | `Set*`, `Get*`, `Is*`, `Clear*`, `Push*`, `Pop*` focus/input helpers |
| `UIFont*`, `UI_FONT_*`, `EnsureUIDefaultFont`, `RegisterUISmallFont` | `TextFont*`, `TEXT_FONT_*`, `EnsureDefaultFont`, `RegisterSmallTextFont` |
| `UIClipboard*`, `UI_CLIPBOARD_*`, `UIPrimarySelection*` | `Clipboard*`, `CLIPBOARD_*`, `PrimarySelection*` |
| old profile image helpers | `ProfileImage*`, `SyncProfileIcon`, `SYNC_PROFILE_ICON_*` |
| `SetUIViewSize`, `GetUIViewWidth`, `GetUIViewHeight` | `SetViewSize`, `GetViewWidth`, `GetViewHeight` |
| `GetUICenteredColumn`, `GetUIPageSidePadding` | `GetCenteredColumn`, `GetPageSidePadding` |
| `SetUIScale`, `GetUIScale`, `ClampUIPx` | `SetScale`, `GetScale`, `ClampPx` |
| `LightenUIColor`, `DarkenUIColor` | `LightenColor`, `DarkenColor` |
| `BeginUIClip`, `EndUIClip`, `ResetUIClip`, `GetUIClip*` | `BeginClip`, `EndClip`, `ResetClip`, `GetClip*` |
| `GetFontSize`, `GetSmallFontSize`, `GetTitleFontSize`, `FitFontSize` | Internal native/KSS typography helpers; public code uses explicit text tokens or style props. |
| `UIFloatDrag*`, `UIIntDrag*`, `UIFloatSlider*`, `UIIntSlider*`, typed fixture values | Public code uses `Drag(DragProps)` and `Slider(SliderProps)` with value kind/props; scalar/whole helper splits are internal runtime policy only. |
| `BeginWidget`, `EndWidget`, `WidgetSet*`, `WIDGET_MOVABLE`/`WIDGET_RESIZABLE`/`WIDGET_READONLY` | Internal inspect registration; public code uses canonical widget declarations. |
| `MeasureGrid`, `BeginGridCursor`, `GridStep`, `GridCursorHeight` | Internal `.kry` grid placement policy; public code uses `Grid(GridProps)`. |
| app-facing `Texture`, `DrawTexture`, `DrawTexturePro`, `DrawTextureRec` fixes | `Image(ImageProps)`; if `ImageProps` cannot express the app case, add the reusable Kryon image primitive first. |
| Go package-level `BeginButton`, `BeginCard` | Removed; public Go code uses `kr.Button` and `kr.Card`. Runtime methods remain internal lowering support until composed block lowering is canonicalized. |
| Go package-level `ClosePopup` | Removed; app `.kry` closes a popup by updating its caller-owned `open` state. Runtime methods remain internal host/test support. |
| public C `ClosePopup` | Removed; app `.kry` closes a popup by updating its caller-owned `open` state. Native tests use the internal host hook. |
| `BeginTabBar`, `BeginTabItem`, `EndTabItem`, `EndTabBar` | `TabBar` plus caller-owned selected state and ordinary conditionals |
| direct `.kry` `BeginScroll`/`EndScroll` and `BeginTableCell`/`EndTableCell` calls | `Scroll` and `TableCell` lexical blocks |
| direct `.kry` `BeginCanvas`/`EndCanvas` calls | `Canvas` lexical block |

`include/*.h` and `docs/PUBLIC_API_SNAPSHOT.txt` are guarded by
`canonical-surface-test`: public `UI*`/`UI_*` prefixes are not accepted there.

## Cleanup Queue

1. Finish text editing policy migration:
   `TextField` and `TextArea` already own metrics, paint geometry,
   buffer-limit, navigation, edit-intent, and selection range policy in
   `.kry`; remaining native work is buffer mutation, IME/composition,
   selection ownership/painting, and the final decision about how much of that
   can become reusable `.kry` policy.
2. Finish rich text migration:
   `Paragraph` has `.kry` metrics/default policy and generated
   `ParagraphSpec` data, but parsing, reflow, icon shaping, and rendering are
   still host work.
3. Audit host-owned input/state lifecycles:
   retained menu open/focus/input state, drag/drop payload storage, reorder and
   swipe pointer ownership, paned-view drag ownership, tree/table selection
   mutation, table resizing/clipboard, modal input capture, and toast message
   storage/timing are still native support around `.kry` policy.
4. Finish lowered block backend cleanup:
   `Scroll`, `Popup`, `Disabled`, `TableCell`, `Canvas`, and composed content
   blocks are canonical `.kry` syntax, but their lowered host scopes still
   require backend support until generated backends own the whole block path.
5. Separate pure host services from widget policy:
   image cache/loading/drawing, icon sheet/type lookup, URL dispatch, text
   measurement, focus registration, paint layers, clipping, and platform
   services should stay native only when they are true host services and not
   widget policy.
6. Decide the Game2D boundary:
   Game2D props/enums are generated from `runtime/node2d_props.kry`, but
   `Scene`/`Node2D` declarations, scene lifecycle, physics/audio handles,
   asset playback, and rendering remain native scene support. Either keep this
   as an explicit non-widget domain or add `.kry` scene declaration support.
7. Keep lowered host scopes out of public `.kry` documentation:
   immediate-mode `Begin*`/`End*` wrappers are native support, not widget names.
   Tutorial image helpers remain internal and route through canonical `Image`
   policy.
8. Keep `docs/IMGUI_WIDGET_COVERAGE.md` as the coverage audit. Use this file
   as the naming and migration review surface.
