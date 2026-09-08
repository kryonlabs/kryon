# Dear ImGui widget coverage

This audit compares Kryon's clean native widget surface with Dear ImGui
`imgui.h` at commit
[`334f484892a1fa881d2a927c2aff222c15458b8f`](https://github.com/ocornut/imgui/commit/334f484892a1fa881d2a927c2aff222c15458b8f)
(`IMGUI_VERSION_NUM` 19296, `1.93.0 WIP`). It tracks widget behavior rather
than copying overload names: Kryon's counted value arrays cover the ImGui
`*2`, `*3`, `*4`, `Scalar`, and `ScalarN` fronts without adding aliases.

“Covered” in this family inventory means a native implementation exists; it is
not proof of every overload, flag, interaction or rendering detail. Full-goal
completion requires those semantics to be checked separately. In particular,
the [composed popup checklist](COMPOSED_POPUP_IMPLEMENTATION.md) records the
implemented combo scope and its remaining lifecycle/backend gaps.

| Dear ImGui widget family | Kryon native surface | Status |
|---|---|---|
| Text and value helpers | `Text(TextProps)`, `LabelText`, `BulletText`, `SeparatorText`, `Value*` | one canonical text widget owns bounds, wrapping, clipping, color, alignment, and disabled presentation |
| Buttons and boolean choices | `Button`, `SmallButton`, `InvisibleButton`, `ArrowButton`, `Toggle`, `Checkbox`, `CheckboxFlags`, `Radio`, `Bullet` | interactive controls share pointer focus, Tab traversal, Enter/Space activation, disabled gating, popup ownership, and focus presentation; `Bullet` is presentation-only |
| Progress and links | `Progress`, `Href` | covered; `Href` represents both clickable text and open-URL links |
| Images | `Picture`, `ImageWithBg`, `ImageButton` | covered; `ImageButton` shares ordinary focus and keyboard activation |
| Combo boxes | `Combobox`, `Dropdown`, `Selectable`, `BeginCombo` / `EndCombo` / `CloseCombo` | option-list helper plus a native arbitrary-child scope with explicit close and presentation flags |
| Drag values | `DragFloat`, `DragInt`, `DragFloatRange2`, `DragIntRange2` | covered, including counted N-component values, component focus/Tab traversal, Left/Right adjustment, Home/End bounds, Shift/Alt step modifiers, and Ctrl-click/double-click temporary keyboard entry |
| Sliders | `SliderFloat`, `SliderInt`, `VSliderFloat`, `VSliderInt`, `SliderAngle` | counted N-component values, component focus, arrow/Home/End keyboard adjustment, slow/fast modifiers, and Ctrl-click/double-click temporary keyboard entry are covered |
| Keyboard inputs | `TextField`, `TextArea`, `InputFloat`, `InputInt`, `InputDouble` | covered, including hints and counted N-component values |
| Color editors and pickers | `ColorEdit3`, `ColorEdit4`, `ColorPicker3`, `ColorPicker4`, `ColorButton` | covered; `ColorButton` shares ordinary focus and keyboard activation |
| Trees and collapsing headers | `TreeView`, `Collapsible` | `Collapsible` supports tree styling, depth indentation, leaves, selected/disabled state, optional close/visibility state, arbitrary nested children, keyboard expansion, and directional header/parent/child focus traversal |
| Selectables and multi-selection | `Selectable`, `MultiSelectList` | covered, including shared focus/Enter/Space activation, Tab traversal, arrow/Home/End navigation, and Ctrl/Shift range selection |
| List boxes | `ListBox`, `BeginListBox` / `EndListBox` | string-list helper with focus, arrow/Home/End navigation and selection-following scroll, plus a framed scrolling scope for arbitrary native children |
| Scrollable child content needed for composed lists and trees | `BeginScroll` / `EndScroll` | C/Go wheel scrolling, scrollbar dragging, and nested clipping implemented and exercised through generated native fixtures |
| Plots | `PlotLines`, `PlotHistogram` | covered |
| Menus | `MenuBar`, `PopupMenu`, `ContextMenu` | nested submenus plus focus, disabled/separator skipping, arrow/Home/End traversal, submenu entry/backout, activation, and Escape dismissal in native C and Go |
| Tooltips and popups | `PopupMenu`, `ContextMenu`, modal/dialog widgets, `BeginPopup` / `EndPopup` / `ClosePopup` with `PopupTooltip`, `PopupModal`, and `PopupContext` | arbitrary popup, hover-tooltip, modal, and right-click context contents are native through one scope |
| Tables | `TableView`, `BeginTableCell` / `EndTableCell` | row/cell model includes resizing, frozen rows, sorting, colors, visibility, ordering, slanted headers, focus/arrow/Tab navigation, activation, clipboard copy/paste targets, and scoped native child widgets in custom-cell mode |
| Tabs | `TabBar`, `BeginTabBar` / `BeginTabItem` / `EndTabItem` / `EndTabBar`, `ClosableTabBar`, `TabItemButton` | canonical tab sizing/scrolling, focus, disabled skipping, keyboard selection/close, popup ownership, close/middle/double-click signals, selected-tab reveal, and reorder reporting are covered; the scope submits arbitrary native children only for the selected item, and omitted scroll state is owned independently by stable tab-bar ID |
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
every option in a large list. The option-list helper's retained state is owned
per render host; native C tests open the same numeric widget ID in independent
stores and verify that popup visibility cannot cross windows. Arbitrary popup
composition uses the separate public combo scope.

Open native C/Go dropdowns support Up/Down, Home/End and Enter, keeping the
highlight separate from committed selection. Native tests navigate a 131-option
list, confirm with Enter, cancel with Escape and reopen. The C test also selects
the last option by pointer after End, verifying that keyboard navigation reveals
it in the constrained viewport. Generated C/Go checks exercise Home/End and
confirmation without prematurely changing selection, opening the control with
Space through its focus ID. Native unit tests cover opening focused positive-ID
controls with Enter, keypad Enter, Space and Down, without moving or committing
the selection, and reject opening under both property and scope disabling.
Go also checks the focused paint record and focus border. Popup keyboard-focus
isolation is covered separately across buttons, editors, choices, drags, lists,
tables, menus, tabs, collapsibles and generic accelerators; these tests still do
not prove every ImGui navigation semantic or widget-specific shortcut.

Native C and Go sliders now register each counted component as a focus target.
Left/Right adjust horizontal sliders, Up/Down adjust vertical sliders, Home/End
select the range limits, Alt slows adjustment, and Shift accelerates it. Pointer
presses focus the exact component, disabled sliders reject keys, popup ownership
gates keyboard input, and the focused component receives the ordinary focus
presentation. Matching native tests cover component Tab traversal and the
pointer-to-keyboard transition; the generated `plots.kry` fixture verifies
horizontal float and vertical integer adjustment through k2c and k2go.
Ctrl-clicking or double-clicking a drag or slider component temporarily
replaces that component with the canonical text editor in native C and Go. Valid edits update live,
Enter returns to the numeric control, and typed values remain unclamped like
Dear ImGui's default temporary-input behavior. The generated `plots.kry`
fixture exercises Ctrl-click and double-click entry through k2c and k2go.

Native Go now constrains and flips long dropdowns using the same placement
rules as C, reusing its ordinary scroll container for clipping, wheel input and
scrollbar interaction. A generated C/Go regression opens a 20-option control
near the bottom of the window, navigates to End and selects its revealed last
row by pointer above the control. Go unit tests also exercise 131 options in
both popup directions, retained wheel position, visible-row-only paint records,
and dismissal cleanup. This is vertical viewport coverage, not proof of every
popup placement policy, touch gesture or rendering backend.
The generated native fixture also drags the scrollbar to the last row and
releases over another row without selecting it, then selects the revealed last
row with a fresh click. C dropdown scrollbar input now runs headlessly and is
not blocked by its own popup capture. Native unit tests dismiss an active thumb
drag via Escape, disabling, and owner removal, checking release of drag state.
The real C framebuffer regression also drags a popup thumb, verifies the text
renderer receives the last row immediately, and compares that frame pixel for
pixel with the following stationary-pointer frame. This catches delayed row
painting independently of the headless selection checks.
Native C/Go also constrain popup width and horizontal position to the window.
Unit tests exercise partially off-screen owners at either edge and owners wider
than the window, including capture bounds and row selection. Generated native
parity selects a popup row shifted left from a right-edge owner. These checks
do not cover multi-monitor work areas or arbitrary popup placement flags.

For composed-popup painting, the C framebuffer regression verifies translucent
immediate and retained capture and compares premultiplied composition against
direct source-over drawing. This manually configured backend test establishes
the required alpha pipeline. Captured retained nodes now preserve declaration-time
blend state and restore the caller's active and pending custom blend settings
after painting, verified by the framebuffer test. The public combo scope uses
the C paint-layer context, which owns and
composites textures with scope-level drawing-state restoration. Real pixel tests
cover mixed translucent/retained content, nested ordering, hidden parents and
missing owners over an existing target. Native `UIWindow` now owns layer frame,
composition and destruction. Main UI frames now own a separate context with
automatic frame composition and pre-graphics-shutdown cleanup.
Additional framebuffer checks interleave independent contexts with identical
owner IDs, resize one, destroy the other while a destination is active, and
verify the real X11 UIWindow presenter's owned-layer pixels. These prove paint
resource isolation. The UIWindow presenter regression additionally exercises
automatic context management across consecutive frames, a missing owner and
closing an active frame. It does not prove isolation of the still-global C
widget/input state or Win32 runtime behavior.
The main-host framebuffer regression also checks a missing owner, interleaved
auxiliary presentation and graphics-context close/reopen. Auxiliary texture
readback no longer changes the main frame's active destination.
C paint layers now isolate the retained layout path. Headless node/position
checks and real pixels verify a popup's independent Column origin and that the
surrounding Row resumes at its next slot. Unclosed child layouts are rejected.
The public combo scope uses this isolation so its child layouts do not consume
the surrounding layout cursor.
C layer disabled scopes now inherit the parent's state without allowing child
scope endings to unwind the parent. Headless nested-scope tests and the real
layer test cover restoration, with invalid-scope checks for an unclosed child.
C tab-bar owned scroll and its bar-local click, reorder, and pane-drag
bookkeeping now belong to that same render-host context instead of
process-global statics. Interleaved graphical hosts use the same tab ID without
sharing omitted scroll state, and restoring the outer host restores its state
immediately. Menu open state, submenu navigation, deferred overlays, and
activation results are host-owned as well; an interleaved-host regression uses
the same menu-bar ID without exposing one host's open menu to the other. Drag,
slider, numeric double-click, and copied drag/drop payload state share that
private host owner. A release in one host cannot observe another host's active
drag source, while restoring the source host exposes it immediately. Native
auxiliary windows also retain their own active focus ID
and restore the caller's focus on exit. A real-window regression reopens one
across frames and checks both directions of focus isolation. The broader C text-edit
state is still shared.
C layers also isolate input clips and scroll depth from the owner. Ordinary
button tests verify escaped clipping without bypassing modal capture, and real
pixels verify mixed content outside a 1x1 scrolling owner. Popup-to-background
and nested-popup input ownership now have a private C registry consulted by the
shared hit-test path. Ordinary button tests cover nested ownership, background
controls, dismissal, missing owners, context isolation and branch reordering.
Paint hosts now own and advance these registries automatically. The graphical
test checks capture before redeclaration, auxiliary-window isolation, parent
binding restoration and missing-owner retirement. Retained hit testing and
button hover/press state now use declaration-time popup ownership. Tests cover
child precedence over later parent/background nodes, dismissal restoring the
parent then background, pending-tree isolation and stale/destroyed registries.
Deferred pointer-focus registration also uses those snapshots; a test isolates
the deferred pass from immediate button focus and verifies modal blocking.
The same regression checks emitted click events: a blocked popup child emits
none, while an eligible child emits one. Deferred hit testing and hover/press
state share the complete capture predicate with focus registration.
Public composed-scope integration is covered. A newly opened top popup now
saves the displaced focus and gives focus to its first eligible child. Closing
a nested popup restores its parent's focus, while closing or omitting the root
owner restores the background focus. Active drag values, sliders, splitters and
table column resizing now retain the popup branch that started the gesture.
The same composed `Popup` scope now supports `PopupTooltip`: hover over an
explicit trigger submits arbitrary native child widgets into a non-input-capturing
overlay layer without an external `open` value. Matching C and Go unit tests
verify nested layout/child painting, disappearance outside the trigger and no
popup-input ownership. The generated C/Go composed fixture also verifies hover
visibility and that an underlying button still receives its click. The former
text-only `Tooltip` helper and `TooltipProps` surface were removed after all
maintained callers migrated, leaving one tooltip implementation.
`PopupModal` extends that same scope rather than adding a modal-specific child
API. It draws a full-view backdrop, keeps outside pointer input from reaching
background controls, preserves arbitrary nested children, and closes on Escape
or explicit caller action rather than an outside release. Matching native and
generated C/Go tests cover these semantics.
`PopupContext` extends the scope with a right-release trigger instead of adding
a context-only child API. The caller retains open state and stable panel bounds;
ordinary children, explicit close, outside dismissal, disabled rejection and
missing-owner cleanup remain shared with `Popup`. Matching native C/Go tests
and a generated k2c/k2cpp/k2go fixture cover the trigger and child lifecycle.
Matching C and Go tests now cover keyboard opening of a combo inside the top
popup versus a blocked parent, with popup bounds away from the combo to prove
the check is independent of pointer position. Child/branch dismissal restores
parent/background keyboard eligibility. This is not Tab trapping or general
button/editor keyboard isolation.
TextField and TextArea now gate editing by top-popup keyboard ownership in C
and Go. Native tests verify that typing leaves a focused parent editor unchanged
and edits an eligible child. C covers both immediate and deferred tree routing;
the immediate TextArea path now avoids renderer calls in headless tests.
All shortcuts and input replay across popup dismissal are not established by
these tests.

Generic native accelerators now have matching C and Go APIs and top-popup
ownership. Generated k2c/k2cpp/k2go coverage proves Ctrl+C dispatch inside the
active popup, suppression of the same background chord, and restoration after
the popup is explicitly closed. Widget-specific shortcut paths still require
their own ownership coverage. Popup menus apply that ownership to Escape as
well as navigation and activation, so an obscured menu cannot clear focus
belonging to the active popup branch.
Dropdown/Combobox stores the declaration's popup-owner snapshot for its
deferred overlay pass. Escape, arrows, Home/End, and Enter are ignored while a
newer child branch owns the keyboard, then work again as soon as that branch
closes.
Collapsible/tree-header arrows now have matching native and generated popup
ownership coverage, including background restoration after close.
Selectable-text copy is likewise ownership-gated in C and Go; native Go tests
cover both blocked background and eligible popup selection copies.
Table focus, display-order arrow and Tab/Shift+Tab navigation, Enter/F2 activation, Escape
clearing, and selection-following scroll now share a native C/Go contract.
Generated k2c and k2go fixtures exercise the same movement and activation, and
native popup tests prove that a focused table cannot consume keys owned by a
higher popup branch.
Cell, full-row, full-column, and override copying plus paste target reporting
now run through that same owned key path. Native C tests cover every copy shape,
and the shared generated fixture proves cell-copy and paste parity in k2c
and k2go.
Separate native Tab tests now cover ownership-filtered focus destinations,
forward/reverse wrapping, duplicate registrations and traversal after explicit
child/branch dismissal. Go tests exercise previous-frame order and real editor
Tab events. C finalizes focus while the host registry is still bound. Separate
native lifecycle tests cover first-child acquisition and restoration after
nested close, root close and missing-owner retirement. Full keyboard routing
remains work.
Ordinary Go Button now supports focus registration, pointer focus, Enter/Space
activation and Tab traversal. C keyboard activation consults popup ownership for
both immediate returns and deferred click events. Matching native popup tests
cover blocked parent versus eligible child activation. Generated C/Go checks
extend `buttons_layout.kry` coverage with Enter/Space, disabled activation
rejection and Tab skipping the disabled button; these additional assertions are
native-only and do not claim JavaScript keyboard coverage.
The same focusable-activation contract now backs native C and Go `Toggle`, `Checkbox`,
`CheckboxFlags`, `Radio`, `Selectable`, `InvisibleButton`, `ImageButton`, and
`ColorButton`, instead of duplicating key handling in each paint routine.
Direct runtime tests cover Enter/Space, Tab, disabled scopes, focus presentation,
and top-popup ownership. The generated `basic_controls.kry` fixture exercises
the canonical checkbox and props-based choice widgets through k2c and k2go;
JavaScript remains outside these added keyboard assertions.
Native missing-owner tests additionally omit the child and then the entire
branch across frames. C now retires missing input ownership before filtering
focus, matching Go's frame-end behavior; Tab reaches the surviving parent or
background in the same frame. Popup lifecycle tests separately verify automatic
restoration without a navigation event.
Text replay tests now span dismissal: focused parent TextField/TextArea controls
reject captured text, remain unchanged on the next frame without input, and
accept fresh text later. C covers both injected and platform-queued characters,
same-frame dismissal, and queued Backspace/Enter for TextField. Popup-captured
leftovers now expire at frame end. Go's existing frame cleanup passes the matching
text test. Device-level Android IME delivery remains unverified.
C IME now has separate regressions: blocked commits do not replay,
read-only TextField/TextArea reject commits, and TextField preedit cancels on
focus loss, a read-only transition or popup capture without revival.
Native Go now implements runtime-local composition events and per-editor
preedit for TextField/TextArea. Tests cover queue isolation/limits, preedit
persistence without buffer mutation, UTF-8 commit, cancellation, editor removal,
focus loss, disabling and popup dismissal. `composition.kry` is executed through
generated C and Go to verify preedit/commit/cancel behavior; it is not executed
by the JavaScript runner. Retained C and native Go now build the visible preedit
through one UTF-8 composition-view primitive per runtime: it visually replaces
the committed selection without mutating the caller buffer, positions the caret
at the IME cursor, highlights the IME-selected subrange, and underlines the full
composition in both TextField and TextArea. C previously omitted TextArea
preedit entirely. The DOM and Android adapters now normalize the composition
cursor to the runtime's documented UTF-8 byte-offset contract, and retained C
editors preserve their declaration-time Kryon font through deferred painting.
Immediate and retained C editors now share one private composition session and
one commit operation; retained widget state no longer duplicates preedit text,
cursor or selection fields. Immediate TextField/TextArea tests cover UTF-8
preedit without caller-buffer mutation, commits, selection replacement and
read-only cancellation.
The pure-Go Linux/X11 window backend now connects directly to the desktop's
IBus daemon without cgo. Each window owns one input context; X11 press/release
and focus events pass through it before the ordinary keysym fallback, and
CommitText/UpdatePreeditText signals feed the existing runtime-local
composition queue. Cursor positions are converted from IBus codepoints to the
runtime's UTF-8 byte offsets, and candidate placement follows the focused
TextField/TextArea caret. A fake-daemon protocol test covers context setup,
key forwarding and Unicode preedit/commit, while an opt-in test verifies the
real desktop daemon. Other Linux input-method protocols and device-level
Android/Win32 IME delivery remain unverified.
Read-only TextField/TextArea behavior is now covered by the native generated
composition fixture: selection/copy remains usable while text, cut/paste,
deletion and IME commits cannot mutate buffers. C retained mutation paths now
enforce the property; Go text props expose ReadOnly and retain focus styling
while suppressing the insertion caret. Go tests cover preedit cancellation,
buffer preservation, rejected-input expiry and fresh editing after re-enabling.
Multiline TextArea navigation now handles Up/Down and PageUp/PageDown in native
C and Go, with page movement derived from the visible editor height. The C
immediate and retained paths route every navigation key through one internal
selection-navigation routine, including the shared line/page movement helpers,
so generated code cannot drift from the direct widget behavior. Enter inserts
a newline in Go TextArea instead of following the single-line commit path.
Direct runtime tests and the generated composition fixture exercise these
behaviors through k2c and k2go. Shift extends selections across horizontal,
vertical, page and Home/End navigation; an unmodified arrow collapses an active
selection toward that edge. Multiline Home/End target the current line, while
the platform modifier plus Home/End targets the whole buffer. Modifier plus
Left/Right now uses Dear ImGui-compatible UTF-8 word boundaries, including
punctuation separators and full-width blanks, and Shift extends those word
selections. Modifier plus Backspace/Delete uses the same boundary helpers and
the same selection-first deletion primitive in immediate C, retained C, and
Go. The generated composition fixture covers word movement, selection, and
deletion through k2c and k2go. Native Go's X11 input path also preserves Shift
on special keys and routes Ctrl+Arrow/Backspace/Delete as modifier shortcuts.

Native C and Go list boxes now register with ordinary Tab focus, expose a
visible focus presentation, and move selection with Up/Down/Home/End while
keeping the selected row visible. Disabled and higher-popup-owned lists reject
the same keys. The shared generated list fixture exercises the navigation
contract through k2c and k2go without adding backend-specific widget aliases.

Native C and Go menus share one navigation model across menu bars, standalone
popup menus, context menus, and recursive submenus. Arrow, Home, End,
Enter/Space, and Escape navigation skips separators and disabled commands,
respects the active popup owner, and does not let passive pointer hover steal
keyboard focus. The generated menu fixture exercises nested activation through
k2c and k2go. C owns a recursive snapshot of deferred overlay items, so
generated frame-local menu arrays remain valid through end-of-frame painting.

`tests/parity/drag_drop.kry` is executed through generated C and Go. A clipped
source cannot activate, and a clipped target cannot consume the release before
an eligible target. Both runners mutate the source bytes after pressing and
check that the target receives the original copied payload. This native-only
fixture is not executed by the JavaScript runner. Go runtime tests additionally
exercise drag sources and targets beneath a popup, within its input scope, and
under disabled/clip restrictions, including rejection without losing the release
or payload. Separate native C/Go tests cover active drag values, sliders,
splitters and table resizing: an owned gesture follows the pointer beyond its
original bounds, a newly opened popup preempts a background gesture, and popup
dismissal cancels the gesture before a background widget can inherit it.
The composed-popup parity fixture drives that ownership lifecycle through
generated C and native Go, while k2cpp compiles the same `.kry` source against
the C runtime surface.

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
inheriting the owner's scroll clip. The separate composed-combo fixture covers
arbitrary native children. Native generated dismissal checks cover Escape and outside clicks,
unchanged selection, and restoration of underlying button interaction. Go also
tests closing when the option list becomes empty.
Generated C/Go tests also check that an earlier background scroll scope does
not change its offset on wheel input over an open combo popup. Native Go unit
tests cover the same ownership rule for scroll scopes, lists, trees, source
views and tables, including disabled and clipped content.

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

Native C and Go now expose the missing composed tab contract through
`BeginTabBar`, `BeginTabItem`, `EndTabItem`, and `EndTabBar`. The bar delegates
all header interaction and painting to the canonical `TabBar`; the scope only
controls arbitrary child submission, so there is no second tab renderer.
Selection changes are written to caller state before item scopes are evaluated
in the same frame. Direct C and Go tests plus generated C, Go, and JavaScript
tests switch tabs by keyboard, activate overlapping child buttons, and verify
that only the selected item's child can receive input. Invalid bars do not open
a scope, and unbalanced item or bar endings are rejected.
