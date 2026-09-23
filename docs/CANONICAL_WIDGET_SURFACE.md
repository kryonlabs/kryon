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
| `runtime/accessibility_policy.kry` | Accessibility focus and activation eligibility | `.kry canonical` |
| `runtime/accessibility_props.kry` | Accessibility action contract | `.kry canonical` |
| `runtime/button.kry` | Button composition, input, metrics, content, and fallback paint policy | `.kry canonical` |
| `runtime/button_props.kry` | Button props | `.kry canonical` |
| `runtime/canvas.kry` | Canvas transform and hit-test policy | `.kry canonical` |
| `runtime/canvas_props.kry` | Canvas props and result | `.kry canonical` |
| `runtime/capability_props.kry` | Capability flag enum and viewport safe-area geometry | `.kry canonical` |
| `runtime/canvas_grid.kry`, `src/ui/canvas.kry` | CanvasGrid lines, drawing, and hit testing | `.kry canonical` |
| `runtime/card.kry` | Card composition | `.kry canonical` |
| `runtime/card_props.kry` | Card props | `.kry canonical` |
| `runtime/checkbox.kry`, `src/ui/checkbox.kry` | Checkbox style, paint, input, row/text layout, and value/flag policy | `.kry canonical` |
| `runtime/checkbox_props.kry` | Checkbox props | `.kry canonical` |
| `runtime/collapsible.kry` | Collapsible metrics/header geometry, pointer, close, keyboard, and open-state policy | `.kry canonical` |
| `runtime/collapsible_props.kry` | Collapsible props | `.kry canonical` |
| `runtime/color_picker.kry`, `src/ui/color_picker.kry` | ColorPicker channel, swatch, color, slider composition, and drawing | `.kry canonical` |
| `runtime/color_picker_props.kry` | ColorPicker props | `.kry canonical` |
| `runtime/control_props.kry` | Shared control props | `.kry canonical` |
| `runtime/drawing_props.kry` | Shared drawing props and paragraph spec data | `.kry canonical` |
| `runtime/dropdown.kry` | Dropdown composition, option/index normalization, open/dismiss/commit-close state, popup, row, scrollbar, keyboard intent, navigation, and indicator geometry policy | `.kry canonical` |
| `runtime/dropdown_props.kry` | Dropdown rich item data and props | `.kry canonical` |
| `runtime/drag_drop.kry`, `src/ui/drag_drop.kry` | DragDrop source/target lifecycle, input, and drawing | `.kry canonical` |
| `runtime/drag_drop_props.kry` | DragDrop props and role enum | `.kry canonical` |
| `runtime/drag.kry` | Drag component layout, text paint geometry, typed keyboard input, and value/keyboard policy | `.kry canonical` |
| `src/ui/drag.kry`, `src/ui/numeric_edit.kry`, `src/ui/widget_store.kry` | Drag interaction and paint, numeric text editing and step buttons, stable editor state | `.kry canonical` |
| `runtime/drag_props.kry` | Drag props | `.kry canonical` |
| `runtime/grid.kry` | Grid composition | `.kry canonical` |
| `runtime/grid_props.kry` | Grid props | `.kry canonical` |
| `runtime/group.kry` | Group bounds/content policy | `.kry canonical` |
| `runtime/guide.kry` | Guide overlay layout, arrow geometry, and step policy | `.kry canonical` |
| `runtime/guide_pager.kry` | Internal pager footer layout and page transition policy | `.kry support` |
| `runtime/icon.kry` | Icon bounds/size policy | `.kry canonical` |
| `runtime/icon_sheet_props.kry` | Icon sheet selection enum | `.kry support` |
| `src/ui/icon.kry` | Icon shape selection, raster drawing, and profile icon behavior; atlas loading stays in the backend | `.kry canonical` |
| `runtime/image.kry` | Image fit and placeholder layout policy | `.kry canonical` |
| `runtime/image_props.kry` | Image props | `.kry canonical` |
| `runtime/input.kry` | Input step-button default, component layout, value/step, temp-edit activation, and generic pointer activation/consume policy | `.kry canonical` |
| `runtime/input_props.kry` | Input props and shared numeric value kind | `.kry canonical` |
| `runtime/instance.kry` | Generated widget instance identity helpers | `.kry support` |
| `runtime/instance_props.kry` | Instance-state borrowing declaration | `.kry support` |
| `runtime/core_props.kry` | Interface frame, focus, and cursor ABI declarations | `.kry support` |
| `runtime/tree_props.kry` | Legacy tree lifecycle and widget entry point declarations; Router declarations have moved to checked Ziran | migration pending |
| `runtime/locale_props.kry` | Locale catalog ABI declarations | `.kry support` |
| `runtime/style_pack_props.kry` | Style pack registration and resolution ABI | `.kry support` |
| `runtime/key_props.kry` | Widget key identity type | `.kry support` |
| `runtime/app_host_props.kry` | Host embedding and app runtime ABI | `.kry support` |
| `src/ui/route.zi` | Portable app-shell route policy; caller owned slices and counts | checked Ziran |
| `runtime/frame_props.kry` | Frame lifecycle and pacing ABI declarations | `.kry support` |
| `runtime/inspect.kry` | Inspector edit/resize geometry, handle-size, and release-consume policy | `.kry support` |
| `runtime/inspect_props.kry` | Inspector selection and node records | `.kry support` |
| `runtime/fieldset.kry` | Fieldset layout/paint policy | `.kry canonical` |
| `runtime/fieldset_props.kry` | Fieldset props | `.kry canonical` |
| `runtime/focus.kry` | Focus ring, activation, tab-direction, and debug overlay geometry policy | `.kry support` |
| `src/ui/focus.kry` | Focus state, traversal, registration, keyboard activation, and ring drawing | `.kry canonical` |
| `src/ui/input_capture.kry` | Pointer gesture, press origin, click activation, modal capture, and input clip stacks | `.kry canonical` |
| `src/ui/cursor.kry` | Cursor intent, priority, and frame reset | `.kry canonical` |
| `runtime/layout.kry` | Column/Row/Stack content and child placement policy | `.kry canonical` |
| `runtime/layout_props.kry` | Column/Row/Flow layout props | `.kry canonical` |
| `runtime/link.kry` | Link bounds, interaction, activation/release-consumption, state, and color policy | `.kry canonical` |
| `runtime/link_props.kry` | Link props | `.kry canonical` |
| `runtime/list_box.kry` | ListBox layout, keyboard navigation intent, and row paint geometry policy | `.kry canonical` |
| `runtime/list_box_props.kry` | ListBox props | `.kry canonical` |
| `runtime/material.kry` | Material layer assembly with typed `MaterialKind` policy | `.kry canonical` |
| `runtime/menu.kry`, `src/ui/menu_host.kry` | Menu metrics, geometry, navigation, accelerator matching, panel tracking, bar/context open/index/outside-close policy, and group pointer open/close decisions | `.kry canonical` |
| `runtime/menu_props.kry` | Menu item/group/result data and props | `.kry canonical` |
| `runtime/list_box_multi.kry` | ListBox multi-selection row activation, keyboard navigation, and selection policy | `.kry canonical` |
| `runtime/navigation_bar.kry` | Navigation bar default-height, item interaction, paint, and configuration layout/count/default policy | `.kry canonical` |
| `runtime/navigation_bar_props.kry` | NavigationBar props and result | `.kry canonical` |
| `runtime/node2d_props.kry` | Game2D scene/node declaration props, defaults, node props, and enums | `.kry canonical` |
| `runtime/node_registry_props.kry` | Public node registry flags | `.kry support` |
| `runtime/node_props.kry` | Retained node kind/flags, node record, and size constants | `.kry canonical` |
| `runtime/scene_tree_props.kry` | Game2D scene node kind and flag values | `.kry support` |
| `runtime/paint.kry` | Paint/drawing helpers | `.kry support` |
| `runtime/paned_view.kry` | PanedView split/layout/handle geometry, drag lifecycle, and change policy | `.kry canonical` |
| `runtime/paned_view_props.kry` | PanedView props | `.kry canonical` |
| `runtime/page.kry`, `src/ui/page.kry` | Page, Section, Heading, ParagraphText, Link, and Flow composition | `.kry canonical` |
| `runtime/page_props.kry` | Page/Section/Heading/Paragraph props | `.kry canonical` |
| `runtime/paragraph.kry` | Paragraph metrics/default line-gap, layout spacing, line-step, height, line stride, alignment, and selectable line-index/local-offset policy | `.kry canonical` |
| `runtime/plot.kry`, `src/ui/plot.kry` | Plot geometry, text, style, and drawing | `.kry canonical` |
| `runtime/plot_props.kry` | Plot props and mode names | `.kry canonical` |
| `runtime/popup_policy.kry` | Popup mode/input record/open-state, tooltip visibility, outside-dismissal, and Escape-close policy | `.kry canonical` |
| `runtime/popup_props.kry` | Popup props | `.kry canonical` |
| `runtime/primitive.kry` | Background/Box/Line/Circle/Ring/Triangle primitive geometry policy | `.kry canonical` |
| `runtime/profile_header.kry` | Profile header geometry/text placement plus profile pointer activation and profile image picker geometry/selection/click policy | `.kry support` |
| `runtime/profile_icon_props.kry` | Profile image (sync) icon selection enum | `.kry support` |
| `runtime/window_props.kry` | Additional OS window flag values | `.kry support` |
| `runtime/dpi_props.kry` | Device pixel-ratio state record | `.kry support` |
| `runtime/progress.kry`, `src/ui/progress.kry` | Progress layout, style, and drawing | `.kry canonical` |
| `runtime/progress_props.kry` | Progress props | `.kry canonical` |
| `runtime/radio.kry`, `src/ui/radio.kry` | Radio paint, layout, activation, animation, and drawing | `.kry canonical` |
| `runtime/radio_props.kry` | Radio props | `.kry canonical` |
| `runtime/reorder.kry` | Reorder metrics, handle/placeholder paint geometry, target-index, lifecycle/release gates, and result normalization policy | `.kry support` |
| `runtime/reorder_props.kry` | Reorder support state, data, and result records | `.kry support` |
| `src/ui/router_props.zi`, `src/ui/router.zi` | Portable Router records, navigation, hash matching, URL effect, and retained node | checked Ziran |
| `runtime/rows.kry` | Info/form/button row sizing, wrapping, and layout fallback policy | `.kry canonical` |
| `runtime/segmented_control.kry` | SegmentedControl layout, gap, font fallback, wrapping, segment sizing, and selection policy | `.kry canonical` |
| `runtime/segmented_control_props.kry` | SegmentedControl props and result | `.kry canonical` |
| `runtime/selectable.kry`, `src/ui/selectable.kry` | Selectable paint, layout, input, and toggle | `.kry canonical` |
| `runtime/selectable_props.kry` | Selectable props | `.kry canonical` |
| `runtime/separator.kry`, `src/ui/separator.kry` | Separator/Bullet layout, style, and paint | `.kry canonical` |
| `runtime/separator_props.kry` | Separator props | `.kry canonical` |
| `runtime/slider.kry` | Slider composition, component/editor/hit layout, text paint geometry, and value/keyboard policy | `.kry canonical` |
| `runtime/slider_props.kry` | Slider props | `.kry canonical` |
| `runtime/spinbox.kry`, `src/ui/spinbox.kry` | Spinbox layout, style, input, value, and drawing | `.kry canonical` |
| `runtime/spinbox_props.kry` | Spinbox props | `.kry canonical` |
| `runtime/scroll.kry` | Scroll measurement, sizing, wheel, content-drag decision, thumb drag offset, scrollbar drag/release decision, and ensure-visible policy | `.kry canonical` |
| `runtime/scroll_props.kry` | Scroll props | `.kry canonical` |
| `runtime/kss_formatter.kry` | The KSS formatter: stable re-layout from parser-verified construct spans with comment preservation | `.kry canonical` |
| `runtime/kss_parser.kry` | The KSS parser: tokens, theme/env overlays, imports, layers, provenance, and diagnostics shared by all backends | `.kry canonical` |
| `runtime/style.kry` | Style helpers | `.kry canonical` |
| `runtime/style_picker_props.kry` | StylePicker props and option/selection/dropdown state policy | `.kry canonical` |
| `runtime/style_token_props.kry` | Named style color replacement token | `.kry support` |
| `runtime/style_sheet.kry` | Style sheet evaluation helpers | `.kry canonical` |
| `runtime/surface.kry` | Surface/container helpers | `.kry canonical` |
| `runtime/swipe.kry` | Swipe begin, drag, release, lifecycle, direction, default, and progress policy | `.kry support` |
| `runtime/swipe_props.kry` | Swipe support state, data, and result records | `.kry support` |
| `runtime/tab_bar.kry` | TabBar sizing, scroll, keyboard index, reorder marker/drag lifecycle/release-consumption, tab pointer release-consumption, and double-click decision policy | `.kry canonical` |
| `runtime/tab_bar_props.kry` | TabBar props | `.kry canonical` |
| `runtime/text.kry` | Text composition, selectable pointer/drag/copy/show decisions, highlight geometry, and double-click line-selection policy | `.kry canonical` |
| `runtime/text_props.kry` | Text props | `.kry canonical` |
| `runtime/text_rows.kry` | Source-preserving visual-row breaks, logical lines, heading metrics, caret affinity and point-to-row decisions | `.kry support` |
| `runtime/text_input.kry` | TextField/TextArea defaults, metrics, scroll, wrap thresholds, caret/IME stroke metrics, paint geometry, buffer-limit, cursor normalization, navigation, selection state/paint-span policy, double-click/pan decisions, text-buffer mutation/range/bracket policy, focus ownership, platform text-input sync, and edit-intent policy | `.kry canonical` |
| `src/ui/text.kry`, `src/ui/text_edit.kry` | Control text fitting and clipping, TextArea gutter, and text-buffer line/edit commands | `.kry canonical` |
| `runtime/text_input_props.kry` | TextField/TextArea props and text input style enums | `.kry canonical` |
| `runtime/theme.kry` | Theme data/helpers and typed `ThemePolicy` resolution | `.kry canonical` |
| `runtime/title_bar.kry` | TitleBar effective state, layout, reservation, and paint geometry policy | `.kry canonical` |
| `runtime/title_bar_props.kry` | TitleBar props | `.kry canonical` |
| `runtime/toggle.kry` | Toggle composition, paint/layout, and value policy | `.kry canonical` |
| `runtime/toggle_props.kry` | Toggle props | `.kry canonical` |
| `runtime/toolbar.kry` | Toolbar, bottom icon row, and icon slider popup metrics/geometry/style-size/open/close policy | `.kry canonical` |
| `src/ui/table_view.kry` | TableView column layout, clipboard, KSS metrics, and keyboard selection | `.kry canonical` |
| `runtime/toolbar_props.kry` | Toolbar and bottom icon row props/results | `.kry canonical` |
| `runtime/toast.kry` | Toast style facts, request/render decision, duration, layout, text-placement, and truncation policy | `.kry canonical` |
| `runtime/toast_props.kry` | Toast props | `.kry canonical` |
| `runtime/transition_fade.kry` | Transition fade alpha/easing policy | `.kry support` |
| `runtime/transition_props.kry` | Transition phase enum names | `.kry support` |
| `runtime/modal.kry` | Modal layout, frame geometry, message line-gap, outside-dismissal, prompt availability/focus fallback, prompt input/result, and action row policy | `.kry canonical` |
| `runtime/modal_props.kry` | Modal props and action props | `.kry canonical` |
| `runtime/popup_ownership.kry` | Popup ancestry, branch ordering, input capture, focus acquisition/restoration, and owner retirement | `.kry support` |
| `runtime/overlay.kry` | Internal dismissible overlay viewport, dismissal, and release-consumption policy | `.kry support` |
| `runtime/tree_view.kry` | TreeView row/window, paint geometry, marker text, and row-selection decision policy | `.kry canonical` |
| `runtime/tree_view_props.kry` | TreeView props | `.kry canonical` |
| `runtime/table_view.kry` | TableView layout, scroll, scrollbar, cell geometry, header-angle normalization, header/row hot/pointer decisions, keyboard selection, activation, clear-selection, resize start/drag/clear/width lifecycle, and clipboard intent policy | `.kry canonical` |
| `runtime/table_view_props.kry` | TableView row and props | `.kry canonical` |
| `runtime/widget_kind.kry` | Retained tree node kind and internal flag values | `.kry support` |

