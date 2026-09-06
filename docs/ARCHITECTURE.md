# Kryon Architecture

Kryon is organized as a reusable runtime with a small set of public headers,
backend implementations, format/tooling paths, and conformance tests. The
runtime should expose primitives; downstream applications compose those
primitives into product behavior.

## Public API

Public headers live in `include/`. They define the app-facing Kryon surface:
window/runtime compatibility, UI widgets, platform services, formats, update,
sync, filesystem, desktop integration, and optional terminal primitives.

Public APIs should be named after the real domain concept. Do not add temporary
prefixes, compatibility aliases, or product-flavored names for new behavior.

## Runtime Implementation

Runtime code lives in `src/`. It owns widget behavior, rendering helpers,
platform adapters, serialization, sync/update primitives, and backend-specific
translation. Internal helpers should stay private to `src/` unless an unrelated
downstream application demonstrably needs the behavior.

### Widget lifecycle consolidation

Captured C retained paint includes blend state through the private
`ui_blend_internal.h` snapshot. OpenGL 3.3/GLES2 backend hooks preserve both
effective GPU factors/equations and rlgl's cached mode/custom configuration,
including changes awaiting activation. The hooks are injected into the build
copy by `prepare-raylib-source.sh`, not into the raylib submodule or public API.
Each captured node restores its declaration state for painting and restores the
caller's state afterward. The snapshot itself does not own render resources.

