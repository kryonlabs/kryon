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

The optional sync-node pool is local-first: paired LAN nodes rank ahead of
paired remote nodes, and a configured public node is the final fallback. Each
node has isolated bearer-token and clock-skew storage, so failing over cannot
send one node's credentials to another. Connectivity and authentication errors
advance to the next node; account, signing, and payload errors stop immediately.
Discovery may add candidates, but only explicit pairing grants trusted status.

## Runtime Implementation

Portable widget policy lives in `runtime/` as `.kry` source. C runtime code in
`src/` still contains widget orchestration alongside rendering helpers, platform
adapters, serialization, sync/update primitives, and backend-specific translation.
The native Go runtime lives in `go/kryon/`. Internal host helpers should remain
private unless an unrelated downstream application demonstrably needs them.

### Widget lifecycle consolidation

#### Canonical `.kry` widget declarations — migration target

Declared records have portable typed initializers, for example
`(Props){.value = 5, .appearance = (Appearance){.inset = 7}}`.
Positional initialization such as `(Props){5}` is also supported; named and
positional fields cannot be mixed. Omitted fields are zero-initialized, and
explicit field expressions run once in source order. Imported records, nested
records, and passing initializers directly to declared widgets use the same
value-copy semantics across C, C++, Go, and JavaScript. Strict checking rejects
unknown, duplicate, excessive, or incorrectly typed fields. This applies to
declared `.kry` records, not arbitrary host-language aggregate syntax.

Portable emission captures each call argument once, in source order. Reading
a nested field captures the field value directly instead of copying every
enclosing record. Record arguments still have independent value semantics,
including when a later argument mutates the original record.

Portable policy records now support immutable UTF-8 `string` values, including
empty initialization, field assignment, copying, parameters, returns,
conditionals, and content equality (`==` / `!=`). Literals preserve embedded
nulls and normalize escapes across C, C++, Go, and JavaScript. Invalid UTF-8,
invalid Unicode scalar escapes, arithmetic, ordering, and numeric casts are
diagnosed rather than emitted as host-dependent operations. String creation,
concatenation, slicing, and a general ownership model are not implemented by
this change; this is not a claim of language completeness.

C/C++ generated headers expose `String { data, length }`, `StringView`, and
`StringEqual`; views borrow immutable storage which the caller must keep alive.
Zero-length views may have a null data pointer. Nonempty views must address
their declared number of valid UTF-8 bytes. Go and JavaScript use native string
values. Portable calls do not silently convert these views to null-terminated
foreign parameters. `runtime/style.kry` uses this support for typeface names;
native style adapters bridge registered, null-terminated host names.

This is the intended ownership boundary, not a claim that the migration is
complete. The parser currently recognizes built-in widget names and props
types in `cmd/kir/kir_parse.c`; native Go also carries a props-field mapping in
`cmd/k2go/k2go_lower.c`. Shared functions in `runtime/button.kry`,
`runtime/text.kry`, `runtime/style.kry`, and `runtime/surface.kry` remove some
duplicated policy, but do not yet constitute complete widget declarations.

Go's public Button tone, emphasis, state, and control-size constants take their
values from generated `.kry` enums, while retaining their public Go types. The
size enum and `SizeValue` selector belong to `runtime/style.kry`, so measurement
and future widgets can use them without depending on button policy. C's public
enum declarations remain separate ABI declarations; parity checks verify every
member against `.kry`. Moving those public type declarations into the shared
source is still pending.

`runtime/style.kry` owns `StyleData` and the nested `StyleStates` record used
by state resolution. C and Go convert their public `ControlStyle` values into
that record; state selection and merging remain in `.kry`. Button's
`ResolveAppearance` selects one effective state for both its defaults and
overrides, so hosts no longer assemble that resolution sequence. Likewise,
`MeasureContent` in `runtime/button.kry` supplies both natural measurement and
content placement with the same fitted icon size and gap rules. These are
shared data and policy building blocks, not a substitute for declaration
resolution or per-instance widget state.

`ResolveInteraction` returns the effective control state together with its
hover, press, and focus signals. C and Go consume this shared result for button
appearance and motion instead of assembling state precedence and explicit
preview behavior independently. Input collection and instance storage remain
host responsibilities; this is not yet a per-instance `.kry` lifecycle.

`runtime/surface.kry` separates the inward material volume from dark focus
edge bloom. The latter uses a finite blurred stroke with an `outside_only`
coverage mask, leaving the face and label center untouched. Both hosts execute
the same mask and focus track; light themes and disabled controls suppress this
layer, and an explicit transparent focus color remains transparent.
Tall saturated faces in light surroundings gather a lifted chromatic lower
reflection inside the face. Height and chroma blend its strength continuously;
pressure attenuates it on the shared motion track. Pale and compact surfaces
retain their restrained reflection, without an added external fog layer.
The layer record also identifies the material face with `is_face`. C and Go
apply custom fill styles to that role, not a hard-coded position in the layer
sequence; the `.kry` material remains authoritative when layers are reordered.