| `runtime/accelerator.kry` | Accelerator policy module | `.kry canonical` |
| `runtime/accessibility_node.kry` | Accessibility Node policy module | `.kry canonical` |
| `runtime/clipboard.kry` | Clipboard policy module | `.kry canonical` |
| `runtime/tree.kry` | Tree policy module | `.kry canonical` |
| `src/ui/accessibility.kry` | Accessibility widget host surface | `.kry canonical` |
| `src/ui/bevel.kry` | Bevel widget host surface | `.kry canonical` |
| `src/ui/button.kry` | Button widget host surface | `.kry canonical` |
| `src/ui/clip.kry` | Clip widget host surface | `.kry canonical` |
| `src/ui/clipboard.kry` | Clipboard widget host surface | `.kry canonical` |
| `src/ui/clipboard_protocol.kry` | Clipboard Protocol widget host surface | `.kry canonical` |
| `src/ui/collapsible.kry` | Collapsible widget host surface | `.kry canonical` |
| `src/ui/disabled.kry` | Disabled widget host surface | `.kry canonical` |
| `src/ui/dpi.kry` | Dpi widget host surface | `.kry canonical` |
| `src/ui/dropdown.kry` | Dropdown widget host surface | `.kry canonical` |
| `src/ui/dropdown_store.kry` | Dropdown Store widget host surface | `.kry canonical` |
| `src/ui/fieldset.kry` | Fieldset widget host surface | `.kry canonical` |
| `src/ui/focus_debug.kry` | Focus Debug widget host surface | `.kry canonical` |
| `src/ui/frame.kry` | Frame widget host surface | `.kry canonical` |
| `src/ui/grapheme.kry` | Grapheme widget host surface | `.kry canonical` |
| `src/ui/guide.kry` | Guide widget host surface | `.kry canonical` |
| `src/ui/icon_assets.kry` | Icon Assets widget host surface | `.kry canonical` |
| `src/ui/icon_controls.kry` | Icon Controls widget host surface | `.kry canonical` |
| `src/ui/image.kry` | Image widget host surface | `.kry canonical` |
| `src/ui/inspect_overlay.kry` | Inspect Overlay widget host surface | `.kry canonical` |
| `src/ui/inspect_state.kry` | Inspect State widget host surface | `.kry canonical` |
| `src/ui/kss_parser.kry` | Kss Parser widget host surface | `.kry canonical` |
| `src/ui/link.kry` | Link widget host surface | `.kry canonical` |
| `src/ui/list_box.kry` | List Box widget host surface | `.kry canonical` |
| `src/ui/list_box_multi.kry` | List Box Multi widget host surface | `.kry canonical` |
| `src/ui/modal.kry` | Modal widget host surface | `.kry canonical` |
| `src/ui/modal_tree.kry` | Modal Tree widget host surface | `.kry canonical` |
| `src/ui/navigation_bar.kry` | Navigation Bar widget host surface | `.kry canonical` |
| `src/ui/node_registry.kry` | Node Registry widget host surface | `.kry canonical` |
| `src/ui/numeric.kry` | Numeric widget host surface | `.kry canonical` |
| `src/ui/overlay.kry` | Overlay widget host surface | `.kry canonical` |
| `src/ui/pager.kry` | Pager widget host surface | `.kry canonical` |
| `src/ui/paint_command.kry` | Resolves KSS widget frames and draws them | `.kry canonical` |
| `src/ui/paint_frame.kry` | Paint Frame widget host surface | `.kry canonical` |
| `src/ui/paint_layers.kry` | Paint Layers widget host surface | `.kry canonical` |
| `src/ui/paned_view.kry` | Paned View widget host surface | `.kry canonical` |
| `src/ui/pointer_frame.kry` | Pointer Frame widget host surface | `.kry canonical` |
| `src/ui/popup.kry` | Popup widget host surface | `.kry canonical` |
| `src/ui/popup_input_store.kry` | Popup Input Store widget host surface | `.kry canonical` |
| `src/ui/profile_header.kry` | Profile Header widget host surface | `.kry canonical` |
| `src/ui/reorder.kry` | Reorder widget host surface | `.kry canonical` |
| `src/ui/rows.kry` | Rows widget host surface | `.kry canonical` |
| `src/ui/scroll.kry` | Scroll widget host surface | `.kry canonical` |
| `src/ui/segmented_control.kry` | Segmented Control widget host surface | `.kry canonical` |
| `src/ui/selectable_text.kry` | Selectable Text widget host surface | `.kry canonical` |
| `src/ui/slider.kry` | Slider widget host surface | `.kry canonical` |
| `src/ui/sprite_sheet.kry` | Sprite Sheet widget host surface | `.kry canonical` |
| `src/ui/style_builtin_packs.kry` | Style Builtin Packs widget host surface | `.kry canonical` |
| `src/ui/style_effects.kry` | Style Effects widget host surface | `.kry canonical` |
| `src/ui/style_metrics.kry` | Style Metrics widget host surface | `.kry canonical` |
| `src/ui/style_pack_source.kry` | Style Pack Source widget host surface | `.kry canonical` |
| `src/ui/style_palette.kry` | Style Palette widget host surface | `.kry canonical` |
| `src/ui/style_picker.kry` | Style Picker widget host surface | `.kry canonical` |
| `src/ui/style_sheet.kry` | Style Sheet widget host surface | `.kry canonical` |
| `src/ui/style_values.kry` | Style Values widget host surface | `.kry canonical` |
| `src/ui/swipe.kry` | Swipe widget host surface | `.kry canonical` |
| `src/ui/syntax_paint.kry` | Syntax Paint widget host surface | `.kry canonical` |
| `src/ui/tab_bar.kry` | Tab Bar widget host surface | `.kry canonical` |
| `src/ui/tab_store.kry` | Tab Store widget host surface | `.kry canonical` |
| `src/ui/text_area.kry` | Text Area widget host surface | `.kry canonical` |
| `src/ui/text_composition.kry` | Text Composition widget host surface | `.kry canonical` |
| `src/ui/text_context.kry` | Text Context widget host surface | `.kry canonical` |
| `src/ui/text_editor.kry` | Text Editor widget host surface | `.kry canonical` |
| `src/ui/text_field.kry` | Text Field widget host surface | `.kry canonical` |
| `src/ui/text_focus.kry` | Text Focus widget host surface | `.kry canonical` |
| `src/ui/text_layout.kry` | Text Layout widget host surface | `.kry canonical` |
| `src/ui/text_navigation.kry` | Text Navigation widget host surface | `.kry canonical` |
| `src/ui/text_rows.kry` | Text Rows widget host surface | `.kry canonical` |
| `src/ui/text_selection.kry` | Text Selection widget host surface | `.kry canonical` |
| `src/ui/text_state.kry` | Text State widget host surface | `.kry canonical` |
| `src/ui/text_surface.kry` | Text Surface widget host surface | `.kry canonical` |
| `src/ui/title_bar.kry` | Title Bar widget host surface | `.kry canonical` |
| `src/ui/toast.kry` | Toast widget host surface | `.kry canonical` |
| `src/ui/toggle.kry` | Toggle widget host surface | `.kry canonical` |
| `src/ui/toolbar.kry` | Toolbar widget host surface | `.kry canonical` |
| `src/ui/tree_disabled.kry` | Tree Disabled widget host surface | `.kry canonical` |
| `src/ui/tree_frame.kry` | Tree Frame widget host surface | `.kry canonical` |
| `src/ui/tree_input.kry` | Tree Input widget host surface | `.kry canonical` |
| `src/ui/tree_layout.kry` | Tree Layout widget host surface | `.kry canonical` |
| `src/ui/tree_paint.kry` | Tree Paint widget host surface | `.kry canonical` |
| `src/ui/tree_view.kry` | Tree View widget host surface | `.kry canonical` |
| `src/ui/ui_layout.kry` | Ui Layout widget host surface | `.kry canonical` |
| `src/ui/ui_scaling.kry` | Ui Scaling widget host surface | `.kry canonical` |
| `src/ui/ui_transition.kry` | Ui Transition widget host surface | `.kry canonical` |
| `src/ui/widget_input.kry` | Widget Input widget host surface | `.kry canonical` |
| `src/ui/app_shell_layout.kry` | App-shell route layout calculation (replaces `src/core/app_shell.c`) | `.kry canonical` |
| `src/ui/route.zi` | App-shell route operations with portable slices | checked Ziran |
| `src/ui/capability_layout.kry` | Capability naming and viewport safe-content geometry (replaces `src/core/kry_capabilities.c`) | `.kry canonical` |
| `src/ui/frame_lifecycle.kry` | Frame lifecycle policy (replaces `src/core/kryon_frame.c`) | `.kry canonical` |
| `src/ui/frame_pacing.kry` | Frame pacing policy (replaces `src/core/kryon_frame_pacing.c`) | `.kry canonical` |
| `src/ui/kryon_test.kry` | Test and inspection helper surface | `.kry support` |
| `src/ui/locale_defaults.kry` | Built-in theme-control fallback labels | `.kry canonical` |
| `src/ui/locale_parser.zi` | Portable catalog entry and language-list parsing into caller owned spans | checked Ziran |
| `src/ui/locale_policy.zi` | Portable locale-code selection, preferred-language matching, and catalog fallback | checked Ziran |
| `src/ui/node.kry` | Retained node initializer and property-value constructors (replaces `src/core/kryon_node.c`) | `.kry canonical` |
| `src/ui/preference_policy.kry` | Theme and orientation preference decisions (replaces `src/core/device_preferences.c`) | `.kry canonical` |
| `src/ui/screen_routes.kry` | App screen routing and callback dispatch (replaces `src/core/app_runtime.c`) | `.kry canonical` |
| `src/ui/theme_catalog.kry` | Theme catalog host policy | `.kry canonical` |
| `src/ui/theme_color.kry` | Theme color resolution host policy | `.kry canonical` |
| `src/ui/theme_identity.kry` | Theme identity host policy | `.kry canonical` |
| `src/ui/theme_state.kry` | Active theme-selection state (replaces `src/core/theme.c`) | `.kry canonical` |
| `src/ui/window_policy.kry` | Window placement, drag threshold, and grabbable-strip bounds | `.kry canonical` |