The private `ui_paint_layers.c` context now owns temporary C render textures,
collects mixed immediate/retained layers, and composites them in opening order.
Nested layers remain above subsequent parent paint; hidden parents suppress
descendants. Capture and composition restore drawing state, and texture resource
operations preserve the active framebuffer. Native `UIWindow` hosts lazily own
their context and handle its frame, composition and destruction. The private
active-window accessor exposes that owner to future composed widgets without
adding app-level lifecycle calls. Main UI frames own a separate lazy context:
`SetUIFrame` starts it, `EndUIFrame` composites it, and `CloseWindow` releases
its resources before graphics shutdown. The private frame accessor routes to
the active UIWindow when appropriate. Presenter readback preserves the calling
framebuffer. Layer scopes suspend the retained Row/Column path, keeping popup
children in the screen tree without consuming the owner's layout slots. Scope
exit checks balanced child layouts and restores the owner's path. Disabled
scopes likewise inherit the owner's state behind a protected local depth floor,
then restore the parent's scope on exit. Input clips and scroll depth are
isolated similarly, while modal capture stays active. The private C popup input
registry supplies nested branch capture to the shared hit-test path. Each paint
host owns its registry and advances, binds, retires and destroys it automatically.
Auxiliary frames switch to their own registry and restore the parent's binding
on completion; hiding a layer closes its input branch. Retained nodes snapshot
popup ownership for deferred hit testing and button hover/press state; snapshots
are kept with the committed tree while a replacement is declared. Closed,
missing, previous-frame or destroyed owners cannot receive those hits.
Deferred pointer-focus registration uses the same ownership snapshots without
reopening scopes, preserving modal, clip, disabled and inspection capture.
Retained hit testing and button hover/press state now use that same full capture
predicate, so modal blocking also prevents deferred click events.
The public `BeginCombo` / `EndCombo` / `CloseCombo` and `BeginPopup` /
`EndPopup` / `ClosePopup` scopes bind these paint, layout and input contexts for
arbitrary native children. `PopupTooltip` reuses that paint/layout scope while
intentionally skipping input ownership. `PopupModal` uses the same scope with a
full-view input/backdrop policy, and `PopupContext` uses it with right-release
activation over a retained trigger. Presentation variants remain flags on the
one popup implementation rather than parallel widget trees.
Modal outside releases remain non-dismissing. The input registry saves focus
when a top popup first opens, focuses its first eligible child, and restores the
parent or background after nested close, root close, or missing-owner retirement.
Long-lived drag values, sliders, splitters and table resizers store the same
persistent owner identity, allowing out-of-bounds continuation only while that
branch remains topmost and cancelling on dismissal or ownership changes.
C and Go now have a pointer-independent top-popup keyboard predicate. Closed
combos use it before keyboard opening, preventing a focused parent/background
combo from opening behind a child popup. Focus registrations now retain their
popup owner: C filters and deduplicates Tab destinations at focus finalization,
before releasing the host input binding; Go filters its current/previous-frame
focus order during traversal. Native tests cover forward/reverse wraparound,
parent/background exclusion and traversal after explicit dismissal. Go also
exercises actual editor Tab events. Native lifecycle tests cover automatic
first-child focus acquisition and parent/background restoration. Complete
keyboard routing remains unfinished.
TextField/TextArea editing now also respects top-popup keyboard ownership in
both runtimes. C's immediate keyboard-enabled check uses the active scope;
retained editor routing uses its declaration snapshot after scopes close.
Tests cover blocked parent and eligible child typing, including immediate and
retained C paths. Immediate C TextArea skips painting without a graphics window.
Ordinary Go Button now registers focus, focuses on pointer activation and
handles Enter/Space and Tab. C's activation check also consults registered popup
ownership, including deferred button events. Unclaimed Go Tab events route at
frame end so a disabled, absent or blocked focus owner cannot swallow traversal.
Generated C/Go button tests cover Enter/Space, disabled rejection and Tab skipping
a disabled control; popup tests cover parent/child keyboard activation.
Missing C popup input owners are now retired before frame-end focus filtering,
not only during paint cleanup. Matching C/Go tests omit a child and then its
parent and verify that Tab reaches the surviving parent/background that frame.
Popup keyboard capture is remembered for the C host frame. Unhandled character
input and queued text-edit commands expire at frame end after such capture,
including same-frame dismissal, instead of leaking into an underlying editor
later. Tests cover injected/platform-queued text, queued Backspace/Enter and
fresh input after dismissal. Non-popup queue behavior is unchanged.
C retained IME commits now respect read-only editors, and popup-captured
composition events expire with other blocked text input. Preedit is cancelled
on focus loss, read-only transition or popup keyboard capture. Regression tests
cover cancellation and no commit replay after dismissal. Native Go now owns a
composition queue and per-editor preedit records. TextField/TextArea display
preedit separately from committed buffers and insert UTF-8 commits; focus loss,
removal, disabling and popup capture discard preedit. A native generated
composition fixture verifies non-mutating preedit, commit and cancellation in
C and Go. Device-level Go IME routing remains unfinished.
Native Go text props now carry ReadOnly, with mutation guards separate from
focus/selection/copy handling and read-only caret metadata. C retained editing
also guards ordinary typing, cut/paste and deletion, not just composition
commits. The generated composition fixture verifies read-only TextField and
TextArea buffers and copying in both native runtimes; Go tests cover preedit
cancellation, byte-for-byte buffer preservation and re-enabling without replay.
Native Go runtime creation now resolves Kryon's Noto Sans UI face from packaged,
development-tree, or standard system locations before falling back to the
minimal bitmap renderer. This matches the C host's default-font policy while
preserving explicit `RegisterUIFontData` / `UseUIFont` overrides.
Private paint scopes share a drawing-order guard across host contexts while
keeping texture ownership host-local. Token generations survive host destruction
and allocation reuse. Invalid closes are rejected before restoring drawing state.

C dropdown identity records now have stable dynamically allocated storage,
rather than a fixed 24-control array whose overflow reused the first control.
Frame-end overlay processing retires records for owners no longer declared.
C and Go tests declare 41 controls, open/select/reopen the first, and remove it
while checking independent selections and capture release. C dropdown storage
is still process-global; per-window ownership remains to be consolidated.
Options now use dynamically sized owned records and strings, removing the
128-option, 255-byte label and 31-byte font-name truncation limits. Unchanged
strings reuse their allocations; shrinking lists and retired controls release
their owned storage. Overlay painting visits visible rows only, and content
height saturates at the runtime's integer coordinate limit.