Ordinary function resolution is shared in KIR: local functions shadow imports,
only public functions of explicitly imported modules are visible, and competing
imports are diagnosed as ambiguous. C, C++, Go, and JavaScript lowering use the
same resolved owner instead of searching a process-wide function-name table.
Tests execute same-named providers in both input-file orders. Stateless custom
blocks now resolve through that same lookup to `#ui` functions with one
record parameter. KIR lowers their props to an ordinary typed record initializer
and call before backend lowering. Blocks and record-valued calls share field
validation, omitted-field defaults, and value-copy semantics; there is no
separate per-field widget assignment path. A local holds the initialized props
before the call so mixed host-control-flow bodies preserve evaluation order.
Leaf blocks with built-in names participate in that same lookup: an explicit
local or imported `Button` or `Text` declaration owns its props type. Host
props are a fallback only when no declaration resolves; an invalid declaration
is an error, not a reason to silently select the host widget. Composed scope
blocks still use the built-in parser path and need migration with child slots.
Unknown, duplicate, and incorrectly typed props are errors even without strict
mode. Tests execute local and imported declarations in both source orders in
C, C++, Go, and JavaScript, including ordinary calls and record value semantics.
Declarations may return an action result. A block invocation discards that
result, like a call used as a statement; an ordinary expression call can consume
it. Cross-target tests exercise true and false results, discarded results,
props copying, and imported declaration order without a second widget entry point.
JavaScript's direct action-call lowering also uses the widget runtime when a
result is initialized, assigned, or returned, rather than constructing an inert
description object. Button tests cover activation and evaluated lexical bounds
in each form. Arbitrary nested action-call expressions still need migration.
Ordinary calls to resolved `#ui` declarations also reject known argument-type
and argument-count mismatches without strict mode. Using function syntax does
not bypass the declaration's signature. Unresolved host-header APIs do not yet
provide that declaration metadata to KIR; their migration remains necessary.
Braced `case` and `default` labels remain control flow, not widget declarations;
cross-target tests execute both switch paths around ordinary widget calls.
Child slots are still unsupported; child content is rejected explicitly.
Explicit-key instance bindings are described below. Built-in widget migration
is still pending.
Portable `#ui` bodies use the same checked emitter as ordinary functions;
the annotation does not force backend-specific arithmetic or record handling.
Strict tests execute narrow-integer overflow inside a declared widget through
both block and ordinary-call syntax in C, C++, Go, and JavaScript. Bodies with
unsupported host operations still require the existing backend path.
The built-in path also accepts `SplitButton` and `MenuButton` blocks. The
Lightfield example uses those blocks with explicit IDs and shared Button props.
Its display Buttons also use named blocks. Childless Button blocks lower to
the ordinary `Button` call; only blocks with child content open a
`BeginButton`/`End` scope. Explicit IDs remain necessary for stable identity;
the block name does not yet supply instance identity.
JavaScript diagnostics record their evaluated nested props and geometry; menu
interaction and material raster parity in that host are not implemented by
this registration.
The indexed open-state references retain their backing state-array elements.
Uninitialized state arrays receive independent zero-initialized elements,
including nested arrays and declared records, instead of scalar placeholders.
The generated-runtime parity fixture also composes actual themed Buttons
inside two instances of a props-taking declaration, with distinct IDs and
action amounts. C, Go, and JavaScript check that activation remains independent
after declaration order reverses; native keyboard checks remain enabled.
This covers explicit stable IDs, not automatic instance-key scoping.
JavaScript emits evaluated Button initializer objects so lexical props reach
the runtime as values, rather than unevaluated source text. Its automatic frame
selection accepts zero-argument screens or a single host `Rectangle` viewport;
other props-taking helpers are not screen entry points. The JavaScript host
supplies current target dimensions, falling back to application dimensions for
headless or zero-sized targets. Local initializer declarations and assignments
now use the same evaluated initializer lowering as Button props. Named fields
become objects and positional initializers become arrays. The actual Lightfield
source is checked for finite Button geometry and representative font sizes.
The check clicks its actual Light/Dark controls and verifies light-to-dark-to-light
switching, including the per-theme geometry and icon-button emphasis.
JavaScript Style presence-bit bindings are checked against the shared `.kry`
merge policy, including zero-valued overrides. This is not browser visual
parity or complete typed-record lowering: remaining host enum coverage,
positional record field access, other widget argument lowering, and rendering
still need migration.