## Current Implementation Audit

This is the current migration truth, not the desired final state. A name in the
registry is only considered `.kry`-backed when its reusable behavior, props, or
layout policy lives in `runtime/*.kry` and native code only adapts host input,
text measurement, painting, storage, or platform services.

| Group | `.kry`-backed today | Still native-only or compatibility |
|---|---|---|
| Text and drawing | `Text` style resolution, centered row text placement, selectable range normalization/pointer/drag/copy/show decisions/highlight geometry/double-click line-selection policy, `Paragraph` metrics/default line-gap/layout spacing/line-step/height/line-stride/alignment/selectable line-index/local-offset policy, `ParagraphSpec` generated data, `Background`/`Box`/`Line`/`Circle`/`Ring`/`Triangle` geometry policy, `Bevel` line geometry, `Icon` bounds/size policy, `Image` canonical props/name and placeholder layout, clean drawing primitive names (`Box`, `Circle`, `Ring`, `Triangle`) | icon sheet/drawing host support, paragraph parsing/line storage/drawing, selectable text ownership/drawing |
| Actions | `Button`, `Card`, `Link`, `Button` menu/split/arrow/info options; button fallback/terminal paint constants | helper button variants belong in `ButtonProps` or composition; invisible hit testing and rasterization are host support |
| Inputs | `Checkbox` paint/row/text/value/flag policy, `Dropdown` option/index normalization, popup/row/scrollbar/keyboard-intent/navigation/indicator policy, `DropdownOption`, `Drag` component layout/text paint/value policy, `Input` step-button default, component/step-button layout, value, temp-edit activation, and generic pointer activation/consume policy, `Progress`, `Radio`, `SegmentedControl` layout/selection policy, `Selectable`, `Slider` component/editor/hit layout, text paint geometry, and value/keyboard policy, `Spinbox` button-width/layout/button-input/value policy, `TextField`/`TextArea` defaults/metrics/paint geometry/buffer-limit/cursor normalization/navigation/edit intent/selection state/paint-span/double-click/pan/focus/text-buffer/range decision policy, `Toggle`, `Button` swatch props, `ColorPicker` layout/swatch/color policy | text composition, raw string storage/memmove/scanning, and platform text services |
| Layout | `Column`/`Row`/`Stack` content and child placement policy, `Group` bounds/content policy, `Screen` viewport fallback bounds policy, `Grid`, `Fieldset` layout policy, `PanedView` split/layout/change geometry and drag lifecycle policy, `Collapsible` header geometry plus pointer/close/keyboard/open-state decisions, `Separator`, `Scroll` measurement/sizing/wheel/content-drag/scrollbar-drag/release decision/thumb-drag/ensure-visible/clip geometry policy, shared `Surface`/`Style`/`Material` policy, `Reorder` metrics/handle geometry/placeholder paint geometry/target-index/lifecycle/release gate/result policy, `ReorderState`/`ReorderItem`/`ReorderList`/`ReorderListResult` generated support records | list/table begin-end wrappers remain host support; `src/ui/scroll.kry` and `src/ui/reorder.kry` own scroll and reorder pointer state and drawing |
| Collections | `Canvas` transform/hit-test policy, `CanvasGrid`, drag/drop source/target lifecycle decision policy, `ListBox` layout/navigation/row paint geometry/multi-selection row activation policy, `Plot` geometry/mode/text policy, `TreeView` row/window/paint geometry, marker text, and row-selection decision policy, `TableView` layout/scroll/scrollbar/cell geometry/header-angle normalization, header/row hot/pointer decisions, keyboard selection, activation, clear-selection, resize start/drag/clear/width lifecycle, and clipboard intent policy | drag/drop payload storage |
| Navigation | `NavigationBar` default-height variant, item interaction, paint/config layout/count/default policy, `TabBar` sizing/scroll/keyboard intent/keyboard-index/reorder marker/drag lifecycle/release-consumption/double-click decision policy, `Toolbar`, bottom icon row, and icon slider popup metrics/geometry/open/close policy, `TitleBar` effective state/layout/reservation/paint geometry policy, `Menu` geometry/keyboard navigation plus bar/context outside-close and group pointer open/close decision policy, `MenuItem`/`MenuGroup`/`MenuResult` data | retained menu open/focus/input state, router/link helpers |
| Overlays | `Popup` mode/input/open-state/tooltip visibility/outside-dismissal/Escape-close policy, internal dismissible-overlay viewport/dismissal/release-consumption policy, `Focus` ring geometry and keyboard activation policy, `Guide` overlay layout/arrow/step/keyboard-input policy, guide pager layout/page/keyboard-input policy, swipe begin/drag/release/lifecycle decision policy, `SwipeGesture`/`SwipeSpec`/`SwipeResult` generated pager support records, `Modal` layout/frame/outside-dismissal/release-consumption/prompt availability/focus fallback/prompt-input/result/action policy, `Toast` style facts/request/render decision, duration/layout/text-placement/truncation policy, transition fade alpha/easing policy, `StylePicker` public props and option/selection/dropdown state policy, profile header geometry/text placement/pointer activation/click policy, profile image picker geometry/selection policy, inspector edit/resize geometry, handle-size, and release-consume policy | theme picker, inspector state/input, and profile image rendering/input host support; swipe pointer ownership storage remains host support |
| Game2D | `Scene`, `Node2D`, `Camera2D`, `Sprite2D`, `AnimatedSprite2D`, `TileMap`, `CollisionShape2D`, `Area2D`, `Body2D`, `AnimationPlayer`, `AudioSource`, and `Light2D` public props/enums/defaults; `NodeKind*` and `NodeFlag*` support values | Scene ownership, lifecycle, physics/audio handles, rendering, and runtime node mutation remain native Game2D support. |