The private C `ui_scrollbar` helper handles interaction independently of window
painting. Dropdown overlays use base input capture so their own popup capture
does not block the thumb, while ordinary scrollbars retain full popup capture.
Dismissal and owner retirement cancel a dropdown's active scrollbar before its
offset storage is released. Scrollbar drags are distinct from popup content
drags and cannot select an option on release. The shared C scrollbar drag slot
is still process-global; this does not complete per-window input ownership.
Dropdowns process and paint the scrollbar after the panel background but before
emitting rows, so both use the updated offset in the drag frame. Row painting
is clipped to the content area, excluding popup padding and the scrollbar.
The C popup bounds helper and Go dropdown layout constrain horizontal placement
and width to the UI view. Hit testing, capture and painting share the popup
rectangle rather than assuming that its horizontal bounds match the owner.

The shared KIR parser lowers `.kry` `Disabled { when = condition ... }` blocks
to existing runtime calls and lexical cleanup. The same cleanup pass used for
`defer` closes the scope on normal exit, return, break and continue before any
backend emits code. The generated buttons parity fixture exercises those exits
and nested disabled inheritance in C, Go and JS; C++ syntax coverage checks the
same shared lowering. Native `Scroll` blocks use the same lexical cleanup and
existing `BeginScroll`/`EndScroll` operations. An optional block name binds the
returned content rectangle without leaking it beyond the block. Native generated
scroll parity covers nested clips/input and restoration after return, break and
continue. Native `Combo` and `Popup` blocks similarly lower caller-defined popup
contents to conditional begin calls plus cleanup-managed end calls, removing manual scope
bookkeeping from `.kry` sources while retaining the small runtime contract.
Other begin/end APIs have not all gained block syntax.

The C widget implementation is still being consolidated. Some constructors
submit retained paint, while others register generic nodes and draw immediately.
Do not treat the presence of a tree node as proof that a widget participates in
deferred painting.

`Button`, `SliderFloat`, `SliderInt`, `VSliderFloat`, `VSliderInt`, and `SliderAngle` resolve
their declaration bounds before handling input and submit typed paint data when
building a tree. The slider painters do not process input. Labels and formats
are copied into node-owned storage; value arrays remain caller-owned. Without
a tree, the same input and paint helpers run immediately. This preserves the
public API while removing the separate prefixed slider draw entry points.
Angle sliders retain the caller's radians pointer and convert to degrees only
within input or paint execution; no pointer to a temporary conversion survives
declaration.

`DragFloat` and `DragInt` follow the same split, including owned label/format
storage and resolved bounds. `DragFloatRange2` and `DragIntRange2` compose two
ordinary typed drag nodes inside a Row, plus retained Text for the label. Their
children preserve the existing input IDs and borrow the caller's endpoint
pointers; no range-specific renderer or prefixed drag draw entry point remains.

`Text(TextProps)` uses one typed retained text node with owned text, captured
font selection, and resolved bounds. Its painter measures intrinsic lines or
wraps bounded text, clips to positive bounds, and applies alignment, color, and
disabled presentation from the same property object. Headless declaration does
not invoke a graphics backend. There are no separate public colored, disabled,
wrapped, or in-rectangle text widget implementations.

The remaining families must migrate with evidence for layout, input timing,
paint order, disabled/clip scopes, and data lifetime. Captured render textures
remain private implementation machinery behind the public combo scope, not a
low-level public paint API or the completed widget architecture. Native Go keeps
its own implementation and must retain matching behavior through
generated-runtime parity coverage.

Numeric editor state uses collision chains keyed by numeric type, widget ID,
and component index,
with stable record addresses. A bucket is not a single replaceable editor slot:
distinct keys must preserve independent text, cursor, and focus. C and Go tests
cover 129 keys that share the C bucket index. Each component reserves three
separate numeric control IDs for its field, decrement, and increment controls;
vectors larger than 16 components do not alias neighboring numeric widgets.
Tests cover 396 component keys and the emitted IDs for a 17-component input
beside another input. C's process-wide numeric state still needs
per-window lifetime integration as numeric widgets are consolidated.
Numeric ID allocation is not yet a shared identity service for every widget
family or arbitrary application-supplied control IDs.

