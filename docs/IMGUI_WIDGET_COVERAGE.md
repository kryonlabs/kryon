# Dear ImGui widget coverage

This audit compares Kryon's clean native widget surface with Dear ImGui
`imgui.h` at commit
[`96b6eb7728d298decdc939d4c698502220190050`](https://github.com/ocornut/imgui/commit/96b6eb7728d298decdc939d4c698502220190050)
(`IMGUI_VERSION_NUM` 19295, `1.93.0 WIP`). It tracks widget behavior rather
than copying overload names: Kryon's counted value arrays cover the ImGui
`*2`, `*3`, `*4`, `Scalar`, and `ScalarN` fronts without adding aliases.

“Covered” in this family inventory means a native implementation exists; it is
not proof of every overload, flag, interaction or rendering detail. Full-goal
completion requires those semantics to be checked separately. In particular,
the [composed popup checklist](COMPOSED_POPUP_IMPLEMENTATION.md) records the
remaining combo scope, presentation choices and required rendering evidence.

| Dear ImGui widget family | Kryon native surface | Status |
|---|---|---|
| Text and value helpers | `Text`, `TextColored`, `TextDisabled`, `TextWrapped`, `LabelText`, `BulletText`, `SeparatorText`, `Value*` | covered |
| Buttons and boolean choices | `Button`, `SmallButton`, `InvisibleButton`, `ArrowButton`, `Checkbox`, `CheckboxFlags`, `Radio`, `Bullet` | covered |
| Progress and links | `Progress`, `Href` | covered; `Href` represents both clickable text and open-URL links |
| Images | `Picture`, `ImageWithBg`, `ImageButton` | covered |
| Combo boxes | `Combobox`, `Dropdown`, `Selectable` | option-list helper implemented, including closure on disable or owner removal; arbitrary child composition is still missing |
| Drag values | `DragFloat`, `DragInt`, `DragFloatRange2`, `DragIntRange2` | covered, including counted N-component values |
| Sliders | `SliderFloat`, `SliderInt`, `VSliderFloat`, `VSliderInt`, `SliderAngle` | covered, including counted N-component values |
| Keyboard inputs | `TextField`, `TextArea`, `InputFloat`, `InputInt`, `InputDouble` | covered, including hints and counted N-component values |
| Color editors and pickers | `ColorEdit3`, `ColorEdit4`, `ColorPicker3`, `ColorPicker4`, `ColorButton` | covered |
| Trees and collapsing headers | `TreeView`, `Collapsible` | `Collapsible` supports tree styling, depth indentation, leaves, selected/disabled state, arbitrary nested children, keyboard expansion, and directional header/parent/child focus traversal |
| Selectables and multi-selection | `Selectable`, `MultiSelectList` | covered, including Ctrl/Shift range selection |
| List boxes | `ListBox`, `BeginListBox` / `EndListBox` | string-list helper and framed scrolling scope for arbitrary native children |
| Scrollable child content needed for composed lists and trees | `BeginScroll` / `EndScroll` | C/Go wheel scrolling, scrollbar dragging, and nested clipping implemented and exercised through generated native fixtures |
| Plots | `PlotLines`, `PlotHistogram` | covered |
| Menus | `MenuBar`, `PopupMenu`, `ContextMenu` | covered, including nested submenus |
| Tooltips and popups | `Tooltip`, `PopupMenu`, `ContextMenu`, modal/dialog widgets | covered through bounded native models |
| Tables | `TableView`, `BeginTableCell` / `EndTableCell` | row/cell model includes resizing, frozen rows, sorting, colors, visibility, ordering, slanted headers, and scoped native child widgets in custom-cell mode |
| Tabs | `TabBar`, `ClosableTabBar`, `TabItemButton` | covered |
| Drag and drop | `DragDropSource`, `DragDropTarget` | covered with typed copied payloads |
| Disabled content | `BeginDisabled`, `EndDisabled`, per-widget `Disabled` fields | covered, including nested scopes |

Dear ImGui layout calls such as `SameLine`, `Spacing`, `Indent`, and groups are
represented by Kryon's retained `Row`, `Column`, `Stack`, and `Grid` layout
containers. They are not separate widget aliases. Window management, style
stacks, input queries, logging, clipping, and item-status queries are support
APIs rather than widget families and are outside this widget inventory.

## Composition verification

C and Go regressions declare 41 combo controls and verify independent identity,
selection and open-state capture. C no longer aliases the first control after
24 dropdown identities; missing owners' records are reclaimed at frame end.
C options and their copied labels/font names also use dynamically sized owned
storage. Native tests select option 130, beyond the former 128-option cap.
The C rendering test observes the real text renderer receiving a full owned
512-byte overlay label after its caller buffer has been overwritten; Go checks
the corresponding deferred text record. Visible-row painting avoids drawing
every option in a large list. These changes do not resolve process-global C
ownership or arbitrary popup composition.

Open native C/Go dropdowns support Up/Down, Home/End and Enter, keeping the
highlight separate from committed selection. Native tests navigate a 131-option
list, confirm with Enter, cancel with Escape and reopen. The C test also selects
the last option by pointer after End, verifying that keyboard navigation reveals
it in the constrained viewport. Generated C/Go checks exercise Home/End and
confirmation without prematurely changing selection. Keyboard opening and
general popup keyboard-focus isolation remain gaps; these tests do not prove
complete ImGui navigation semantics.

`tests/parity/drag_drop.kry` is executed through generated C and Go. A clipped
source cannot activate, and a clipped target cannot consume the release before
an eligible target. Both runners mutate the source bytes after pressing and
check that the target receives the original copied payload. This native-only
fixture is not executed by the JavaScript runner. Go runtime tests additionally
exercise drag sources and targets beneath a popup, within its input scope, and
under disabled/clip restrictions, including rejection without losing the release
or payload. Active scalar-slider/resize drags and composed popup dismissal are
not established by these drag-and-drop tests.

`tests/parity/scroll_content.kry` is driven through generated C and Go in the
parity harness. It checks wheel-driven offset changes, visible child clicks,
rejection of clipped child clicks, and parent input restoration. The generated
Go path also checks pixels below the viewport and outside the parent of a nested
container. Both generated native paths exercise nested child clicks, thumb
dragging, and the bottom scroll limit. A second region combines a checkbox,
editable text field, and button: the native runners verify independent state,
text editing, rejection of hidden button clicks, and activation after scrolling.
This demonstrates mixed-content composition using the general scroll scope,
with selection and editing delegated to its child widgets. The mixed-content
region now uses `BeginListBox`/`EndListBox`. This fixture is not executed by the
JavaScript runner. The same native fixture opens nested tree-style `Collapsible`
headers, activates an arbitrary button child, closes the root, rejects clicks on
the hidden child, and reopens with the nested open state preserved. Callers own
child layout; `Depth` indents the header hit and drawing bounds only.
The native runners also exercise Left/Right expansion and Enter/Space toggling
while preserving nested state. Directional tests move between headers and their
parent/child, including a leaf and a disabled header that navigation must skip.
The native fixture also selects a combo popup row outside the owner's scroll
viewport while a previously drawn button occupies the same bounds. Go pixel
coverage verifies that the deferred popup paints above later content, without
inheriting the owner's scroll clip. This does not yet provide arbitrary combo
children. Native generated dismissal checks cover Escape and outside clicks,
unchanged selection, and restoration of underlying button interaction. Go also
tests closing when the option list becomes empty.
Generated C/Go tests also check that an earlier background scroll scope does
not change its offset on wheel input over an open combo popup. Native Go unit
tests cover the same ownership rule for scroll scopes, lists, trees, source
views and tables, including disabled and clipped content.
Arbitrary combo composition remains a separate gap as listed above.

Generated native custom-cell tests place a button and checkbox in adjacent
cells, verify independent state and no table-selection interception, and reject
an oversized button's spill into its neighboring cell. Native scope tests cover
reordered columns, frozen-row clipping, and disabled-state restoration. Cell
widgets use explicit cell bounds. A generated nested `Row` test verifies two
buttons inside a cell, clipping against the adjacent checkbox, and restoration
of a surrounding `Column` cursor. C retained nodes preserve the scope clip for
deferred hit testing and painting; explicitly positioned children do not advance
the parent layout cursor, matching Go.
An editable field in a partially visible cell is also exercised through both
generated native paths: click focus, cursor movement, text entry, disabling
without accepting text, and editing again after re-enabling. C retains disabled
scope state through deferred tree input and paint.

Table header rotation uses slanted cells. Native generated tests check the
empty header wedge, height-dependent column hit tests, separator resizing, and
body-row selection with an 80-pixel header; Go pixel coverage checks rotated
label paint and clipping to the cell polygon. C uses scanline scissors for
slanted glyph clipping, while Go clips its software rasterization directly.
`header_angle` is in degrees, clamped to -89 through 89, with
non-finite values treated as zero. `header_height` defaults to 30 and cannot
reduce the header below that height.