Strict portable emission supports declared `.kry` enum values in function
parameters, returns, locals, conditionals, and record fields, including imported
types. Values use signed 32-bit storage: C emits an `int32_t` typedef, C++ a
fixed-underlying-type enum, Go a named `int32` type, and JavaScript checked
numeric values. Explicit numeric casts use the shared integer conversion rules;
zero initialization and record copies preserve enum fields. The shared checker
rejects implicit integer-to-enum and cross-enum assignments, record-to-enum
casts, and enum compound arithmetic. Enum member constants retain their existing
integer expression behavior; use an explicit enum cast when assigning them to
a named enum property. `tests/imported_cast_test.py` executes this contract in
strict mode on all four targets.

JavaScript fallback expressions resolve casts to declared `.kry` enums through
the shared type lookup, including runtime-call arguments. Numeric operands are
truncated toward zero. This does not supply metadata for host-only C enums
such as `ThemeMode`; those declarations still need migration before the theme
catalog's C-style casts can execute in JavaScript.

Each widget must have one canonical declaration in `.kry`. That declaration
owns its typed props and defaults, per-instance state, events, measurement and
layout policy, child composition, and appearance. Generated C and native Go
must consume the same declaration. Adding an application-defined widget must
not require editing compiler name lists, backend props tables, or handwritten
widget implementations.

Button size is a prop, not another widget: compact callers use `Button` with
`ControlSizeSmall`. The separate small-button entry points have been removed
from C, native Go, and the compiler's built-in call registry.

Shared abstractions beneath declarations have separate responsibilities:

- `Style` describes material and typography, with explicit field presence so
  transparent colors and zero dimensions are not mistaken for missing values.
- Surface and motion primitives describe reusable drawing and transitions;
  there is no separate public button-paint abstraction.
- Text measurement and text editing are distinct services. TextField and
  TextArea remain separate widgets but share editing, selection, and composition
  behavior rather than duplicating an editor.
- Hosts provide font shaping/rasterization, drawing, input delivery, clipboard,
  IME, time, and storage. Widget policy decides how these services are used.

The compiler must represent declarations and typed child slots in KIR before
backend lowering. Declaration resolution must work across modules and source
order, with diagnostics for unknown props, invalid event/slot types, duplicate
declarations, and invalid state access. A block invocation and a function-style
invocation must resolve to the same widget; a second implementation or alias
does not satisfy this requirement.

State belongs to a stable widget instance within a window, not to a declaration
global. Reordering keyed children must preserve their state; separate windows
and separate instances must not share interaction or animation tracks.

`runtime/button.kry` declares `ButtonInstance`, including its motion tracks.
C's host-owned `ToolkitStore` and Go's runtime allocate typed instance records
by a 64-bit key. Types and render hosts have separate namespaces; inserting or
reordering other instances preserves existing records and their addresses.
Existing C frame bindings select the store for native windows, nested rendering
hosts, and headless UI frames. Closing a host releases its instance storage.
`runtime/instance.kry` supplies the shared twelve-frame retention rule. Each
host sweeps its own store even when no buttons are drawn; another host's frames
cannot expire its entries. Button uses this storage in both native runtimes.
The shared compiler lowers `name: Record #instance(key)` to a borrowed typed
record in C, C++, Go, and JavaScript. Keys evaluate once; ordinary copies retain
value semantics, while field and whole-record assignments update the retained
value. JavaScript uses the generated `instance.kry` expiration policy too.
Integration tests execute custom blocks and ordinary calls against all four
real hosts, including type isolation, reordered and wide keys, whole-record
replacement, lexical shadowing, and expiration. Instance bodies must pass the
shared portable checker, even without `--strict`; a fallback must not turn an
instance binding into a local or module global. Hierarchical keys and typed
child slots remain necessary before this is a complete declaration lifecycle.

With `k2go --runtime-implementation`, functions that access instances
are methods on their owning runtime. The checker propagates this requirement
through calls, so composed helpers preserve their receiver without consulting
another window's active host. Application declarations use the active host's
generic instance service. Button's retained motion update now lives in `.kry`
and uses this binding; C and Go no longer look up or mutate its stored tracks.

Runtime generation discovers `runtime/*.kry` as one checked module set. Adding
a shared module does not require another per-widget C or Go generation rule.

Migrate Button end-to-end as the first composition/state test, followed by Text.
Do not call this complete merely because a new declaration parses: tests must
prove user-defined widgets compile without name-table changes, generated C/Go
props agree, two instances remain independent, children inherit and clip
correctly, and existing input, transparency, theme, and motion behavior remains
covered. Remove each old implementation only after its maintained callers have
migrated. TextField/TextArea implementation follows this foundation; visual
proposals are not evidence that their declarations or editing behavior exist.

#### Current retained runtime

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
keyboard routing remains unfinished. Generic accelerator dispatch is isolated
to the top live popup branch in C and Go, including immediate restoration when
an explicitly closed popup is next declared.
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