Numeric input wrappers resolve their bounds before invoking the editor and do
not retain pointers to stack-local props. Their increment/decrement controls
use standard `Button` submissions, so step handling is independent of graphics
availability and participates in retained painting and disabled scopes. C and
Go tests cover normal, fast, and disabled stepping inside Row layout. Numeric
text editing also runs without a graphics window and returns changes during
declaration. Its painter receives an owned display-text snapshot plus caret,
selection, scroll, style, and font information; retained painting does not
process editing input again. Disabled typing is discarded, not replayed on
re-enabling the field. The ordinary retained TextField input-routing path and
numeric editor input path are still separate; consolidation is not complete.

Native Go collects deferred popup paint in `go/kryon/paint_layers.go`, using
ordinary frame operations rather than dropdown-specific drawing records.
Dropdown placement lives in `dropdown_layout.go`; capture and painting share
the resulting constrained, optionally upward-facing rectangle. Popup rows use
the ordinary scroll container, with runtime-owned stable offset storage that
is released on dismissal or owner removal. Keyboard navigation reveals rows
without overriding wheel scrolling on idle frames; only visible rows are emitted.
Nested layers paint above their parents and restore the surrounding layout,
clip and disabled state. Existing dropdowns use this collector and the private
`popup_input.go` registry for scoped click ownership. That registry tracks
parent/child and sibling order, preserves capture before owner declaration in
the following frame, and removes descendants on closure or owner removal.
Native Go scroll scopes, lists, trees, source views and tables share a pointer
reachability check for wheel input, respecting popup ownership, disabled state
and parent clips. Generated C/Go tests verify that a background scroll scope
declared before an open combo cannot steal its wheel input.
Go drag-and-drop sources and targets also use that pointer check for starting
and accepting a drag; an already active source retains its payload when the
pointer leaves its original bounds. Rejected targets do not consume the release
or payload. Generated C/Go tests cover clipped sources/targets and copied data
lifetime across press and release.
The public arbitrary-content combo scope integrates these paint and input
registries in C and Go. Generated C/Go execution and k2cpp syntax coverage use
the same clean calls. Popup focus acquisition/restoration and active scalar,
slider, splitter and table-resize ownership are covered in both native runtimes;
device-level integration remains unfinished.

## Backends

Backend selection is controlled by `KRYON_BACKEND`. The default native backend
uses raylib through Kryon's compatibility surface. Other backends are expected
to implement Kryon behavior through the same public/runtime contracts, with
their support documented in `docs/BACKEND_CAPABILITIES.json`,
`docs/BACKENDS.md`, and `docs/FEATURE_MATRIX.md`.

## Formats And Tools

The `.kry`, KIR, generated C, generated Go, and KRB paths are Kryon-owned
tooling surfaces. Tool changes should update the matching specs, conformance
matrix data, examples or fixtures, and generated documentation when applicable.

## Examples And Fixtures

Examples under `examples/` should remain generic and readable. They demonstrate
Kryon capabilities, not downstream product screens. Exact rendering and parity
fixtures should be generic enough that they can run in any app-independent
Kryon build.

## Tests And Matrices

Kryon uses several test layers:

- boundary and naming checks for repository hygiene
- public API snapshot checks for app-facing identifier drift
- public header compile checks for app-facing include hygiene
- examples manifest checks for example inventory and exactness fixtures
- backend capability checks for backend inventory drift
- generated-file checks for docs, compatibility headers, icons, and matrices
- parser, runtime, sync, update, platform, and widget tests
- conformance and visual matrix checks across renderers and runtime paths

Use `make preflight` before committing focused Kryon changes. Use `make test`
for the broader local regression suite. Use `make test-asan` or
`make test-ubsan` for sanitizer-backed regression runs, and
`make release-preflight` before publishing release artifacts.

## Downstream Integration

Downstream applications vendor Kryon as a submodule. Permanent Kryon changes
must be made and committed in this repository first, then brought into apps by
updating the submodule pointer. Never edit a downstream `vendor/kryon` tree as
the source of a Kryon change.