The remaining migration target is the native support around text editing and
content wrappers: `TextField` and `TextArea` own defaults/metrics/wrap/caret
stroke/paint geometry/buffer-limit/cursor normalization/navigation/edit-intent, focus/platform
text-input sync, text-buffer mutation/range/bracket decisions, and selection
range/movement/collapse/select-all and selection paint-span policy in `.kry`,
with selection ownership and painting in `.kry`. Platform IME, clipboard,
font rasterization, and glyph measurement remain backend services.

## Release and keyboard ownership audit (2026-09-14)

`ConsumeRelease()` calls in maintained widget implementations apply generated
policy flags. The `ConsumeRelease` definition in `src/ui/input_capture.kry` stores the
frame's consumed state; it does not decide which widget should consume input.
The nine generic `InputPointerInteractionFor` calls use `runtime/input.kry`.

| Native files | Policy owner | Native responsibility |
|---|---|---|
| `src/ui/button.kry`, `src/ui/dropdown.kry`, `src/ui/slider.kry`, `src/ui/drag.kry`, `src/ui/numeric_edit.kry`, `src/ui/toggle.kry`, `src/ui/checkbox.kry`, generic helpers in `input_capture.kry` | `input.kry`, with dropdown/value policy | Sample input, apply activation/consume flags, draw/store results |
| `src/ui/modal.kry`, `src/ui/popup.kry`, `src/ui/overlay.kry` | `modal.kry`, `overlay.kry`, `popup_policy.kry` | Dialog, popup, and dismissible overlay behavior is authored in `.kry` |
| `src/ui/navigation_bar.kry`, `src/ui/profile_header.kry` | `navigation_bar.kry`, `profile_header.kry` | Navigation and profile input, image selection, and drawing are authored in `.kry` |
| `src/ui/swipe.kry`, `src/ui/tab_bar.kry`, `src/ui/tab_store.kry`, `ui_inspect.c` | `swipe.kry`, `tab_bar.kry`, `inspect.kry` | Swipe and TabBar input, drag state, and drawing are authored in `.kry`; inspector work remains in C |
| Scrollbar and ScrollScope in `src/ui/scroll.kry` | `scroll.kry` | Store scroll/drag state, apply release, clip content, and paint the scrollbar |
| Collection and Menu paths in `src/ui/table_view.kry` and `src/ui/menu_host.kry` | `drag_drop.kry`, `list_box_multi.kry`, `menu.kry`, `list_box.kry`, `tree_view.kry`, `table_view.kry`, `collapsible.kry` | Apply source/target, row, menu, resize and header decision flags |

Immediate menu Escape and context-trigger dismissal suppression now come from
`menu.kry`. Collapsible arrow priority comes from `collapsible.kry`; the Go
host also applies its keyboard decision record. Immediate C text widgets and
buffer editing use `text_input.kry` shortcut facts, and Go text commands use
its edit-command decisions. Go menu Escape uses the shared menu decision.
The old Go desktop pointer fallback was removed; desktop input uses the current
mouse interface, with wheel sampling remaining a host capability.

This audit does not classify every surrounding branch as complete. Go/web host
runtimes still contain independent policy, and web block/composition fixtures
are not all executed by the shared parity runner. Public canonical names and
successful generation are not proof of equivalent behavior. The outstanding
inventory and web expression-placeholder removal are tracked in
`plan/canonical/README.md`.

## Registry Surface Audit

This table is the authoritative shared list of public node names from
`src/ui/node_registry.kry`. Keep one row per registry name so rename feedback
has a single place to land.

| Public name | Registry group | Detail | Runtime `.kry` source | State | Migration note |
|---|---|---|---|---|---|
| `Background` | `Display` | Fill | `runtime/primitive.kry` | `.kry-backed` | Viewport bounds and app fallback policy are `.kry`; host keeps immediate fill drawing and retained paint ordering. |
| `Text` | `Display` | Label | `runtime/text.kry`, `runtime/text_input.kry` | `.kry-backed` | Keep one `Text(TextProps)` surface; retained tree typography uses resolved KSS font sizes directly; selectable range normalization uses shared `.kry` text-input policy, and selectable pointer/drag/copy/show/highlight/double-click line-selection policy is `.kry`. |
| `Paragraph` | `Display` | Rich text | `runtime/paragraph.kry`, `runtime/drawing_props.kry`, `runtime/text.kry`, `runtime/text_input.kry` | Partly `.kry-backed` | Metrics/default line-gap, layout spacing, line-step, height, line-stride, alignment, selectable line-index/local-offset, text-selection pointer/drag/copy/show decisions and double-click line-selection policy, `ParagraphSpec` data, and selectable range normalization are `.kry`; text parsing, line-break array ownership, icon shaping, selection ownership/drawing, and drawing remain host support. |
| `Box` | `Display` | Shape | `runtime/primitive.kry` | `.kry-backed` | Rectangle bounds policy is `.kry`; host keeps fill/border drawing. |
| `Line` | `Display` | Stroke | `runtime/primitive.kry` | `.kry-backed` | Endpoint and retained-bounds policy is `.kry`; host keeps stroke drawing. |
| `Bevel` | `Display` | Relief | `runtime/bevel.kry`, `src/ui/bevel.kry` | `.kry canonical` | Line geometry and drawing are `.kry`; still review whether it should fold into `Surface`/material props. |
| `Icon` | `Display` | Icon | `runtime/icon.kry` | Partly `.kry-backed` | Bounds/size policy is `.kry`; icon sheet/type lookup and drawing remain host support. |
| `Image` | `Display` | Image | `runtime/image.kry` | Partly `.kry-backed` | Fit and placeholder layout policy are `.kry`; placeholder typography uses resolved KSS font sizes directly; cache/loading/drawing remain host support. |
| `Card` | `Input` | Surface action | `runtime/card.kry`, `runtime/card_props.kry` | `.kry-backed` | Card composition and props live in `.kry`. |
| `Button` | `Input` | Action | `runtime/button.kry`, `runtime/button_props.kry` | `.kry-backed` | Single button surface; menu/split/info/icon variants are props/composition; retained and immediate typography defaults plus fallback/terminal paint policy are `.kry`/KSS-owned. |
| `Link` | `Input` | Link | `runtime/link.kry`, `src/ui/link.kry` | `.kry canonical` | Bounds, interaction, activation, styling, inspector scope, and drawing are `.kry`; URL dispatch remains a platform service. |
| `TextField` | `Input` | Input | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics, scroll, paint geometry, buffer-limit, cursor normalization, navigation, edit intent, keyboard edit-command decisions applied by both immediate and retained trees, double-click/pan/focus decisions, text-buffer mutation/range/bracket policy, and selection range/movement/collapse/select-all/paint-span policy are `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership, and paint still native. |
| `Dropdown` | `Input` | Selection | `runtime/dropdown.kry`, `runtime/dropdown_props.kry`, `src/ui/dropdown.kry`, `src/ui/dropdown_store.kry` | `.kry canonical` | Option data, KSS styling, trigger and menu paint, input, scrolling, keyboard navigation, retained state, and dismissal are authored in `.kry`. |
| `Slider` | `Input` | Value | `runtime/slider.kry` | `.kry-backed` | Value type, orientation, angle/unit, component/editor/hit layout, and text paint geometry are props/policy; label/value typography is KSS-owned. |
| `Toggle` | `Input` | On/off | `runtime/toggle.kry` | `.kry-backed` | Host samples input and draws; paint/layout and value policy are `.kry`. |
| `Checkbox` | `Input` | Boolean | `runtime/checkbox.kry`, `src/ui/checkbox.kry` | `.kry-backed` | Input, style, drawing, and value/flag toggles are authored in `.kry`; box, mark, and label roles are KSS-owned. |
| `Radio` | `Input` | Choice | `runtime/radio.kry`, `src/ui/radio.kry` | `.kry-backed` | Paint, input, animation, and drawing are authored in `.kry`; host retains per-window state in `src/ui/widget_store.kry`. |
| `Progress` | `Input` | Progress | `runtime/progress.kry`, `src/ui/progress.kry` | `.kry-backed` | Layout, style, and drawing are authored in `.kry`. |
| `Spinbox` | `Input` | Number | `runtime/spinbox.kry`, `src/ui/spinbox.kry` | `.kry-backed` | Layout, button actions, value formatting, stepping, and drawing are authored in `.kry`. |
| `ColorPicker` | `Input` | Color | `runtime/color_picker.kry`, `src/ui/color_picker.kry` | `.kry-backed` | Channel layout, color conversion, swatch drawing, and slider composition are authored in `.kry`. |
| `SegmentedControl` | `Input` | Segments | `runtime/segmented_control.kry` | `.kry-backed` | Layout, gap, font fallback, wrapping, segment sizing, and selection policy are `.kry`; host keeps label measurement, input sampling, state storage, and button drawing. |
| `Group` | `Layout` | Container | `runtime/group.kry` | `.kry-backed` | Canonical non-layout grouping scope; bounds/content policy is `.kry`, host keeps retained tree scope ownership. |
| `Separator` | `Layout` | Divider | `runtime/separator.kry`, `src/ui/separator.kry` | `.kry-backed` | Line, label, bullet style, and rendering are authored in `.kry`. |
| `Fieldset` | `Layout` | Frame | `runtime/fieldset.kry` | `.kry-backed` | Titled group and border policy are `.kry`. |
| `PanedView` | `Layout` | Split panes | `runtime/paned_view.kry`, `src/ui/paned_view.kry` | `.kry canonical` | Split clamp, layout, handle geometry, pointer input, drag lifecycle, active split storage, and drawing are `.kry`. |
| `Collapsible` | `Layout` | Section | `runtime/collapsible.kry`, `src/ui/collapsible.kry` | `.kry canonical` | Header metrics, geometry, marker text, pointer/body toggle, close, keyboard open, tree focus routing, state storage, styling, and drawing are `.kry`/KSS-owned. |
| `ListBox` | `Collections` | List | `runtime/list_box.kry` | `.kry-backed` | Layout, keyboard navigation intent, and row paint geometry policy is `.kry`; host keeps input/scroll sampling. |
| `TreeView` | `Collections` | Tree | `runtime/tree_view.kry` | Partly `.kry-backed` | Row, indent, scroll-window, marker text, text bounds, paint geometry, and row-selection decision policy are `.kry`; item typography defaults are KSS-owned; host keeps input sampling, selected-id storage, expansion state, and drawing. |
| `TableView` | `Collections` | Table | `runtime/table_view.kry` | Partly `.kry-backed` | Header/body/frozen-row/scroll/scrollbar/cell geometry, header-angle normalization, header/row hot/pointer decisions, keyboard selection intent, activation, clear-selection, resize start/drag/clear/width lifecycle, and clipboard intent policy are `.kry`; host keeps column ordering, input sampling, stored selection pointers, resize pointer ownership, clipboard IO, and drawing. |
| `TextArea` | `Collections` | Text area | `runtime/text_input.kry` | Partly `.kry-backed` | Metrics, page-navigation rows, paint geometry, buffer-limit, cursor normalization, navigation, edit intent, double-click/pan/focus decisions, text-buffer mutation/range/bracket policy, and selection range/movement/collapse/select-all/paint-span policy are `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership, and paint still native. |
| `CanvasGrid` | `Collections` | Grid | `runtime/canvas_grid.kry`, `src/ui/canvas.kry` | `.kry-backed` | Grid spacing, line geometry, drawing, and hit testing are authored in `.kry`. |
| `Menu` | `Navigation` | Menu | `runtime/menu.kry`, `runtime/menu_props.kry` | `.kry canonical` | Command menu surface; item/group/result data plus bar, popup, context, and outside-close behavior props are generated from `.kry`. |
| `NavigationBar` | `Navigation` | Tabs | `runtime/navigation_bar.kry`, `src/ui/navigation_bar.kry` | `.kry canonical` | Item interaction, KSS styling, paint, and configuration modal composition are `.kry`. |
| `Toolbar` | `Navigation` | Tools | `runtime/toolbar.kry` | `.kry-backed` | Metrics/geometry/style-size and icon slider popup open/close policy are `.kry`; host dispatches child actions. |
| `TabBar` | `Navigation` | Tabs | `runtime/tab_bar.kry`, `src/ui/tab_bar.kry`, `src/ui/tab_store.kry` | `.kry canonical` | Sizing, KSS style, input, scroll, retained drag state, icon and text paint, reorder, close, and double click are authored in `.kry`. |
| `TitleBar` | `Navigation` | Title | `runtime/title_bar.kry`, `src/ui/title_bar.kry` | `.kry-backed` | TitleBar layout, styling, title text, dropdown dispatch, and leading action are authored in `.kry`; text measurement and drawing use lower-level runtime services. |
| `Focus` | `Overlays` | Focus | `runtime/focus.kry`, `src/ui/focus.kry` | `.kry-backed` | State, traversal, registration, keyboard activation, and ring drawing are in `.kry`; backend supplies key and pointer samples. |
| `Modal` | `Overlays` | Dialog | `runtime/modal.kry`, `src/ui/modal.kry` | `.kry canonical` | Layout, frame geometry, dismissal, prompt and action behavior, capture, styling, and drawing are `.kry`; text editing calls the shared text input surface. |
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
| `SegmentedControl` | `.kry canonical` | Segmented choice control; layout/wrapping/selection policy is in `.kry`, generated Go uses `kr.SegmentedControl`. |
| `Slider` | `.kry canonical` | Type/orientation/angle variants, component/editor/hit layout, and text paint geometry are props/policy; label/value typography is KSS-owned. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props. `MenuItem`, `MenuGroup`, and `MenuResult` are generated data/result records, not separate widgets. KSS uses `Menu`, `MenuItem`, and `MenuSeparator`; item typography participates in popup sizing; no `MenuBar` selector. |
| `Toggle` | `.kry canonical` | Boolean switch. |
| `Checkbox` | `.kry canonical` | Boolean checkbox. |
| `Radio` | `.kry canonical` | Choice control. |
| `Progress` | `.kry canonical` | One progress concept. |
| `Plot` | `.kry canonical` | Public props and mode names live in `runtime/plot_props.kry`; plot geometry, text, style, and drawing live in `.kry`. |
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
| `TableView` | `.kry canonical` | Table layout, scroll, scrollbar, cell geometry, header-angle normalization, header/row hot/pointer decisions, keyboard selection, activation, clear-selection, resize start/drag/clear/width lifecycle, and clipboard intent policy in `.kry`; host keeps state/input. |
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
`include/ui_tree_props.generated.h`. Parser lowering and native tests use internal declarations
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
| `Text` | `.kry canonical` | Public props live in `runtime/text_props.kry`; single canonical signature is `Text(TextProps)`. Selectable pointer/drag/copy/show decisions, highlight geometry, and double-click line-selection policy are in `.kry`. |
| `Paragraph` | `.kry canonical` | Metrics/default line-gap/layout spacing/height/alignment/selectable line-index/local-offset/text-selection pointer/drag/copy/show/double-click line-selection policy is in `.kry`; host keeps rich text parsing, line-break storage, icon shaping, and drawing. |
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
| `Link` | `.kry canonical` | Canonical public name for URL/link activation; `src/ui/link.kry` owns the widget, using `runtime/link.kry` interaction policy and KSS typography. URL dispatch remains a platform service. |
| `TextField` | `.kry canonical` | Metrics, horizontal scroll, paint geometry, buffer-limit, navigation, selection state, double-click/pan/focus decisions, platform text-input sync, text-buffer mutation/range/bracket policy, and edit intent policy are in `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership/painting, and rendering remain native host support. |
| `TextArea` | `.kry canonical` | Metrics, page-navigation rows, paint geometry, buffer-limit, navigation, selection state, double-click/pan/focus decisions, platform text-input sync, text-buffer mutation/range/bracket policy, and edit intent policy are in `.kry`; raw string storage/memmove/scanning, IME, pointer history/ownership, selection ownership/painting, and rendering remain native host support. |
| `Dropdown` | `.kry canonical` | `src/ui/dropdown.kry` owns trigger and menu input, KSS paint, scrolling, navigation, and dismissal; `src/ui/dropdown_store.kry` owns retained state and option strings. |
| `Slider` | `.kry canonical` | Public props live in `runtime/slider_props.kry`; value type, orientation, angle/unit, component/editor/hit layout, and text paint geometry live in `SliderProps`/`.kry`; generated Go uses `kr.Slider`. |
| `Drag` | `.kry canonical` | Public props live in `runtime/drag_props.kry`; value type, range mode, typed keyboard input, component layout, and text paint geometry live in `DragProps`/`.kry`; generated Go uses `kr.Drag`. |
| `Input` | `.kry canonical` | Public props live in `runtime/input_props.kry`; value type, values, component/step-button layout, step policy, and temp-edit activation live in `.kry`; generated Go uses `kr.Input`; embedded editing uses `TextField` typography and step controls use `Button` typography. |
| `Spinbox` | `.kry canonical` | Public props live in `runtime/spinbox_props.kry`; layout, button actions, value formatting, stepping, and drawing are authored in `.kry`. |
| `Toggle` | `.kry canonical` | Public props live in `runtime/toggle_props.kry`; paint/layout and value policy are in `.kry`, label typography is KSS-owned, host samples input and draws. |
| `Checkbox` | `.kry canonical` | Public props live in `runtime/checkbox_props.kry`; paint, input, drawing, row/text layout, and value/flag toggle policy are authored in `.kry`. |
| `Radio` | `.kry canonical` | Public props live in `runtime/radio_props.kry`; paint, layout, input, activation, animation, and drawing are authored in `.kry`. |
| `Selectable` | `.kry canonical` | Public props live in `runtime/selectable_props.kry`; paint, input, and toggle are authored in `.kry`; review whether list item props should absorb it later. |
| `Progress` | `.kry canonical` | Public props live in `runtime/progress_props.kry`; prefer one public progress name. |
| `ColorPicker` | `.kry canonical` | Public props live in `runtime/color_picker_props.kry`; channel layout, swatch paint geometry, and color conversion are in `.kry`; swatch activation is `Button` with swatch props. |
| `SegmentedControl` | `.kry canonical` | Layout and selection policy are in `.kry`; segment typography and paint are KSS-owned; host handles label measurement, focus/input, state storage, and button drawing. |
| `LabelTextField` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `TextField`. |
| `CheckboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Checkbox`. |
| `SpinboxRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Text` and `Spinbox`. |
| `ButtonRow` | Removed | Removed from public headers; internal row helper only. Public code should compose `Row` with `Button` children. |
| `SectionLabel` | Removed | Removed from public headers; internal row helper only. Public code should use `Text`/`Heading` props or `.kry` composition. |
| `InfoRows` | Internal support | Internal repeated label/value row helper in `src/ui/rows.kry`; typography uses resolved KSS font sizes directly; public forms should use `.kry` layout with `Row`/`Text`. |

## Layout And Containers

| Public name | Current decision | Notes |
|---|---|---|
| `Column` | `.kry canonical` | Content and child placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Row` | `.kry canonical` | Content and child placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Grid` | `.kry canonical` | Metrics, columns, and cursor placement policy are in `.kry`; host keeps retained tree scope ownership. |
| `Stack` | `.kry canonical` | Content/child fill policy is in `.kry`; host keeps retained tree scope ownership. |
| `Screen` | `.kry canonical` | Top-level screen container; viewport fallback bounds policy is in `.kry`. |
| `Group` | `.kry canonical` | Non-layout grouping scope. Bounds/content policy is in `.kry`; host keeps retained tree scope ownership. |
| `Separator` | `.kry canonical` | Public props live in `runtime/separator_props.kry`; line, label, and bullet layout, style, and drawing are authored in `.kry`; label typography is KSS-owned. |
| `Fieldset` | `.kry canonical` | Public props live in `runtime/fieldset_props.kry`; titled border group. |
| `PanedView` | `.kry canonical` | Public props live in `runtime/paned_view_props.kry`; `runtime/paned_view.kry` defines split and drag policy, while `src/ui/paned_view.kry` owns pointer input, active split storage, styling, and drawing. |
| `Collapsible` | `.kry canonical` | Public props live in `runtime/collapsible_props.kry`; `runtime/collapsible.kry` owns policy, and `src/ui/collapsible.kry` owns input sampling, focus, state storage, styling, and drawing. |
| `Scroll` | `.kry canonical` | Public props live in `runtime/scroll_props.kry`; lexical scroll-content block. `runtime/scroll.kry` owns portable policy, while `src/ui/scroll.kry` owns pointer state, clipping, scrollbar styling and drawing, and page scaffolds. The platform bridge supplies raw Android gestures. |
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
| `ListBox` | `.kry canonical` | `src/ui/list_box.kry` owns single selection input, scroll, styling, and paint around `runtime/list_box.kry` policy. Multi-selection uses `selected`, `selected_count`, and `anchor` props; its corresponding behavior lives in `src/ui/list_box_multi.kry`. Item typography is KSS-owned; KSS styles multi-select mode with `ListBoxMulti` and `ListBoxMultiItem`. |
| `TreeView` | `.kry canonical` | `src/ui/tree_view.kry` owns input sampling, scrolling, styling, selection, and drawing around row/window, marker, and paint geometry policy in `runtime/tree_view.kry`. Item typography defaults are KSS-owned. |
| `TableView` | `.kry canonical` | Header/body/frozen-row/scroll/scrollbar/cell geometry, header-angle normalization, header/row hot/pointer decisions, keyboard selection, activation, clear-selection, resize start/drag/clear/width lifecycle, and clipboard intent policy are in `.kry`; header, cell, and selection text typography is KSS-owned, including native fallback sizing; host handles column ordering, input sampling, stored selection pointers, resize pointer ownership, clipboard IO, and drawing. |
| `CanvasGrid` | `.kry canonical` | Grid spacing, line counts, and line rectangles are in `.kry`; host handles drawing. |
| `Canvas` | `.kry canonical` | Transform, hit-test, and result policy are in `.kry`; host keeps clip/camera renderer scope. |
| `DragDrop` | `.kry canonical` | Typed drag/drop interaction concept. Source and target roles belong in props or composition; source/target lifecycle decision policy is in `.kry`; host keeps payload storage, type comparison, and pointer ownership. |

## Navigation

| Public name | Current decision | Notes |
|---|---|---|
| `NavigationBar` | `.kry canonical` | `src/ui/navigation_bar.kry` owns input, item paint, icons, and configuration modal composition around `runtime/navigation_bar.kry` policy. Item and configuration-slot labels use `NavigationBarItem` KSS typography. |
| `Toolbar` | `.kry canonical` | Metrics, geometry, icon style-size, and icon slider popup open/close policy are in `.kry`; host handles input, drawing, state storage, and child `Button`/`Dropdown` calls. |
| `Menu` | `.kry canonical` | Command menu surface; bar, popup, and context behavior are selected by props; metrics, selectable/keyboard navigation, bar/context open/index policy, and group pointer open/close decisions are in `.kry`. |
| `TabBar` | `.kry canonical` | `src/ui/tab_bar.kry` owns KSS style, sizing, input, scroll, drag, close, and paint; `src/ui/tab_store.kry` owns retained state. |
| `TitleBar` | `.kry canonical` | Effective height/state, layout, and paint geometry policy are in `.kry`; title typography uses resolved KSS font sizes directly; leading action and dropdown behavior live in `TitleBarProps`. |
| `Router` | checked Ziran | Caller owned routes and state; submits an inert retained node; returns a URL effect for the host to apply. |
| `Link` | `.kry canonical` | Canonical navigation/link widget. |

## Terminal Support

Terminal support is reusable runtime infrastructure, not a general UI widget
family. Keep app-specific terminal product UX outside Kryon; keep reusable pane
metrics and protocol support here.

| Public name | Current decision | Notes |
|---|---|---|

## Overlays And Feedback

| Public name | Current decision | Notes |
|---|---|---|
| `Popup` | `.kry canonical` | Arbitrary anchored/floating content. `src/ui/popup.kry` owns pointer sampling, paint layers, clipping, release consumption, and child scope around `runtime/popup_policy.kry` lifecycle decisions. |
| `Modal` | `.kry canonical` | `src/ui/modal.kry` owns dialog input, inspector scope, capture, styling, prompt placement, actions, and drawing around `runtime/modal.kry` policy. Title, message, and action typography is KSS-owned; prompt fields use `TextField` typography. |
| `Toast` | `.kry canonical` | Public toast feedback surface. Style facts, request/render clear decisions, duration/deadline, layout, text-placement, and truncation policy are in `.kry`; host keeps message storage, clock source, text measurement, and drawing. |
| `Focus` | Partly `.kry-backed` | Focus ring geometry and keyboard activation policy are in `.kry`; focus state, registration, key sampling, popup capture lookup, and drawing remain host support. |
| `Guide` | `.kry canonical` | Guided overlay flow. The clean public API is one `Guide(GuideProps)` surface with step data in props; `GuideStep` is data, not a widget. `src/ui/guide.kry` owns input, styling, layout, and drawing around `runtime/guide.kry` policy. Label typography uses resolved KSS font sizes directly. |
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
| `WidgetKindParagraph` | `Paragraph` | `.kry canonical`; rich text metrics/default line-gap/layout spacing/height/alignment/selectable line-index/local-offset/text-selection pointer/drag/copy/show/double-click line-selection policy is `.kry-backed` |
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
| `WidgetKindIcon` | `Icon` | `.kry-backed`; retained node kind now matches the public icon concept |
| `WidgetKindInput` | `Input` | `.kry canonical`; shared numeric input composition and step policy live in `runtime/input.kry` |
| `WidgetKindSeparator` | `Separator` | `.kry canonical`; line/label/bullet policy lives in `runtime/separator.kry` |
| `WidgetKindDragDrop` | `DragDrop` | `.kry canonical`; source/target lifecycle policy lives in `runtime/drag_drop.kry` |
| `WidgetKindRadio` | `Radio` | `.kry canonical`; paint/layout/marker policy lives in `runtime/radio.kry` |
| `WidgetKindProgress` | `Progress` | `.kry canonical`; track/fill/label layout policy lives in `runtime/progress.kry` |
| `WidgetKindPlot` | `Plot` | `.kry canonical`; range/mark/text paint policy lives in `runtime/plot.kry` |
| `WidgetKindFocus` | `Focus` | `.kry-backed`; focus ring geometry lives in `runtime/focus.kry` |
| `WidgetKindSpinbox` | `Spinbox` | `.kry canonical`; step and layout policy lives in `runtime/spinbox.kry` |
| `WidgetKindFieldset` | `Fieldset` | `.kry canonical`; title/border paint policy lives in `runtime/fieldset.kry` |
| `WidgetKindListBox` | `ListBox` | `.kry canonical`; row/window plus multi-select row activation/selection policy lives in `runtime/list_box.kry` and `runtime/list_box_multi.kry` |
| `WidgetKindTreeView` | `TreeView` | `.kry canonical`; row/window/marker/selection paint policy lives in `runtime/tree_view.kry` |
| `WidgetKindTableView` | `TableView` | `.kry canonical`; table geometry, roles, pointer decisions, selection, activation, resize, clipboard intent, and scroll policy live in `runtime/table_view.kry` |
| `WidgetKindCanvasGrid` | `CanvasGrid` | `.kry canonical`; grid line policy lives in `runtime/canvas_grid.kry` |
| `WidgetKindPanedView` | `PanedView` | `.kry canonical`; split/handle/drag policy lives in `runtime/paned_view.kry` |
| `WidgetKindCollapsible` | `Collapsible` | `.kry canonical`; header/close/layout and input decision policy lives in `runtime/collapsible.kry` |
| `WidgetKindColorPicker` | `ColorPicker` | `.kry canonical`; channel/swatch policy lives in `runtime/color_picker.kry` |
| `WidgetKindModal` | `Modal` | `.kry canonical`; layout/frame/action policy lives in `runtime/modal.kry` |
| `WidgetKindToolbar` | `Toolbar` | `.kry canonical`; toolbar, bottom-row layout, and icon slider popup policy lives in `runtime/toolbar.kry` |
| `WidgetKindMenu` | `Menu` | `.kry canonical`; bar/popup/context metrics and navigation policy live in `runtime/menu.kry` |
| `WidgetKindSelectable` | `Selectable` | `.kry canonical`; paint/layout and toggle policy live in `runtime/selectable.kry` |
| `WidgetKindBullet` | `Bullet` | `.kry-backed`; bullet paint geometry lives in `runtime/separator.kry` |

### WidgetKindCustom Policy

`WidgetKindCustom` remains only as an internal retained-tree escape hatch for
host-only helper nodes, transient paint submissions, and future app-defined
experiments. Canonical widgets must use their own retained node kind instead of
lowering through this bucket.

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
   popup input records, tooltip visibility, outside-dismissal, and Escape-close policy into `runtime/popup_policy.kry` and label text-field row layout into
  `runtime/rows.kry`; SegmentedControl row-advance/wrap policy now routes through
  `runtime/segmented_control.kry`; InfoRows background/text/separator geometry and
  button-row wrap height/advance now also
  route through `runtime/rows.kry`; dropdown panel/option/scrollbar role
  policy, selected-option appearance, trigger/menu keyboard intent, open/dismiss/commit-close state, and menu transient pointer state now route through `runtime/dropdown.kry`;
  menu selectable-item, submenu activation, pointer item effects,
  row and bar keyboard input decisions, wraparound navigation, and bar open/index policy now route through
  `runtime/menu.kry`; group pointer open/close decisions also now route
  through `runtime/menu.kry`; ListBox row selection policy now routes through
  `runtime/list_box.kry`; ListBox multi-select keyboard input now routes
  through `runtime/list_box_multi.kry`; centered-column and page side-padding policy now route
  through `runtime/layout.kry`; reorder lifecycle and release gates now route through
  `runtime/reorder.kry`; swipe drag/release lifecycle effects now route through
  `runtime/swipe.kry`; drag/drop source/target lifecycle decisions now route
  through `runtime/drag_drop.kry`; scroll-page content-width normalization and
  scroll drag/scrollbar-drag/release/ensure-visible policy now route through
  `runtime/scroll.kry`;
  table keyboard selection, clear-selection, header sort cycling,
  header-angle normalization, row hot/click/context effects, resize start/drag/clear/width lifecycle, clipboard intent, and scroll-into-view policy now route through
  `runtime/table_view.kry`; `Input` numeric kind default format, integer
  rounding, step dispatch, temp-edit activation, shared pointer interaction for old immediate helpers, and
  pointer-drag threshold/start/direction policy now route through
  `runtime/input.kry`; retained
  and immediate `Drag`/`Slider`/`Spinbox` default numeric formats now also use
  that runtime policy; `Spinbox` child focus IDs and button-combination stepping now route through
  `runtime/spinbox.kry`; `Drag` typed keyboard input, text inset, label-gap metrics, component
  drag tokens, and retained pointer lifecycle now route through
  `runtime/drag.kry`; `Slider` component tokens,
  focus IDs, editor center placement, old immediate pointer lifecycle, and
  retained ratio pointer lifecycle now route through
  `runtime/slider.kry`; toolbar action focus IDs now route through
  `runtime/toolbar.kry`; `ColorPicker` channel focus IDs now route through
  `runtime/color_picker.kry`; immediate list/tree/table row wheel-step policy
  now routes through `runtime/scroll.kry`; numeric temp-edit double-click activation
  and selectable text pointer/drag/copy/show/double-click slop metrics, including retained text widget click slop,
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
   `text_rows.kry` now owns logical/visual rows, wrapping, heading metrics and
   caret affinity. Native adapters retain strings, font measurement and Unicode
   traversal. Real OS IME delivery/candidate windows remain platform work.
4. Finish rich text migration:
  `Paragraph` has `.kry` metrics/default line-gap/layout spacing/height/line-stride/alignment/selectable line-index/local-offset/text-selection pointer/drag/copy/show/double-click line-selection
  policy, retained selectable text block height/line advance and generated
  `ParagraphSpec` data. Token parsing and line-break ownership are shared through
  `ParagraphTokenNext` / `ParagraphLineAdvance`; Go inline icons use that policy.
  Font shaping, glyph/image resources and rendering stay native.
5. Audit host-owned input/state lifecycles:
   retained menu open/focus/input state, drag/drop payload storage, reorder and
   swipe pointer ownership storage, paned-view active split storage, tree/table stored selection
   mutation, table resize pointer ownership/clipboard IO, modal input capture, and toast message
   storage/clock source are still native support around `.kry` policy.
6. Finish lowered block backend cleanup:
   `Scroll`, `Popup`, `Disabled`, `TableCell`, `Canvas`, and composed content
   blocks are canonical `.kry` syntax. Lowered `Scroll` scope geometry, wheel,
   content-drag, thumb-drag, scrollbar-drag/release, and ensure-visible policy now route through
   `runtime/scroll.kry`. Canvas coordinate policy is shared and host scope
   snapshots restore parent cameras/clips. Compiler cleanup for named scope
   results executes on return, break and continue in C and Go. Native stacks
   remain required renderer/storage services.
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

The detailed native follow-up inventory and executable evidence are in
[`NATIVE_POLICY_OWNERSHIP.md`](NATIVE_POLICY_OWNERSHIP.md). Historical JS/web
implementation notes above do not make the paused target an active support claim.
